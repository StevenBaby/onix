#include <onix/ide.h>
#include <onix/io.h>
#include <onix/printk.h>
#include <onix/stdio.h>
#include <onix/memory.h>
#include <onix/interrupt.h>
#include <onix/task.h>
#include <onix/string.h>
#include <onix/net/types.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/device.h>
#include <onix/timer.h>
#include <onix/errno.h>
#include <onix/pci.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define IDE_TIMEOUT 60000

// IDE 寄存器基址
#define IDE_IOBASE_PRIMARY 0x1F0   // 主通道基地址
#define IDE_IOBASE_SECONDARY 0x170 // 从通道基地址

// IDE 寄存器偏移
#define IDE_DATA 0x0000       // 数据寄存器
#define IDE_ERR 0x0001        // 错误寄存器
#define IDE_FEATURE 0x0001    // 功能寄存器
#define IDE_SECTOR 0x0002     // 扇区数量
#define IDE_LBA_LOW 0x0003    // LBA 低字节
#define IDE_CHS_SECTOR 0x0003 // CHS 扇区位置
#define IDE_LBA_MID 0x0004    // LBA 中字节
#define IDE_CHS_CYL 0x0004    // CHS 柱面低字节
#define IDE_LBA_HIGH 0x0005   // LBA 高字节
#define IDE_CHS_CYH 0x0005    // CHS 柱面高字节
#define IDE_HDDEVSEL 0x0006   // 磁盘选择寄存器
#define IDE_STATUS 0x0007     // 状态寄存器
#define IDE_COMMAND 0x0007    // 命令寄存器
#define IDE_ALT_STATUS 0x0206 // 备用状态寄存器
#define IDE_CONTROL 0x0206    // 设备控制寄存器
#define IDE_DEVCTRL 0x0206    // 驱动器地址寄存器

// IDE 命令

#define IDE_CMD_READ 0x20       // 读命令
#define IDE_CMD_WRITE 0x30      // 写命令
#define IDE_CMD_IDENTIFY 0xEC   // 识别命令
#define IDE_CMD_DIAGNOSTIC 0x90 // 诊断命令

#define IDE_CMD_READ_UDMA 0xC8  // UDMA 读命令
#define IDE_CMD_WRITE_UDMA 0xCA // UDMA 写命令

#define IDE_CMD_PIDENTIFY 0xA1 // 识别 PACKET 命令
#define IDE_CMD_PACKET 0xA0    // PACKET 命令

// ATAPI 命令
#define IDE_ATAPI_CMD_REQUESTSENSE 0x03
#define IDE_ATAPI_CMD_READCAPICITY 0x25
#define IDE_ATAPI_CMD_READ10 0x28

#define IDE_ATAPI_FEATURE_PIO 0
#define IDE_ATAPI_FEATURE_DMA 1

// IDE 控制器状态寄存器
#define IDE_SR_NULL 0x00 // NULL
#define IDE_SR_ERR 0x01  // Error
#define IDE_SR_IDX 0x02  // Index
#define IDE_SR_CORR 0x04 // Corrected data
#define IDE_SR_DRQ 0x08  // Data request
#define IDE_SR_DSC 0x10  // Drive seek complete
#define IDE_SR_DWF 0x20  // Drive write fault
#define IDE_SR_DRDY 0x40 // Drive ready
#define IDE_SR_BSY 0x80  // Controller busy

// IDE 控制寄存器
#define IDE_CTRL_HD15 0x00 // Use 4 bits for head (not used, was 0x08)
#define IDE_CTRL_SRST 0x04 // Soft reset
#define IDE_CTRL_NIEN 0x02 // Disable interrupts

// IDE 错误寄存器
#define IDE_ER_AMNF 0x01  // Address mark not found
#define IDE_ER_TK0NF 0x02 // Track 0 not found
#define IDE_ER_ABRT 0x04  // Abort
#define IDE_ER_MCR 0x08   // Media change requested
#define IDE_ER_IDNF 0x10  // Sector id not found
#define IDE_ER_MC 0x20    // Media change
#define IDE_ER_UNC 0x40   // Uncorrectable data error
#define IDE_ER_BBK 0x80   // Bad block

#define IDE_LBA_MASTER 0b11100000 // 主盘 LBA
#define IDE_LBA_SLAVE 0b11110000  // 从盘 LBA
#define IDE_SEL_MASK 0b10110000   // CHS 模式 MASK

#define IDE_INTERFACE_UNKNOWN 0
#define IDE_INTERFACE_ATA 1
#define IDE_INTERFACE_ATAPI 2

// 总线主控寄存器偏移
#define BM_COMMAND_REG 0 // 命令寄存器偏移
#define BM_STATUS_REG 2  // 状态寄存器偏移
#define BM_PRD_ADDR 4    // PRD 地址寄存器偏移

// 总线主控命令寄存器
#define BM_CR_STOP 0x00  // 终止传输
#define BM_CR_START 0x01 // 开始传输
#define BM_CR_WRITE 0x00 // 主控写磁盘
#define BM_CR_READ 0x08  // 主控读磁盘

// 总线主控状态寄存器
#define BM_SR_ACT 0x01     // 激活
#define BM_SR_ERR 0x02     // 错误
#define BM_SR_INT 0x04     // 中断信号生成
#define BM_SR_DRV0 0x20    // 驱动器 0 可以使用 DMA 方式
#define BM_SR_DRV1 0x40    // 驱动器 1 可以使用 DMA 方式
#define BM_SR_SIMPLEX 0x80 // 仅单纯形操作

#define IDE_LAST_PRD 0x80000000 // 最后一个描述符

// 分区文件系统
// 参考 https://www.win.tue.nl/~aeb/partitions/partition_types-1.html
typedef enum PART_FS
{
    PART_FS_FAT12 = 1,    // FAT12
    PART_FS_EXTENDED = 5, // 扩展分区
    PART_FS_MINIX = 0x80, // minux
    PART_FS_LINUX = 0x83, // linux
} PART_FS;

typedef struct ide_params_t
{
    u16 config;                 // 0 General configuration bits
    u16 cylinders;              // 01 cylinders
    u16 RESERVED;               // 02
    u16 heads;                  // 03 heads
    u16 RESERVED[5 - 3];        // 05
    u16 sectors;                // 06 sectors per track
    u16 RESERVED[9 - 6];        // 09
    u8 serial[20];              // 10 ~ 19 序列号
    u16 RESERVED[22 - 19];      // 10 ~ 22
    u8 firmware[8];             // 23 ~ 26 固件版本
    u8 model[40];               // 27 ~ 46 模型数
    u8 drq_sectors;             // 47 扇区数量
    u8 RESERVED[3];             // 48
    u16 capabilities;           // 49 能力
    u16 RESERVED[59 - 49];      // 50 ~ 59
    u32 total_lba;              // 60 ~ 61
    u16 RESERVED;               // 62
    u16 mdma_mode;              // 63
    u8 RESERVED;                // 64
    u8 pio_mode;                // 64
    u16 RESERVED[79 - 64];      // 65 ~ 79 参见 ATA specification
    u16 major_version;          // 80 主版本
    u16 minor_version;          // 81 副版本
    u16 commmand_sets[87 - 81]; // 82 ~ 87 支持的命令集
    u16 RESERVED[118 - 87];     // 88 ~ 118
    u16 support_settings;       // 119
    u16 enable_settings;        // 120
    u16 RESERVED[221 - 120];    // 221
    u16 transport_major;        // 222
    u16 transport_minor;        // 223
    u16 RESERVED[254 - 223];    // 254
    u16 integrity;              // 校验和
} _packed ide_params_t;

ide_ctrl_t controllers[IDE_CTRL_NR];

static int ide_reset_controller(ide_ctrl_t *ctrl);

// 硬盘中断处理函数
static void ide_handler(int vector)
{
    send_eoi(vector); // 向中断控制器发送中断处理结束信号

    // 得到中断向量对应的控制器
    ide_ctrl_t *ctrl = &controllers[vector - IRQ_HARDDISK - 0x20];

    // 读取常规状态寄存器，表示中断处理结束
    u8 state = inb(ctrl->iobase + IDE_STATUS);
    LOGK("harddisk interrupt vector %d state 0x%x\n", vector, state);
    if (ctrl->waiter)
    {
        // 如果有进程阻塞，则取消阻塞
        task_unblock(ctrl->waiter, EOK);
        ctrl->waiter = NULL;
    }
}

static void ide_error(ide_ctrl_t *ctrl)
{
    u8 error = inb(ctrl->iobase + IDE_ERR);
    if (error & IDE_ER_BBK)
        LOGK("bad block\n");
    if (error & IDE_ER_UNC)
        LOGK("uncorrectable data\n");
    if (error & IDE_ER_MC)
        LOGK("media change\n");
    if (error & IDE_ER_IDNF)
        LOGK("id not found\n");
    if (error & IDE_ER_MCR)
        LOGK("media change requested\n");
    if (error & IDE_ER_ABRT)
        LOGK("abort\n");
    if (error & IDE_ER_TK0NF)
        LOGK("track 0 not found\n");
    if (error & IDE_ER_AMNF)
        LOGK("address mark not found\n");
}

// 硬盘延迟
static void ide_delay()
{
    task_sleep(25);
}

static err_t ide_busy_wait(ide_ctrl_t *ctrl, u8 mask, int timeout_ms)
{
    int expires = timer_expire_jiffies(timeout_ms);
    while (true)
    {
        // 超时
        if (timeout_ms > 0 && timer_is_expires(expires))
        {
            return -ETIME;
        }

        // 从备用状态寄存器中读状态
        u8 state = inb(ctrl->iobase + IDE_ALT_STATUS);
        if (state & IDE_SR_ERR) // 有错误
        {
            ide_error(ctrl);
            ide_reset_controller(ctrl);
            return -EIO;
        }
        if (state & IDE_SR_BSY) // 驱动器忙
        {
            ide_delay();
            continue;
        }
        if ((state & mask) == mask) // 等待的状态完成
            return EOK;
    }
}

// 重置硬盘控制器
static err_t ide_reset_controller(ide_ctrl_t *ctrl)
{
    outb(ctrl->iobase + IDE_CONTROL, IDE_CTRL_SRST);
    ide_delay();
    outb(ctrl->iobase + IDE_CONTROL, ctrl->control);
    return ide_busy_wait(ctrl, IDE_SR_NULL, IDE_TIMEOUT);
}

// 选择磁盘
static void ide_select_drive(ide_disk_t *disk)
{
    outb(disk->ctrl->iobase + IDE_HDDEVSEL, disk->selector);
    disk->ctrl->active = disk;
}

// 选择扇区
static void ide_select_sector(ide_disk_t *disk, u32 lba, u8 count)
{
    // 输出功能，可省略
    outb(disk->ctrl->iobase + IDE_FEATURE, 0);

    // 读写扇区数量
    outb(disk->ctrl->iobase + IDE_SECTOR, count);

    // LBA 低字节
    outb(disk->ctrl->iobase + IDE_LBA_LOW, lba & 0xff);
    // LBA 中字节
    outb(disk->ctrl->iobase + IDE_LBA_MID, (lba >> 8) & 0xff);
    // LBA 高字节
    outb(disk->ctrl->iobase + IDE_LBA_HIGH, (lba >> 16) & 0xff);

    // LBA 最高四位 + 磁盘选择
    outb(disk->ctrl->iobase + IDE_HDDEVSEL, ((lba >> 24) & 0xf) | disk->selector);
    disk->ctrl->active = disk;
}

// 从磁盘读取一个扇区到 buf
static void ide_pio_read_sector(ide_disk_t *disk, u16 *buf)
{
    for (size_t i = 0; i < (disk->sector_size / 2); i++)
    {
        buf[i] = inw(disk->ctrl->iobase + IDE_DATA);
    }
}

// 从 buf 写入一个扇区到磁盘
static void ide_pio_write_sector(ide_disk_t *disk, u16 *buf)
{
    for (size_t i = 0; i < (disk->sector_size / 2); i++)
    {
        outw(disk->ctrl->iobase + IDE_DATA, buf[i]);
    }
}

// 磁盘控制
int ide_pio_ioctl(ide_disk_t *disk, int cmd, void *args, int flags)
{
    switch (cmd)
    {
    case DEV_CMD_SECTOR_START:
        return 0;
    case DEV_CMD_SECTOR_COUNT:
        return disk->total_lba;
    case DEV_CMD_SECTOR_SIZE:
        return disk->sector_size;
    default:
        panic("device command %d can't recognize!!!", cmd);
        break;
    }
}

// PIO 方式读取磁盘
int ide_pio_read(ide_disk_t *disk, void *buf, u8 count, idx_t lba)
{
    assert(count > 0);
    assert(!get_interrupt_state()); // 异步方式，调用该函数时不许中断

    ide_ctrl_t *ctrl = disk->ctrl;

    lock_acquire(&ctrl->lock);

    int ret = -EIO;

    // 选择磁盘
    ide_select_drive(disk);

    // 等待就绪
    if ((ret = ide_busy_wait(ctrl, IDE_SR_DRDY, IDE_TIMEOUT)) < EOK)
        goto rollback;

    // 选择扇区
    ide_select_sector(disk, lba, count);

    // 发送读命令
    outb(ctrl->iobase + IDE_COMMAND, IDE_CMD_READ);

    task_t *task = running_task();
    for (size_t i = 0; i < count; i++)
    {
        // 阻塞自己等待中断的到来，等待磁盘准备数据
        ctrl->waiter = task;
        if ((ret = task_block(task, NULL, TASK_BLOCKED, IDE_TIMEOUT)) < EOK)
            goto rollback;

        if ((ret = ide_busy_wait(ctrl, IDE_SR_DRQ, IDE_TIMEOUT)) < EOK)
            goto rollback;

        u32 offset = ((u32)buf + i * SECTOR_SIZE);
        ide_pio_read_sector(disk, (u16 *)offset);
    }
    ret = EOK;

rollback:
    lock_release(&ctrl->lock);
    return ret;
}

// PIO 方式写磁盘
int ide_pio_write(ide_disk_t *disk, void *buf, u8 count, idx_t lba)
{
    assert(count > 0);
    assert(!get_interrupt_state()); // 异步方式，调用该函数时不许中断

    ide_ctrl_t *ctrl = disk->ctrl;

    lock_acquire(&ctrl->lock);

    int ret = EOK;

    LOGK("write lba 0x%x\n", lba);

    // 选择磁盘
    ide_select_drive(disk);

    // 等待就绪
    if ((ret = ide_busy_wait(ctrl, IDE_SR_DRDY, IDE_TIMEOUT)) < EOK)
        goto rollback;

    // 选择扇区
    ide_select_sector(disk, lba, count);

    // 发送写命令
    outb(ctrl->iobase + IDE_COMMAND, IDE_CMD_WRITE);
    task_t *task = running_task();
    for (size_t i = 0; i < count; i++)
    {
        u32 offset = ((u32)buf + i * SECTOR_SIZE);
        ide_pio_write_sector(disk, (u16 *)offset);

        // 阻塞自己等待中断的到来，等待磁盘准备数据
        ctrl->waiter = task;
        if ((ret = task_block(task, NULL, TASK_BLOCKED, IDE_TIMEOUT)) < EOK)
            goto rollback;

        if ((ret = ide_busy_wait(ctrl, IDE_SR_NULL, IDE_TIMEOUT)) < EOK)
            goto rollback;
    }
    ret = EOK;

rollback:
    lock_release(&ctrl->lock);
    return ret;
}

// 分区控制
int ide_pio_part_ioctl(ide_part_t *part, int cmd, void *args, int flags)
{
    switch (cmd)
    {
    case DEV_CMD_SECTOR_START:
        return part->start;
    case DEV_CMD_SECTOR_COUNT:
        return part->count;
    case DEV_CMD_SECTOR_SIZE:
        return part->disk->sector_size;
    default:
        panic("device command %d can't recognize!!!", cmd);
        break;
    }
}

// 读分区
int ide_pio_part_read(ide_part_t *part, void *buf, u8 count, idx_t lba)
{
    return ide_pio_read(part->disk, buf, count, part->start + lba);
}

// 写分区
int ide_pio_part_write(ide_part_t *part, void *buf, u8 count, idx_t lba)
{
    return ide_pio_write(part->disk, buf, count, part->start + lba);
}

static void ide_setup_dma(ide_ctrl_t *ctrl, int cmd, char *buf, u32 len)
{
    // 保证没有跨页
    assert(((u32)buf + len) <= ((u32)buf & (~0xfff)) + PAGE_SIZE);

    // 设置 prdt
    ctrl->prd.addr = get_paddr((u32)buf);
    ctrl->prd.len = len | IDE_LAST_PRD;

    // 设置 prd 地址
    outl(ctrl->bmbase + BM_PRD_ADDR, (u32)&ctrl->prd);

    // 设置读写
    outb(ctrl->bmbase + BM_COMMAND_REG, cmd | BM_CR_STOP);

    // 设置中断和错误
    outb(ctrl->bmbase + BM_STATUS_REG, inb(ctrl->bmbase + BM_STATUS_REG) | BM_SR_INT | BM_SR_ERR);
}

// 启动 DMA
static void ide_start_dma(ide_ctrl_t *ctrl)
{
    outb(ctrl->bmbase + BM_COMMAND_REG, inb(ctrl->bmbase + BM_COMMAND_REG) | BM_CR_START);
}

static err_t ide_stop_dma(ide_ctrl_t *ctrl)
{
    // 停止 DMA
    outb(ctrl->bmbase + BM_COMMAND_REG, inb(ctrl->bmbase + BM_COMMAND_REG) & (~BM_CR_START));

    // 获取 DMA 状态
    u8 status = inb(ctrl->bmbase + BM_STATUS_REG);

    // 清除中断和错误位
    outb(ctrl->bmbase + BM_STATUS_REG, status | BM_SR_INT | BM_SR_ERR);

    // 检测错误
    if (status & BM_SR_ERR)
    {
        LOGK("IDE dma error %02X\n", status);
        return -EIO;
    }
    return EOK;
}

err_t ide_udma_read(ide_disk_t *disk, void *buf, u8 count, idx_t lba)
{
    LOGK("IDE dma read lba 0x%x\n", lba);

    int ret = 0;
    ide_ctrl_t *ctrl = disk->ctrl;

    lock_acquire(&ctrl->lock);

    // 设置 DMA
    ide_setup_dma(ctrl, BM_CR_READ, buf, count * SECTOR_SIZE);

    // 选择扇区
    ide_select_sector(disk, lba, count);

    // 设置 UDMA 读
    outb(disk->ctrl->iobase + IDE_COMMAND, IDE_CMD_READ_UDMA);

    ide_start_dma(ctrl);

    disk->ctrl->waiter = running_task();
    if ((ret = task_block(disk->ctrl->waiter, NULL, TASK_BLOCKED, IDE_TIMEOUT)) < EOK)
    {
        LOGK("ide dma error occur!!! %d\n", ret);
    }

    assert(ide_stop_dma(ctrl) == EOK);

    lock_release(&ctrl->lock);
    return ret;
}

err_t ide_udma_write(ide_disk_t *disk, void *buf, u8 count, idx_t lba)
{
    LOGK("IDE dma write lba 0x%x\n", lba);
    int ret = EOK;
    ide_ctrl_t *ctrl = disk->ctrl;

    lock_acquire(&ctrl->lock);

    // 设置 DMA
    ide_setup_dma(ctrl, BM_CR_WRITE, buf, count * SECTOR_SIZE);

    // 选择扇区
    ide_select_sector(disk, lba, count);

    // 设置 UDMA 写
    outb(disk->ctrl->iobase + IDE_COMMAND, IDE_CMD_WRITE_UDMA);

    ide_start_dma(ctrl);

    disk->ctrl->waiter = running_task();
    if ((ret = task_block(disk->ctrl->waiter, NULL, TASK_BLOCKED, IDE_TIMEOUT)) < EOK)
    {
        LOGK("ide dma error occur!!! %d\n", ret);
    }

    assert(ide_stop_dma(ctrl) == EOK);

    lock_release(&ctrl->lock);
    return ret;
}

// 读取 atapi 数据包
static int ide_atapi_packet_read_pio(ide_disk_t *disk, u8 *pkt, int pktlen, void *buf, size_t bufsize)
{
    // 等待磁盘空闲
    // ide_busy_wait(disk->ctrl, IDE_SR_NULL);

    lock_acquire(&disk->ctrl->lock);

    // 设置寄存器
    outb(disk->ctrl->iobase + IDE_FEATURE, IDE_ATAPI_FEATURE_PIO);
    outb(disk->ctrl->iobase + IDE_SECTOR, 0);
    outb(disk->ctrl->iobase + IDE_LBA_LOW, 0);
    outb(disk->ctrl->iobase + IDE_LBA_MID, (bufsize & 0xFF));
    outb(disk->ctrl->iobase + IDE_LBA_HIGH, (bufsize >> 8) & 0xFF);
    outb(disk->ctrl->iobase + IDE_HDDEVSEL, disk->selector & 0x10);

    // 发送命令
    outb(disk->ctrl->iobase + IDE_COMMAND, IDE_CMD_PACKET);

    int ret = EOF;
    // 等待磁盘就绪
    if ((ret = ide_busy_wait(disk->ctrl, IDE_SR_DRDY, IDE_TIMEOUT)) < 0)
        goto rollback;

    // 输出 pkt 内容
    u16 *ptr = (u16 *)pkt;
    for (size_t i = 0; i < pktlen / 2; i++)
    {
        outw(disk->ctrl->iobase + IDE_DATA, ptr[i]);
    }

    // 阻塞自己等待磁盘写数据完成
    task_t *task = running_task();
    disk->ctrl->waiter = task;
    if ((ret = task_block(task, NULL, TASK_BLOCKED, IDE_TIMEOUT)) < 0)
        goto rollback;

    int bufleft = bufsize;

    ptr = (u16 *)buf;
    int idx = 0;
    while (bufleft > 0)
    {
        // 等待磁盘空闲
        if ((ret = ide_busy_wait(disk->ctrl, IDE_SR_NULL, IDE_TIMEOUT)) < 0)
            goto rollback;

        // 输入字节数量
        int bytes = inb(disk->ctrl->iobase + IDE_LBA_HIGH) << 8;
        bytes |= inb(disk->ctrl->iobase + IDE_LBA_MID);

        assert(bytes >= 0);

        if (bytes == 0)
            break;
        assert(bufleft <= bytes);
        // 输入字节
        for (size_t i = 0; i < bytes / 2; i++)
        {
            ptr[idx++] = inw(disk->ctrl->iobase + IDE_DATA);
        }
        bufleft -= bytes;
    }
    ret = bufsize - bufleft;

rollback:
    lock_release(&disk->ctrl->lock);
    return ret;
}

static int ide_atapi_packet_read_dma(ide_disk_t *disk, u8 *pkt, int pktlen, void *buf, size_t bufsize)
{
    // 等待磁盘空闲
    // ide_busy_wait(disk->ctrl, IDE_SR_NULL);

    lock_acquire(&disk->ctrl->lock);

    ide_setup_dma(disk->ctrl, BM_CR_READ, buf, bufsize);

    // 设置寄存器
    outb(disk->ctrl->iobase + IDE_FEATURE, IDE_ATAPI_FEATURE_DMA);
    outb(disk->ctrl->iobase + IDE_SECTOR, 0);
    outb(disk->ctrl->iobase + IDE_LBA_LOW, 0);
    outb(disk->ctrl->iobase + IDE_LBA_MID, 0);
    outb(disk->ctrl->iobase + IDE_LBA_HIGH, 0);
    outb(disk->ctrl->iobase + IDE_HDDEVSEL, disk->selector & 0x10);

    // 发送命令
    outb(disk->ctrl->iobase + IDE_COMMAND, IDE_CMD_PACKET);

    int ret = EOF;
    // 等待磁盘就绪
    if ((ret = ide_busy_wait(disk->ctrl, IDE_SR_DRDY, IDE_TIMEOUT)) < 0)
        goto rollback;

    // 输出 pkt 内容
    u16 *ptr = (u16 *)pkt;
    for (size_t i = 0; i < pktlen / 2; i++)
    {
        outw(disk->ctrl->iobase + IDE_DATA, ptr[i]);
    }

    ide_start_dma(disk->ctrl); /* code */

    // 阻塞自己等待磁盘写数据完成
    task_t *task = running_task();
    disk->ctrl->waiter = task;
    if ((ret = task_block(task, NULL, TASK_BLOCKED, IDE_TIMEOUT)) < 0)
        goto rollback;

    assert(ide_stop_dma(disk->ctrl) == EOK);
    ret = bufsize;

rollback:
    lock_release(&disk->ctrl->lock);
    return ret;
}

// 读取 ATAPI 容量
static int ide_atapi_read_capacity(ide_disk_t *disk)
{
    u8 pkt[12];
    u32 buf[2];
    u32 block_count;
    u32 block_size;

    memset(pkt, 0, sizeof(pkt));
    pkt[0] = IDE_ATAPI_CMD_READCAPICITY;

    int (*function)() = ide_atapi_packet_read_pio;
    if (disk->ctrl->iotype == IDE_TYPE_UDMA)
        function = ide_atapi_packet_read_dma;

    int ret = function(disk, pkt, sizeof(pkt), buf, sizeof(buf));
    if (ret < 0)
        return ret;
    if (ret != sizeof(buf))
        return -EIO;

    block_count = ntohl(buf[0]);
    block_size = ntohl(buf[1]);

    if (block_size != disk->sector_size)
    {
        LOGK("CD block size warning %d\n", block_size);
        return 0;
    }
    return block_count;
}

// atapi 设备读
static int ide_atapi_read(ide_disk_t *disk, void *buf, int count, idx_t lba)
{
    u8 pkt[12];
    if (count > 0xffff)
        return -EIO;

    memset(pkt, 0, sizeof(pkt));

    pkt[0] = IDE_ATAPI_CMD_READ10;
    pkt[2] = lba >> 24;
    pkt[3] = (lba >> 16) & 0xFF;
    pkt[4] = (lba >> 8) & 0xFF;
    pkt[5] = lba & 0xFF;
    pkt[7] = (count >> 8) & 0xFF;
    pkt[8] = count & 0xFF;

    int (*function)() = ide_atapi_packet_read_pio;
    if (disk->ctrl->iotype == IDE_TYPE_UDMA)
        function = ide_atapi_packet_read_dma;

    return function(disk, pkt, sizeof(pkt), buf, count * disk->sector_size);
}

// 探测设备
static err_t ide_probe_device(ide_disk_t *disk)
{
    outb(disk->ctrl->iobase + IDE_HDDEVSEL, disk->selector & IDE_SEL_MASK);
    ide_delay();

    outb(disk->ctrl->iobase + IDE_SECTOR, 0x55);
    outb(disk->ctrl->iobase + IDE_CHS_SECTOR, 0xAA);

    outb(disk->ctrl->iobase + IDE_SECTOR, 0xAA);
    outb(disk->ctrl->iobase + IDE_CHS_SECTOR, 0x55);

    outb(disk->ctrl->iobase + IDE_SECTOR, 0x55);
    outb(disk->ctrl->iobase + IDE_CHS_SECTOR, 0xAA);

    u8 sector_count = inb(disk->ctrl->iobase + IDE_SECTOR);
    u8 sector_index = inb(disk->ctrl->iobase + IDE_CHS_SECTOR);

    if (sector_count == 0x55 && sector_index == 0xAA)
        return EOK;
    return -EIO;
}

// 检测设备类型
static int ide_interface_type(ide_disk_t *disk)
{
    outb(disk->ctrl->iobase + IDE_COMMAND, IDE_CMD_DIAGNOSTIC);
    if (ide_busy_wait(disk->ctrl, IDE_SR_NULL, IDE_TIMEOUT) < EOK)
        return IDE_INTERFACE_UNKNOWN;

    outb(disk->ctrl->iobase + IDE_HDDEVSEL, disk->selector & IDE_SEL_MASK);
    ide_delay();

    u8 sector_count = inb(disk->ctrl->iobase + IDE_SECTOR);
    u8 sector_index = inb(disk->ctrl->iobase + IDE_LBA_LOW);
    if (sector_count != 1 || sector_index != 1)
        return IDE_INTERFACE_UNKNOWN;

    u8 cylinder_low = inb(disk->ctrl->iobase + IDE_CHS_CYL);
    u8 cylinder_high = inb(disk->ctrl->iobase + IDE_CHS_CYH);
    u8 state = inb(disk->ctrl->iobase + IDE_STATUS);

    if (cylinder_low == 0x14 && cylinder_high == 0xeb)
        return IDE_INTERFACE_ATAPI;

    if (cylinder_low == 0 && cylinder_high == 0 && state != 0)
        return IDE_INTERFACE_ATA;

    return IDE_INTERFACE_UNKNOWN;
}

static void ide_fixstrings(char *buf, u32 len)
{
    for (size_t i = 0; i < len; i += 2)
    {
        register char ch = buf[i];
        buf[i] = buf[i + 1];
        buf[i + 1] = ch;
    }
    buf[len - 1] = '\0';
}

static err_t ide_identify(ide_disk_t *disk, u16 *buf)
{
    LOGK("identifing disk %s...\n", disk->name);
    lock_acquire(&disk->ctrl->lock);
    ide_select_drive(disk);

    // ide_select_sector(disk, 0, 0);
    u8 cmd = IDE_CMD_IDENTIFY;
    if (disk->interface == IDE_INTERFACE_ATAPI)
    {
        cmd = IDE_CMD_PIDENTIFY;
    }

    outb(disk->ctrl->iobase + IDE_COMMAND, cmd);

    int ret = EOF;
    if ((ret = ide_busy_wait(disk->ctrl, IDE_SR_NULL, IDE_TIMEOUT)) < EOK)
        goto rollback;

    ide_params_t *params = (ide_params_t *)buf;

    ide_pio_read_sector(disk, buf);

    ide_fixstrings(params->serial, sizeof(params->serial));
    LOGK("disk %s serial number %s\n", disk->name, params->serial);

    ide_fixstrings(params->firmware, sizeof(params->firmware));
    LOGK("disk %s firmware version %s\n", disk->name, params->firmware);

    ide_fixstrings(params->model, sizeof(params->model));
    LOGK("disk %s model number %s\n", disk->name, params->model);

    if (disk->interface == IDE_INTERFACE_ATAPI)
    {
        ret = EOK;
        goto rollback;
    }

    if (params->total_lba == 0)
    {
        ret = -EIO;
        goto rollback;
    }
    LOGK("disk %s total lba %d\n", disk->name, params->total_lba);

    disk->total_lba = params->total_lba;
    disk->cylinders = params->cylinders;
    disk->heads = params->heads;
    disk->sectors = params->sectors;
    ret = EOK;

rollback:
    lock_release(&disk->ctrl->lock);
    return ret;
}

static void ide_part_init(ide_disk_t *disk, u16 *buf)
{
    // 磁盘不可用
    if (!disk->total_lba)
        return;

    // 读取主引导扇区
    ide_pio_read(disk, buf, 1, 0);

    // 初始化主引导扇区
    boot_sector_t *boot = (boot_sector_t *)buf;

    for (size_t i = 0; i < IDE_PART_NR; i++)
    {
        part_entry_t *entry = &boot->entry[i];
        ide_part_t *part = &disk->parts[i];
        if (!entry->count)
            continue;

        sprintf(part->name, "%s%d", disk->name, i + 1);

        LOGK("part %s \n", part->name);
        LOGK("    bootable %d\n", entry->bootable);
        LOGK("    start %d\n", entry->start);
        LOGK("    count %d\n", entry->count);
        LOGK("    system 0x%x\n", entry->system);

        part->disk = disk;
        part->count = entry->count;
        part->system = entry->system;
        part->start = entry->start;

        if (entry->system == PART_FS_EXTENDED)
        {
            LOGK("Unsupported extended partition!!!\n");

            boot_sector_t *eboot = (boot_sector_t *)(buf + SECTOR_SIZE);
            ide_pio_read(disk, (void *)eboot, 1, entry->start);

            for (size_t j = 0; j < IDE_PART_NR; j++)
            {
                part_entry_t *eentry = &eboot->entry[j];
                if (!eentry->count)
                    continue;
                LOGK("part %d extend %d\n", i, j);
                LOGK("    bootable %d\n", eentry->bootable);
                LOGK("    start %d\n", eentry->start);
                LOGK("    count %d\n", eentry->count);
                LOGK("    system 0x%x\n", eentry->system);
            }
        }
    }
}

// ide 控制器初始化
static void ide_ctrl_init()
{
    int iotype = IDE_TYPE_PIO;
    int bmbase = 0;

    pci_device_t *device = pci_find_device_by_class(PCI_CLASS_STORAGE_IDE);
    if (device)
    {
        pci_bar_t bar;
        int ret = pci_find_bar(device, &bar, PCI_BAR_TYPE_IO);
        assert(ret == EOK);

        LOGK("find dev 0x%x io bar 0x%x size %d\n",
             device, bar.iobase, bar.size);

        pci_enable_busmastering(device);

        iotype = IDE_TYPE_UDMA;
        bmbase = bar.iobase;
    }

    u16 *buf = (u16 *)alloc_kpage(1);
    for (size_t cidx = 0; cidx < IDE_CTRL_NR; cidx++)
    {
        ide_ctrl_t *ctrl = &controllers[cidx];
        sprintf(ctrl->name, "ide%u", cidx);
        lock_init(&ctrl->lock);
        ctrl->active = NULL;
        ctrl->waiter = NULL;
        ctrl->iotype = iotype;
        ctrl->bmbase = bmbase + cidx * 8;

        if (cidx) // 从通道
        {
            ctrl->iobase = IDE_IOBASE_SECONDARY;
        }
        else // 主通道
        {
            ctrl->iobase = IDE_IOBASE_PRIMARY;
        }

        ctrl->control = inb(ctrl->iobase + IDE_CONTROL);

        for (size_t didx = 0; didx < IDE_DISK_NR; didx++)
        {
            ide_disk_t *disk = &ctrl->disks[didx];
            sprintf(disk->name, "hd%c", 'a' + cidx * 2 + didx);
            disk->ctrl = ctrl;
            if (didx) // 从盘
            {
                disk->master = false;
                disk->selector = IDE_LBA_SLAVE;
            }
            else // 主盘
            {
                disk->master = true;
                disk->selector = IDE_LBA_MASTER;
            }
            if (ide_probe_device(disk) < 0)
            {
                LOGK("IDE device %s not exists...\n", disk->name);
                continue;
            }

            disk->interface = ide_interface_type(disk);
            LOGK("IDE device %s type %d...\n", disk->name, disk->interface);
            if (disk->interface == IDE_INTERFACE_UNKNOWN)
                continue;

            if (disk->interface == IDE_INTERFACE_ATA)
            {
                disk->sector_size = SECTOR_SIZE;
                if (ide_identify(disk, buf) == EOK)
                {
                    ide_part_init(disk, buf);
                }
            }
            else if (disk->interface == IDE_INTERFACE_ATAPI)
            {
                LOGK("Disk %s interface is ATAPI\n", disk->name);
                disk->sector_size = CD_SECTOR_SIZE;
                if (ide_identify(disk, buf) == EOK)
                {
                    disk->total_lba = ide_atapi_read_capacity(disk);
                    LOGK("disk %s total lba %d\n", disk->name, disk->total_lba);
                }
            }
        }
    }
    free_kpage((u32)buf, 1);
}

static void ide_install()
{
    void *read = ide_pio_read;
    void *write = ide_pio_write;

    for (size_t cidx = 0; cidx < IDE_CTRL_NR; cidx++)
    {
        ide_ctrl_t *ctrl = &controllers[cidx];
        if (ctrl->iotype == IDE_TYPE_UDMA)
        {
            read = ide_udma_read;
            write = ide_udma_write;
        }

        for (size_t didx = 0; didx < IDE_DISK_NR; didx++)
        {
            ide_disk_t *disk = &ctrl->disks[didx];
            if (!disk->total_lba)
                continue;

            if (disk->interface == IDE_INTERFACE_ATA)
            {
                dev_t dev = device_install(
                    DEV_BLOCK, DEV_IDE_DISK, disk, disk->name, 0,
                    ide_pio_ioctl, read, write);
                for (size_t i = 0; i < IDE_PART_NR; i++)
                {
                    ide_part_t *part = &disk->parts[i];
                    if (!part->count)
                        continue;
                    device_install(
                        DEV_BLOCK, DEV_IDE_PART, part, part->name, dev,
                        ide_pio_part_ioctl, ide_pio_part_read, ide_pio_part_write);
                }
            }
            else if (disk->interface == IDE_INTERFACE_ATAPI)
            {
                device_install(
                    DEV_BLOCK, DEV_IDE_CD, disk, disk->name, 0,
                    ide_pio_ioctl, ide_atapi_read, NULL);
            }
        }
    }
}

// ide 硬盘初始化
void ide_init()
{
    LOGK("ide init...\n");

    // 注册硬盘中断，并打开屏蔽字
    set_interrupt_handler(IRQ_HARDDISK, ide_handler);
    set_interrupt_handler(IRQ_HARDDISK2, ide_handler);
    set_interrupt_mask(IRQ_HARDDISK, true);
    set_interrupt_mask(IRQ_HARDDISK2, true);
    set_interrupt_mask(IRQ_CASCADE, true);

    ide_ctrl_init();

    ide_install(); // 安装设备
}
