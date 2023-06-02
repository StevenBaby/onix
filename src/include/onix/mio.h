#ifndef ONIX_MIO_H
#define ONIX_MIO_H

#include <onix/types.h>

// 映射内存 IO

// 内存输入字节 8bit
u8 minb(u32 addr);
// 内存输入字 16bit
u16 minw(u32 addr);
// 内存输入双字 32bit
u32 minl(u32 addr);


// 内存输出字节
void moutb(u32 addr, u8 value);
// 内存输出字
void moutw(u32 addr, u16 value);
// 内存输出双字
void moutl(u32 addr, u32 value);

#endif