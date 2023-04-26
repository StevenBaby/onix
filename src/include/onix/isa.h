#ifndef ONIX_ISA_H
#define ONIX_ISA_H

#include <onix/types.h>

#define DMA_MODE_CHECK 0x00   // 自检模式
#define DMA_MODE_READ 0x04    // 外部设备读出（写内存）
#define DMA_MODE_WRITE 0x08   // 写入外部设备（读内存）
#define DMA_MODE_AUTO 0x10    // 自动模式
#define DMA_MODE_DOWN 0x20    // 从高地址向低地址访问内存
#define DMA_MODE_DEMAND 0x00  // 按需传输；
#define DMA_MODE_SINGLE 0x40  // 单次 DMA 传输；
#define DMA_MODE_BLOCK 0x80   // 块 DMA 传输；
#define DMA_MODE_CASCADE 0xC0 // 级联模式(用于级联另一个 DMA 控制器)；

void isa_dma_mask(u8 channel, bool mask);
void isa_dma_addr(u8 channel, void *addr);
void isa_dma_size(u8 channel, u32 size);
void isa_dma_reset(u8 channel);
void isa_dma_mode(u8 channel, u8 mode);

#endif
