#include <onix/types.h>
#include <onix/stdio.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/rtc.h>
#include <onix/string.h>
#include <onix/interrupt.h>
#include <onix/io.h>
#include <onix/task.h>
#include <onix/errno.h>
#include <onix/syscall.h>
#include <onix/mutex.h>
#include <onix/memory.h>
#include <onix/device.h>
#include <onix/isa.h>
#include <onix/timer.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define FDT_NONE 0
#define FDT_144M 4
#define FDT_INVALID -1

#define FDC_OUTB_TIMEOUT 1000
#define FDC_INB_TIMEOUT 1000
#define FDC_WAIT_TIMEOUT 10000
#define FDC_MOTOR_WAITING 1     // 这里等待时间实际可能太小，但在模拟器中无关
#define FDC_MOTOR_TIMEOUT 10000 // 超时未使用软盘，则关闭马达

#define FDC_DOR 0x3F2  // Digital Output Register
#define FDC_MSR 0x3F4  // Main Status Register (input)
#define FDC_DRS 0x3F4  // Data Rate Select Register (output)
#define FDC_DATA 0x3F5 // Data Register
#define FDC_DIR 0x3F7  // Digital Input Register (input)
#define FDC_CCR 0x3F7  // Configuration Control Register

#define MSR_ACTA 0x1   // 驱动器 0 寻道
#define MSR_ACTB 0x2   // 驱动器 1 寻道
#define MSR_ACTC 0x4   // 驱动器 2 寻道
#define MSR_ACTD 0x8   // 驱动器 3 寻道
#define MSR_BUSY 0x10  // 命令忙
#define MSR_NDMA 0x20  // 0 - 为 DMA 数据传输模式，1 - 为非 DMA 模式
#define MSR_DIO 0x40   // 传输方向：0 - CPU -> FDC，1 - 相反
#define MSR_READY 0x80 // 数据寄存器就绪位

#define DOR_DSEL0 0x1  // 选择驱动器
#define DOR_DSEL1 0x2  // 选择驱动器
#define DOR_NORMAL 0x4 // 常规状态 该位为 0 表示重置
#define DOR_IRQ 0x8    // 中断允许
#define DOR_MOTA 0x10  // 驱动器 0 马达开启
#define DOR_MOTB 0x20  // 驱动器 1 马达开启
#define DOR_MOTC 0x40  // 驱动器 2 马达开启
#define DOR_MOTD 0x80  // 驱动器 3 马达开启

#define DIR_CHANGED 0x80 // 磁盘发生变化

#define CMD_SPECIFY 0x03     // Specify drive timings
#define CMD_WRITE 0xC5       // Write data (+ MT,MFM)
#define CMD_READ 0xE6        // Read data (+ MT,MFM,SK)
#define CMD_RECALIBRATE 0x07 // Recalibrate
#define CMD_SENSEI 0x08      // Sense interrupt status
#define CMD_FORMAT 0x4D      // Format track (+ MFM)
#define CMD_SEEK 0x0F        // Seek track
#define CMD_VERSION 0x10     // FDC version
#define CMD_RESET 0xFE       // FDC Reset

#define FDC_82077 0x90 // Extended uPD765B controller

#define MOTOR_OFF 0
#define MOTOR_DELAY 1
#define MOTOR_ON 2

#define FD_READ 0
#define FD_WRITE 1

#define SECTOR_SIZE 512

#define DMA_BUF_ADDR 0x98000 // 不能跨越 64K 边界

#define RESULT_NR 8

typedef struct fdresult_t
{
    u8 st0;
    u8 st1;
    u8 st2;
    u8 st3;
    u8 track;
    u8 head;
    u8 sector;
    u8 size;
} fdresult_t;

typedef struct floppy_t
{
    task_t *waiter; // 等待进程
    timer_t *timer; // 定时器
    lock_t lock;    // 锁

    char name[8];
    int type; // 软盘类型

    u8 dor; // dor registers

    u8 *buf; // DMA 地址 (其实是一样的)

    union
    {
        u8 tracks;    // 磁道数
        u8 cylinders; // 柱面数
    };
    u8 heads;   // 磁头数
    u8 sectors; // 每磁道扇区数
    u8 gap3;    // GAP3 长度

    u8 drive;   // 磁盘序号
    u8 st0;     //
    u8 track;   // 当前磁道
    u8 motor;   // 马达状态
    u8 changed; // 磁盘发生改变

    union
    {
        u8 st[RESULT_NR];
        fdresult_t result;
    };

} floppy_t;

// 为简单起见，系统暂时只支持一个 1.44M 软盘
static floppy_t floppy;

// 软盘中断
static void fd_handler(int vector)
{
    send_eoi(vector); // 发送中断结束信号；
    LOGK("floppy handler ....\n");

    floppy_t *fd = &floppy;

    if (fd->waiter)
    {
        task_unblock(fd->waiter, EOK);
        fd->waiter = NULL;
    }
}

// 获得软盘驱动器类型
static u8 fd_type()
{
    u8 type = cmos_read(0x10);
    return (type >> 4) & 0xf; // 高四位表示主软驱
}

static err_t fd_outb(u8 byte)
{
    u32 expire = timer_expire_jiffies(FDC_OUTB_TIMEOUT);

    extern u32 jiffies;

    while (true)
    {
        if (timer_is_expires(expire))
            return -ETIME;
        u8 msr = inb(FDC_MSR) & (MSR_READY | MSR_DIO);
        LOGK("out state 0x%X %d %d...\n", msr, expire, jiffies);
        if (msr == MSR_READY)
        {
            outb(FDC_DATA, byte);
            return EOK;
        }
        task_yield();
    }
}

static err_t fd_inb()
{
    u32 expire = timer_expire_jiffies(FDC_INB_TIMEOUT);
    while (true)
    {
        if (timer_is_expires(expire))
            return -ETIME;
        u8 msr = inb(FDC_MSR) & (MSR_READY | MSR_DIO | MSR_BUSY);
        if (msr == (MSR_READY | MSR_DIO | MSR_BUSY))
            return inb(FDC_DATA) & 0xFF;
        task_yield();
    }
}

// 等待中断到来
static err_t fd_wait(floppy_t *fd)
{
    assert(!fd->waiter);
    fd->waiter = running_task();
    return task_block(fd->waiter, NULL, TASK_BLOCKED, FDC_WAIT_TIMEOUT);
}

// 得到执行的结果
static void fd_result(floppy_t *fd, bool sensei)
{
    int n = 0;
    while (n < 7)
    {
        u8 msr = inb(FDC_MSR) & (MSR_READY | MSR_DIO | MSR_BUSY);
        if (msr == MSR_READY)
        {
            break;
        }
        if (msr == (MSR_READY | MSR_DIO | MSR_BUSY))
        {
            fd->st[n++] = fd_inb();
        }
    }

    if (sensei)
    {
        // Send a "sense interrupt status" command
        fd_outb(CMD_SENSEI);
        fd->st0 = fd_inb();
        fd->track = fd_inb();
    }

    // Check for disk changed
    if (inb(FDC_DIR) & DIR_CHANGED)
        fd->changed = true;
}

static void fd_motor_on(floppy_t *fd)
{
    if (fd->motor != MOTOR_OFF)
    {
        fd->motor = MOTOR_ON;
        return;
    }

    LOGK("fd: motor on\n");

    fd->dor |= 0x10 << fd->drive;
    outb(FDC_DOR, fd->dor);
    fd->motor = MOTOR_ON;
    task_sleep(FDC_MOTOR_WAITING);
}

static void fd_motor_timeout(timer_t *timer)
{
    LOGK("floppy motor off timeout\n");
    floppy_t *fd = timer->arg;
    fd->dor &= ~(0x10 << fd->drive);
    outb(FDC_DOR, fd->dor);
    fd->motor = MOTOR_OFF;
    fd->timer = NULL;
}

static void fd_motor_off(floppy_t *fd)
{
    if (fd->motor == MOTOR_ON)
    {
        LOGK("floppy motor off delay\n");
        fd->motor = MOTOR_DELAY;
    }
    if (!fd->timer)
    {
        fd->timer = timer_add(FDC_MOTOR_TIMEOUT, fd_motor_timeout, fd);
    }
    else
    {
        timer_update(fd->timer, FDC_MOTOR_TIMEOUT);
    }
}

// 重新校正，将磁道归零
static void fd_recalibrate(floppy_t *fd)
{
    LOGK("fd: recalibrate\n");

    fd_outb(CMD_RECALIBRATE);
    fd_outb(0x00);

    assert(fd_wait(fd) == EOK);

    fd_result(fd, true);
}

// 设置软盘的控制细节
static void fd_specify(floppy_t *fd)
{
    LOGK("floppy specify...\n");
    fd_outb(CMD_SPECIFY);
    fd_outb(0xDF); // SRT = 3ms, HUT = 240ms
    fd_outb(0x02); // HLT = 16ms, ND = 0
}

// 寻道
static void fd_seek(floppy_t *fd, u8 track, u8 head)
{
    if (fd->track == track)
        return;

    LOGK("fd: seek track %d head %d (current %d)\n", track, head, fd->track);
    fd_outb(CMD_SEEK);
    fd_outb((unsigned char)((head << 2) | fd->drive));
    fd_outb(track);

    assert(fd_wait(fd) == EOK);

    fd_result(fd, true);

    if ((fd->st0 & 0xE0) != 0x20 || fd->track != track)
    {
        panic("fd: seek failed, st0 0x%02x, current %d, target %d\n", fd->st0, fd->track, track);
    }

    // 休眠 15 毫秒
    task_sleep(15);
}

static void lba2chs(floppy_t *fd, idx_t lba, u8 *track, u8 *head, u8 *sector)
{
    *track = lba / (fd->heads * fd->sectors);
    *head = (lba / fd->sectors) % fd->heads;
    *sector = lba % fd->sectors + 1;
}

static err_t fd_transfer(floppy_t *fd, bool mode, void *buf, u8 count, idx_t lba)
{
    u8 track, head, sector;

    // 转换 lba 到 chs cylinder head sector
    lba2chs(fd, lba, &track, &head, &sector);

    // 当前磁道剩余扇区数
    u8 remaining = ((fd->sectors + 1 - sector) + fd->sectors * (fd->heads - head - 1));

    // 这个地方确实有问题，可能存在读取内容较少，外层做了兼容；
    if (remaining < count)
        count = remaining;

    if (mode == FD_WRITE)
        memcpy(fd->buf, buf, count * SECTOR_SIZE);

    // Perform seek if necessary
    fd_seek(fd, track, head);

    // Program data rate (500K/s)
    outb(FDC_CCR, 0);

    // 设置 DMA
    isa_dma_mask(2, false);
    isa_dma_reset(2);

    if (mode == FD_READ)
        isa_dma_mode(2, DMA_MODE_SINGLE | DMA_MODE_READ);
    else
        isa_dma_mode(2, DMA_MODE_SINGLE | DMA_MODE_WRITE);

    // Setup DMA transfer
    isa_dma_addr(2, fd->buf);
    isa_dma_size(2, (u32)count * SECTOR_SIZE);
    isa_dma_mask(2, true);

    // 执行读写请求
    if (mode == FD_READ)
        fd_outb(CMD_READ);
    else
        fd_outb(CMD_WRITE);

    fd_outb((u8)((head << 2) | fd->drive));
    fd_outb(track);
    fd_outb(head);
    fd_outb(sector);
    fd_outb(0x02); // 512 bytes/sector
    fd_outb(fd->sectors);
    fd_outb(fd->gap3);
    fd_outb(0xFF); // DTL = unused

    assert(fd_wait(fd) == EOK); // 等待中断

    fd_result(fd, false);

    if ((fd->result.st0 & 0xC0) == 0)
    {
        // Successful transfer
        if (mode == FD_READ)
            memcpy(buf, fd->buf, count * SECTOR_SIZE);
        return EOK;
    }
    else
    {
        LOGK("fd: xfer error, st0 %02X st1 %02X st2 %02X THS=%d/%d/%d\n",
             fd->result.st0, fd->result.st1, fd->result.st2,
             fd->result.track, fd->result.head, fd->result.sector);
        return -EIO;
    }
}

static err_t fd_read(floppy_t *fd, void *buf, u8 count, idx_t lba)
{
    assert(count + lba < (u32)fd->tracks * fd->heads * fd->sectors);

    lock_acquire(&fd->lock);
    fd_motor_on(fd);

    int ret = fd_transfer(fd, FD_READ, buf, count, lba);

    fd_motor_off(fd);
    lock_release(&fd->lock);
    return ret;
}

static err_t fd_write(floppy_t *fd, void *buf, u8 count, idx_t lba)
{
    assert(count + lba < (u32)fd->tracks * fd->heads * fd->sectors);

    lock_acquire(&fd->lock);
    fd_motor_on(fd);

    int ret = fd_transfer(fd, FD_WRITE, buf, count, lba);

    fd_motor_off(fd);
    lock_release(&fd->lock);
    return ret;
}

// 磁盘控制
static int fd_ioctl(floppy_t *fd, int cmd, void *args, int flags)
{
    switch (cmd)
    {
    case DEV_CMD_SECTOR_START:
        return 0;
    case DEV_CMD_SECTOR_COUNT:
        return (int)fd->tracks * fd->heads * fd->sectors;
    case DEV_CMD_SECTOR_SIZE:
        return SECTOR_SIZE;
    default:
        panic("device command %d can't recognize!!!", cmd);
        break;
    }
}

static err_t fd_reset(floppy_t *fd)
{
    LOGK("floppy setup...\n");
    outb(FDC_DOR, DOR_IRQ | (fd->drive) | (0x10 << fd->drive));
    // assert(fd_wait(fd) == EOK);
    return EOK;
}

static err_t fd_setup(floppy_t *fd)
{
    // fd_reset(fd);

    // LOGK("floppy check version...\n");
    // err_t err;
    // if ((err = fd_outb(CMD_VERSION)) < EOK)
    //     return err;

    // u8 result = fd_inb();
    // LOGK("floppy result 0x%X\n", result);
    // if (result != FDC_82077)
    // {
    //     LOGK("unsupported floppy controller...\n");
    //     fd->type = FDT_INVALID;
    //     return -EIO;
    // }

    fd_specify(fd);
    fd_recalibrate(fd);
    return EOK;
}

void floppy_init()
{
    LOGK("floppy disk init...\n");

    floppy_t *fd = &floppy;
    fd->type = fd_type();
    if (!fd->type) // 主驱动器不存在
    {
        LOGK("floppy drive not exists...\n");
        return;
    }

    strcpy(fd->name, "fda");
    if (fd->type != FDT_144M)
    {
        LOGK("floppy %s type %d not supported!!!\n", fd->name, fd->type);
        return;
    }

    LOGK("floppy %s 1.44M init...\n", fd->name);

    // 设置中断处理函数，以及中断屏蔽字
    set_interrupt_handler(IRQ_FLOPPY, fd_handler);
    set_interrupt_mask(IRQ_FLOPPY, true);

    lock_init(&fd->lock);

    fd->waiter = NULL;

    fd->dor = (DOR_IRQ | DOR_NORMAL);

    fd->buf = (u8 *)DMA_BUF_ADDR;

    fd->tracks = 80;
    fd->heads = 2;
    fd->sectors = 18;
    fd->gap3 = 0x1B;

    fd->drive = 0;
    fd->track = 0xFF;

    if (fd_setup(fd) < EOK)
        return;

    device_install(
        DEV_BLOCK, DEV_FLOPPY, fd, fd->name, 0,
        fd_ioctl, fd_read, fd_write);
}
