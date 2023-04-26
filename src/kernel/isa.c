#include <onix/isa.h>
#include <onix/assert.h>
#include <onix/io.h>

enum
{
    DMA0_CHAN0_ADDR = 0x00,  // 通道 0 起始地址寄存器 (未使用)
    DMA0_CHAN0_COUNT = 0x01, // 通道 0 计数寄存器 (未使用)
    DMA0_CHAN1_ADDR = 0x02,  // 通道 1 起始地址寄存器
    DMA0_CHAN1_COUNT = 0x03, // 通道 1 计数寄存器
    DMA0_CHAN2_ADDR = 0x04,  // 通道 2 起始地址寄存器
    DMA0_CHAN2_COUNT = 0x05, // 通道 2 计数寄存器
    DMA0_CHAN3_ADDR = 0x06,  // 通道 3 起始地址寄存器
    DMA0_CHAN3_COUNT = 0x07, // 通道 3 计数寄存器

    DMA0_STATUS = 0x08,       // 主 DMA 状态寄存器
    DMA0_COMMAND = 0x08,      // 主 DMA 命令寄存器
    DMA0_REQUEST = 0x09,      // 主 DMA 请求寄存器
    DMA0_MASK1 = 0x0A,        // 主 DMA 单通道掩码寄存器
    DMA0_MODE = 0x0B,         // 主 DMA 模式寄存器
    DMA0_RESET = 0x0C,        // 主 DMA 触发器重置寄存器
    DMA0_TEMP = 0x0d,         // 主 DMA 中间寄存器
    DMA0_MASTER_CLEAR = 0x0d, // 主 DMA 主控重置寄存器
    DMA0_MASK_CLEAR = 0x0E,   // 主 DMA 掩码重置寄存器
    DMA0_MASK2 = 0x0F,        // 主 DMA 多通道掩码寄存器

    DMA1_CHAN4_ADDR = 0XC0,  // 通道 4 起始地址寄存器 (未使用)
    DMA1_CHAN4_COUNT = 0XC2, // 通道 4 计数寄存器 (未使用)
    DMA1_CHAN5_ADDR = 0XC4,  // 通道 5 起始地址寄存器
    DMA1_CHAN5_COUNT = 0XC6, // 通道 5 计数寄存器
    DMA1_CHAN6_ADDR = 0XC8,  // 通道 6 起始地址寄存器
    DMA1_CHAN6_COUNT = 0XCA, // 通道 6 计数寄存器
    DMA1_CHAN7_ADDR = 0XCC,  // 通道 7 起始地址寄存器
    DMA1_CHAN7_COUNT = 0XCE, // 通道 7 计数寄存器

    DMA1_STATUS = 0xD0,       // 从 DMA 状态寄存器
    DMA1_COMMAND = 0xD0,      // 从 DMA 命令寄存器
    DMA2_REQUEST = 0xD2,      // 从 DMA 请求寄存器
    DMA1_MASK1 = 0xD4,        // 从 DMA 单通道掩码寄存器
    DMA1_MODE = 0xD6,         // 从 DMA 模式寄存器
    DMA1_RESET = 0xD8,        // 从 DMA 触发器重置寄存器
    DMA1_TEMP = 0xDA,         // 从 DMA 中间寄存器
    DMA1_MASTER_CLEAR = 0xDA, // 从 DMA 主控重置寄存器
    DMA1_MASK_CLEAR = 0xDC,   // 从 DMA 掩码重置寄存器
    DMA1_MASK2 = 0xDE,        // 从 DMA 多通道掩码寄存器

    DMA0_CHAN1_PAGE = 0x83, // 通道 1 页地址寄存器
    DMA0_CHAN2_PAGE = 0x81, // 通道 2 页地址寄存器
    DMA0_CHAN3_PAGE = 0x82, // 通道 3 页地址寄存器

    DMA1_CHAN5_PAGE = 0x8B, // 通道 5 页地址寄存器
    DMA1_CHAN6_PAGE = 0x89, // 通道 6 页地址寄存器
    DMA1_CHAN7_PAGE = 0x8A, // 通道 7 页地址寄存器
};

// 各通道掩码寄存器
static u8 DMA_MASK1[] = {
    DMA0_MASK1,
    DMA0_MASK1,
    DMA0_MASK1,
    DMA0_MASK1,
    DMA1_MASK1,
    DMA1_MASK1,
    DMA1_MASK1,
    DMA1_MASK1,
};

// 各通道模式寄存器
static u8 DMA_MODE[] = {
    DMA0_MODE,
    DMA0_MODE,
    DMA0_MODE,
    DMA0_MODE,
    DMA1_MODE,
    DMA1_MODE,
    DMA1_MODE,
    DMA1_MODE,
};

// 各通道触发器重置寄存器
static u8 DMA_RESET[] = {
    DMA0_RESET,
    DMA0_RESET,
    DMA0_RESET,
    DMA0_RESET,
    DMA1_RESET,
    DMA1_RESET,
    DMA1_RESET,
    DMA1_RESET,
};

// 各通道起始地址寄存器
static u8 DMA_ADDR[] = {
    DMA0_CHAN0_ADDR,
    DMA0_CHAN1_ADDR,
    DMA0_CHAN2_ADDR,
    DMA0_CHAN3_ADDR,
    DMA1_CHAN4_ADDR,
    DMA1_CHAN5_ADDR,
    DMA1_CHAN6_ADDR,
    DMA1_CHAN7_ADDR,
};

// 各通道计数寄存器
static u8 DMA_COUNT[] = {
    DMA0_CHAN0_COUNT,
    DMA0_CHAN1_COUNT,
    DMA0_CHAN2_COUNT,
    DMA0_CHAN3_COUNT,
    DMA1_CHAN4_COUNT,
    DMA1_CHAN5_COUNT,
    DMA1_CHAN6_COUNT,
    DMA1_CHAN7_COUNT,
};

// 页地址寄存器
static u8 DMA_PAGE[] = {
    0,
    DMA0_CHAN1_PAGE,
    DMA0_CHAN2_PAGE,
    DMA0_CHAN3_PAGE,
    0,
    DMA1_CHAN5_PAGE,
    DMA1_CHAN6_PAGE,
    DMA1_CHAN7_PAGE,
};

#define ISA_DMA_CHAN2_READ 0x46
#define ISA_DMA_CHAN2_WRITE 0x4A

#define ISA_DMA0_CHAN2_PAGE 0x81

// 设置 DMA 掩码
void isa_dma_mask(u8 channel, bool mask)
{
    assert(channel < 8);
    u16 port = DMA_MASK1[channel];
    u8 data = channel % 4;
    if (!mask)
    {
        data |= 0x4;
    }
    outb(port, data);
}

// 设置起始地址
void isa_dma_addr(u8 channel, void *addr)
{
    assert(channel < 8);
    u16 port = DMA_ADDR[channel];

    u32 offset = ((u32)addr) % 0x10000;
    if (channel >= 5)
    {
        offset >>= 1;
    }
    outb(port, offset & 0xFF);
    outb(port, (offset >> 8) & 0xFF);

    port = DMA_PAGE[channel];
    outb(port, (u32)addr >> 16);
}

// 设置传输大小
void isa_dma_size(u8 channel, u32 size)
{
    assert(channel < 8);
    u16 port = DMA_COUNT[channel];
    if (channel >= 5)
    {
        size >>= 1;
    }

    outb(port, (size - 1) & 0xFF);
    outb(port, ((size - 1) >> 8) & 0xFF);
}

// 设置 DMA 模式
void isa_dma_mode(u8 channel, u8 mode)
{
    assert(channel < 8);
    u16 port = DMA_MODE[channel];
    outb(port, mode | (channel % 4));
}

// 重置 DMA
void isa_dma_reset(u8 channel)
{
    assert(channel < 8);
    u16 port = DMA_RESET[channel];
    outb(port, 0);
}
