#include <onix/types.h>
#include <onix/pci.h>
#include <onix/io.h>
#include <onix/mio.h>
#include <onix/memory.h>
#include <onix/task.h>
#include <onix/arena.h>
#include <onix/string.h>
#include <onix/interrupt.h>
#include <onix/net.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/syscall.h>
#include <onix/stdlib.h>
#include <onix/printk.h>
#include <onix/errno.h>
#include <onix/net/pbuf.h>
#include <onix/net/chksum.h>
#include <onix/net/netif.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define VENDORID 0x8086 // 供应商英特尔

// 8254x 兼容网卡
// https://www.pcilookup.com/?ven=%098086&dev=&action=submit
#define DEVICEID_LOW 0x1000
#define DEVICEID_HIGH 0x1028

// 寄存器偏移
enum REGISTERS
{
    E1000_CTRL = 0x00,   // Device Control 设备控制
    E1000_STATUS = 0x08, // Device Status 设备状态
    E1000_EERD = 0x14,   // EEPROM Read EEPROM 读取

    E1000_ICR = 0xC0, // Interrupt Cause Read 中断原因读
    E1000_ITR = 0xC4, // Interrupt Throttling 中断节流
    E1000_ICS = 0xC8, // Interrupt Cause Set 中断原因设置
    E1000_IMS = 0xD0, // Interrupt Mask Set/Read 中断掩码设置/读
    E1000_IMC = 0xD8, // Interrupt Mask Clear 中断掩码清除

    E1000_RCTL = 0x100,   // Receive Control 接收控制
    E1000_RDBAL = 0x2800, // Receive Descriptor Base Address LOW 接收描述符低地址
    E1000_RDBAH = 0x2804, // Receive Descriptor Base Address HIGH 64bit only 接收描述符高地址
    E1000_RDLEN = 0x2808, // Receive Descriptor Length 接收描述符长度
    E1000_RDH = 0x2810,   // Receive Descriptor Head 接收描述符头
    E1000_RDT = 0x2818,   // Receive Descriptor Tail 接收描述符尾

    E1000_TCTL = 0x400,   // Transmit Control 发送控制
    E1000_TDBAL = 0x3800, // Transmit Descriptor Base Low 传输描述符低地址
    E1000_TDBAH = 0x3804, // Transmit Descriptor Base High 传输描述符高地址
    E1000_TDLEN = 0x3808, // Transmit Descriptor Length 传输描述符长度
    E1000_TDH = 0x3810,   // TDH Transmit Descriptor Head 传输描述符头
    E1000_TDT = 0x3818,   // TDT Transmit Descriptor Tail 传输描述符尾

    E1000_MAT0 = 0x5200, // Multicast Table Array 05200h-053FCh 组播表数组
    E1000_MAT1 = 0x5400, // Multicast Table Array 05200h-053FCh 组播表数组
};

// 设备状态
enum STATUS
{
    STATUS_FD = 1 << 0, // Full Duplex 全双工传输
    STATUS_LU = 1 << 1, // Link Up 网线连接
};

// 中断类型
enum IMS
{
    // 传输描述符写回，表示有一个数据包发出
    IM_TXDW = 1 << 0, // Transmit Descriptor Written Back.

    // 传输队列为空
    IM_TXQE = 1 << 1, // Transmit Queue Empty.

    // 连接状态变化，可以认为是网线拔掉或者插上
    IM_LSC = 1 << 2, // Link Status Change

    // 接收序列错误
    IM_RXSEQ = 1 << 3, // Receive Sequence Error.

    // 到达接受描述符最小阈值，表示流量太大，接收描述符太少了，应该再多加一些，不过没有数据包丢失
    IM_RXDMT0 = 1 << 4, // Receive Descriptor Minimum Threshold hit.

    // 因为没有可用的接收缓冲区或因为PCI接收带宽不足，已经溢出，有数据包丢失
    IM_RXO = 1 << 6, // Receiver FIFO Overrun.

    // 接收定时器中断
    IM_RXT0 = 1 << 7, // Receiver Timer Interrupt.

    // 这个位在 MDI/O 访问完成时设置
    IM_MADC = 1 << 9, // MDI/O Access Complete Interrupt

    IM_RXCFG = 1 << 10,  // Receiving /C/ ordered sets.
    IM_PHYINT = 1 << 12, // Sets mask for PHY Interrupt
    IM_GPI0 = 1 << 13,   // General Purpose Interrupts.
    IM_GPI1 = 1 << 14,   // General Purpose Interrupts.

    // 传输描述符环已达到传输描述符控制寄存器中指定的阈值。
    IM_TXDLOW = 1 << 15, // Transmit Descriptor Low Threshold hit
    IM_SRPD = 1 << 16,   // Small Receive Packet Detection
};

// 接收控制
enum RCTL
{
    RCTL_EN = 1 << 1,               // Receiver Enable
    RCTL_SBP = 1 << 2,              // Store Bad Packets
    RCTL_UPE = 1 << 3,              // Unicast Promiscuous Enabled
    RCTL_MPE = 1 << 4,              // Multicast Promiscuous Enabled
    RCTL_LPE = 1 << 5,              // Long Packet Reception Enable
    RCTL_LBM_NONE = 0b00 << 6,      // No Loopback
    RCTL_LBM_PHY = 0b11 << 6,       // PHY or external SerDesc loopback
    RTCL_RDMTS_HALF = 0b00 << 8,    // Free Buffer Threshold is 1/2 of RDLEN
    RTCL_RDMTS_QUARTER = 0b01 << 8, // Free Buffer Threshold is 1/4 of RDLEN
    RTCL_RDMTS_EIGHTH = 0b10 << 8,  // Free Buffer Threshold is 1/8 of RDLEN

    RCTL_BAM = 1 << 15, // Broadcast Accept Mode
    RCTL_VFE = 1 << 18, // VLAN Filter Enable

    RCTL_CFIEN = 1 << 19, // Canonical Form Indicator Enable
    RCTL_CFI = 1 << 20,   // Canonical Form Indicator Bit Value
    RCTL_DPF = 1 << 22,   // Discard Pause Frames
    RCTL_PMCF = 1 << 23,  // Pass MAC Control Frames
    RCTL_SECRC = 1 << 26, // Strip Ethernet CRC

    RCTL_BSIZE_256 = 3 << 16,
    RCTL_BSIZE_512 = 2 << 16,
    RCTL_BSIZE_1024 = 1 << 16,
    RCTL_BSIZE_2048 = 0 << 16,
    RCTL_BSIZE_4096 = (3 << 16) | (1 << 25),
    RCTL_BSIZE_8192 = (2 << 16) | (1 << 25),
    RCTL_BSIZE_16384 = (1 << 16) | (1 << 25),
};

// 传输控制
enum TCTL
{
    TCTL_EN = 1 << 1,      // Transmit Enable
    TCTL_PSP = 1 << 3,     // Pad Short Packets
    TCTL_CT = 4,           // Collision Threshold
    TCTL_COLD = 12,        // Collision Distance
    TCTL_SWXOFF = 1 << 22, // Software XOFF Transmission
    TCTL_RTLC = 1 << 24,   // Re-transmit on Late Collision
    TCTL_NRTU = 1 << 25,   // No Re-transmit on underrun
};

// 接收状态
enum RS
{
    RS_DD = 1 << 0,    // Descriptor done
    RS_EOP = 1 << 1,   // End of packet
    RS_VP = 1 << 3,    // Packet is 802.1q (matched VET);
                       // indicates strip VLAN in 802.1q packet
    RS_UDPCS = 1 << 4, // UDP checksum calculated on packet
    RS_L4CS = 1 << 5,  // L4 (UDP or TCP) checksum calculated on packet
    RS_IPCS = 1 << 6,  // Ipv4 checksum calculated on packet
    RS_PIF = 1 << 7,   // Passed in-exact filter
};

// 传输命令
enum TCMD
{
    TCMD_EOP = 1 << 0,  // End of Packet
    TCMD_IFCS = 1 << 1, // Insert FCS
    TCMD_IC = 1 << 2,   // Insert Checksum
    TCMD_RS = 1 << 3,   // Report Status
    TCMD_RPS = 1 << 4,  // Report Packet Sent
    TCMD_VLE = 1 << 6,  // VLAN Packet Enable
    TCMD_IDE = 1 << 7,  // Interrupt Delay Enable
};

// 发送状态
enum TS
{
    TS_DD = 1 << 0, // Descriptor Done
    TS_EC = 1 << 1, // Excess Collisions
    TS_LC = 1 << 2, // Late Collision
    TS_TU = 1 << 3, // Transmit Underrun
};

// 错误
enum ERR
{
    ERR_CE = 1 << 0,   // CRC Error or Alignment Error
    ERR_SE = 1 << 1,   // Symbol Error
    ERR_SEQ = 1 << 2,  // Sequence Error
    ERR_RSV = 1 << 3,  // Reserved
    ERR_CXE = 1 << 4,  // Carrier Extension Error
    ERR_TCPE = 1 << 5, // TCP/UDP Checksum Error
    ERR_IPE = 1 << 6,  // IP Checksum Error
    ERR_RXE = 1 << 7,  // RX Data Error
};

// 接收描述符
typedef struct rx_desc_t
{
    u64 addr;     // 地址
    u16 length;   // 长度
    u16 checksum; // 校验和
    u8 status;    // 状态
    u8 error;     // 错误
    u16 special;  // 特殊
} _packed rx_desc_t;

// 传输描述符
typedef struct tx_desc_t
{
    u64 addr;    // 缓冲区地址
    u16 length;  // 包长度
    u8 cso;      // Checksum Offset
    u8 cmd;      // 命令
    u8 status;   // 状态
    u8 css;      // Checksum Start Field
    u16 special; // 特殊
} _packed tx_desc_t;

#define NAME_LEN 16
#define RX_DESC_NR 32 // 接收描述符数量
#define TX_DESC_NR 32 // 传输描述符数量

typedef struct e1000_t
{
    char name[NAME_LEN]; // 名称

    pci_device_t *device; // PCI 设备
    u32 membase;          // 映射内存基地址

    u8 mac[6];   // MAC 地址
    bool link;   // 网络连接状态
    bool eeprom; // 只读存储器可用

    rx_desc_t *rx_desc; // 接收描述符
    u16 rx_cur;         // 接收描述符指针

    tx_desc_t *tx_desc; // 传输描述符
    u16 tx_cur;         // 传输描述符指针
    task_t *tx_waiter;  // 传输等待进程

    pbuf_t **rx_pbuf; // 接收高速缓冲数组
    pbuf_t **tx_pbuf; // 传输高速缓冲数组

    netif_t *netif; // 虚拟网卡
} e1000_t;

static e1000_t obj;

// 接收数据包
static void recv_packet(e1000_t *e1000)
{
    while (true)
    {
        rx_desc_t *rx = &e1000->rx_desc[e1000->rx_cur];
        // LOGK("rx status 0x%X\n", rx->status);
        if (!(rx->status & RS_DD))
        {
            return;
        }

        if (rx->error)
        {
            LOGK("error 0x%X happened...\n", rx->error);
        }

        assert(rx->length < 1600);

        pbuf_t *pbuf = e1000->rx_pbuf[e1000->rx_cur];
        assert(pbuf == element_entry(pbuf_t, payload, rx->addr));

        pbuf->length = rx->length;

        // 将数据包加入缓冲队列
        netif_input(e1000->netif, pbuf);

        pbuf = pbuf_get();

        e1000->rx_pbuf[e1000->rx_cur] = pbuf;
        rx->addr = get_paddr((u32)pbuf->payload);
        rx->status = 0;

        moutl(e1000->membase + E1000_RDT, e1000->rx_cur);
        e1000->rx_cur = (e1000->rx_cur + 1) % RX_DESC_NR;
    }
}

static void send_packet(netif_t *netif, pbuf_t *pbuf)
{
    e1000_t *e1000 = netif->nic;
    tx_desc_t *tx = &e1000->tx_desc[e1000->tx_cur];
    while (tx->status == 0)
    {
        assert(e1000->tx_waiter == NULL);
        e1000->tx_waiter = running_task();
        assert(task_block(e1000->tx_waiter, NULL, TASK_BLOCKED, TIMELESS) == EOK);
    }

    assert(pbuf->count <= 2);

    // Ethernet checksum
    // u32 sum = eth_fcs((char *)pbuf->payload, pbuf->length);
    // *(u32 *)((u32)pbuf->payload + pbuf->length) = sum;
    // pbuf->length += ETH_FCS_LEN;

    e1000->tx_pbuf[e1000->tx_cur] = pbuf;

    tx->addr = get_paddr((u32)pbuf->payload);
    tx->length = pbuf->length;
    tx->cmd = TCMD_EOP | TCMD_RS | TCMD_RPS | TCMD_IFCS;
    tx->status = 0;

    e1000->tx_cur = (e1000->tx_cur + 1) % TX_DESC_NR;
    moutl(e1000->membase + E1000_TDT, e1000->tx_cur);

    // LOGK("ETH S 0x%p [0x%04X]: %m -> %m, %d\n",
    //      pbuf,
    //      ntohs(pbuf->eth->type),
    //      pbuf->eth->src,
    //      pbuf->eth->dst,
    //      pbuf->length);
}

static void free_packet(e1000_t *e1000)
{
    u32 cur = e1000->tx_cur;
    while (true)
    {
        cur = (cur - 1) % TX_DESC_NR;
        tx_desc_t *tx = &e1000->tx_desc[cur];
        if (!tx->addr) // 没有高速缓冲
            break;
        if (tx->status == 0) // 发送未结束
            break;

        // 释放高速缓冲
        pbuf_t *pbuf = e1000->tx_pbuf[cur];
        assert(pbuf == element_entry(pbuf_t, payload, e1000->tx_desc[cur].addr));

        e1000->tx_pbuf[cur] = NULL;
        e1000->tx_desc[cur].addr = 0;
        pbuf_put(pbuf);
    }

    if (e1000->tx_waiter)
    {
        task_unblock(e1000->tx_waiter, EOK);
        e1000->tx_waiter = NULL;
    }
}

// 中断处理函数
static void e1000_handler(int vector)
{
    assert(vector == IRQ_NIC + 0x20);

    e1000_t *e1000 = &obj;

    u32 status = minl(e1000->membase + E1000_ICR);
    // LOGK("e1000 interrupt fired status %X\n", status);

    // 传输描述符写回，表示有一个数据包发送完毕
    if ((status & IM_TXDW))
    {
        LOGK("e1000 TXDW...\n");
        free_packet(e1000);
    }

    // 传输队列为空，并且传输进程阻塞
    if ((status & IM_TXQE))
    {
        // LOGK("e1000 TXQE...\n");
    }

    // 连接状态改变
    if (status & IM_LSC)
    {
        LOGK("e1000 link state changed...\n");
        // u32 e1000_status = minl(e1000->membase +  E1000_STATUS);
        // e1000->link = (e1000_status & STATUS_LU != 0);
    }

    // Overrun
    if (status & IM_RXO)
    {
        LOGK("e1000 RXO...\n");
        // overrun
    }

    if (status & IM_RXDMT0)
    {
        LOGK("e1000 RXDMT0...\n");
    }

    if (status & IM_RXT0)
    {
        recv_packet(e1000);
    }

    // 去掉已知中断状态，其他的如果发生再说；
    status &= ~IM_TXDW & ~IM_TXQE & ~IM_LSC & ~IM_RXO;
    status &= ~IM_RXDMT0 & ~IM_RXT0;
    if (status)
    {
        LOGK("e1000 status unhandled %d\n", status);
        hang();
    }

    // 必须在处理完成之后发送中断处理完成信号，否则会有意想不到的中断发生
    // 参考 https://wiki.osdev.org/8259_PIC#Spurious_IRQs
    send_eoi(vector);
}

// 检测只读存储器
static void e1000_eeprom_detect(e1000_t *e1000)
{
    moutl(e1000->membase + E1000_EERD, 0x1);
    for (size_t i = 0; i < 1000; i++)
    {
        u32 val = minl(e1000->membase + E1000_EERD);
        if (val & 0x10)
            e1000->eeprom = true;
        else
            e1000->eeprom = false;
    }
}

// 读取只读存储器
static u16 e1000_eeprom_read(e1000_t *e1000, u8 addr)
{
    u32 tmp;
    if (e1000->eeprom)
    {
        moutl(e1000->membase + E1000_EERD, 1 | (u32)addr << 8);
        while (!((tmp = minl(e1000->membase + E1000_EERD)) & (1 << 4)))
            ;
    }
    else
    {
        moutl(e1000->membase + E1000_EERD, 1 | (u32)addr << 2);
        while (!((tmp = minl(e1000->membase + E1000_EERD)) & (1 << 1)))
            ;
    }
    return (tmp >> 16) & 0xFFFF;
}

// 读取 MAC 地址
static void e1000_read_mac(e1000_t *e1000)
{
    e1000_eeprom_detect(e1000);
    if (e1000->eeprom)
    {
        u16 val;
        val = e1000_eeprom_read(e1000, 0);
        e1000->mac[0] = val & 0xFF;
        e1000->mac[1] = val >> 8;

        val = e1000_eeprom_read(e1000, 1);
        e1000->mac[2] = val & 0xFF;
        e1000->mac[3] = val >> 8;

        val = e1000_eeprom_read(e1000, 2);
        e1000->mac[4] = val & 0xFF;
        e1000->mac[5] = val >> 8;
    }
    else
    {
        char *mac = (char *)e1000->membase + 0x5400;
        for (int i = 5; i >= 0; i--)
        {
            e1000->mac[i] = mac[i];
        }
    }

    LOGK("E1000 MAC: %m\n", e1000->mac);
}

// 重置网卡
static void e1000_reset(e1000_t *e1000)
{
    e1000_read_mac(e1000);

    // 初始化组播表数组
    for (int i = E1000_MAT0; i < E1000_MAT1; i += 4)
        moutl(e1000->membase + i, 0);

    // 禁用中断
    moutl(e1000->membase + E1000_IMS, 0);

    // 接收初始化
    e1000->rx_desc = (rx_desc_t *)alloc_kpage(1); // TODO: free
    memset(e1000->rx_desc, 0, PAGE_SIZE);
    e1000->rx_cur = 0;

    e1000->rx_pbuf = (pbuf_t **)&e1000->rx_desc[RX_DESC_NR];

    moutl(e1000->membase + E1000_RDBAL, (u32)e1000->rx_desc);
    moutl(e1000->membase + E1000_RDBAH, 0);
    moutl(e1000->membase + E1000_RDLEN, sizeof(rx_desc_t) * RX_DESC_NR);

    // 接收描述符头尾指针
    moutl(e1000->membase + E1000_RDH, 0);
    moutl(e1000->membase + E1000_RDT, RX_DESC_NR - 1);

    // 接收描述符地址
    for (size_t i = 0; i < RX_DESC_NR; i++)
    {
        pbuf_t *pbuf = pbuf_get();
        e1000->rx_pbuf[i] = pbuf;
        e1000->rx_desc[i].addr = get_paddr((u32)pbuf->payload);
        e1000->rx_desc[i].status = 0;
    }

    // 接收控制寄存器
    u32 value = 0;
    value |= RCTL_EN;
    // value |= RCTL_SBP | RCTL_UPE | RCTL_MPE; // 混杂模式
    value |= RCTL_LBM_NONE | RTCL_RDMTS_HALF;
    value |= RCTL_BAM | RCTL_SECRC | RCTL_BSIZE_2048;
    moutl(e1000->membase + E1000_RCTL, value);

    // 传输初始化
    e1000->tx_desc = (tx_desc_t *)alloc_kpage(1); // TODO:free
    memset(e1000->tx_desc, 0, PAGE_SIZE);
    e1000->tx_cur = 0;

    e1000->tx_pbuf = (pbuf_t **)&e1000->tx_desc[TX_DESC_NR];

    moutl(e1000->membase + E1000_TDBAL, (u32)e1000->tx_desc);
    moutl(e1000->membase + E1000_TDBAH, 0);
    moutl(e1000->membase + E1000_TDLEN, sizeof(tx_desc_t) * TX_DESC_NR);

    // 传输描述符头尾指针
    moutl(e1000->membase + E1000_TDH, 0);
    moutl(e1000->membase + E1000_TDT, 0);

    // 传输描述符基地址
    for (size_t i = 0; i < TX_DESC_NR; i++)
    {
        e1000->tx_desc[i].addr = 0;
        e1000->tx_desc[i].status = TS_DD;
    }

    // 传输控制寄存器
    value = 0;
    value |= TCTL_EN | TCTL_PSP | TCTL_RTLC;
    value |= 0x10 << TCTL_CT;
    value |= 0x40 << TCTL_COLD;
    moutl(e1000->membase + E1000_TCTL, value);

    // 初始化中断
    value = 0;
    value = IM_RXT0 | IM_RXO | IM_RXDMT0 | IM_RXSEQ | IM_LSC;
    value |= IM_TXQE | IM_TXDW | IM_TXDLOW;
    moutl(e1000->membase + E1000_IMS, value);
}

// 查找网卡设备
static pci_device_t *find_e1000_device()
{
    pci_device_t *device = NULL;

    for (size_t i = DEVICEID_LOW; i <= DEVICEID_HIGH; i++)
    {
        device = pci_find_device(VENDORID, i);
        if (device)
            break;
    }
    return device;
}

// 初始化 e1000
void e1000_init()
{
    pci_device_t *device = find_e1000_device();
    if (!device)
    {
        LOGK("PCI e1000 ethernet card not exists...\n");
        return;
    }

    e1000_t *e1000 = &obj;
    e1000->tx_waiter = NULL;

    strcpy(e1000->name, "e1000");

    e1000->device = device;

    pci_enable_busmastering(device);

    pci_bar_t membar;
    err_t ret = pci_find_bar(device, &membar, PCI_BAR_TYPE_MEM);
    assert(ret == EOK);

    LOGK("e1000 membase 0x%x size 0x%x\n", membar.iobase, membar.size);

    // 映射内存因该在高地址，避免与用户程序地址和操作系统冲突
    // 但是不在最后 4M 内，最后 4M 用于页表
    // 不然内存映射机制有问题，需要调整，不能直接使用 map_area
    assert(membar.iobase < 0xFFC00000 && membar.iobase >= 0xF0000000);

    // 映射物理内存区域
    e1000->membase = membar.iobase;
    map_area(membar.iobase, membar.size);

    e1000_reset(e1000);

    e1000->netif = netif_setup(e1000, e1000->mac, send_packet);

    u32 intr = pci_interrupt(device);

    LOGK("e1000 irq 0x%X...\n", intr);
    assert(intr == IRQ_NIC);

    // 设置中断处理函数
    set_interrupt_handler(intr, e1000_handler);
    set_interrupt_mask(intr, true);
    if (intr >= 8)
        set_interrupt_mask(IRQ_CASCADE, true);
}