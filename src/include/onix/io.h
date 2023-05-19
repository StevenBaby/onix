#ifndef ONIX_IO_H
#define ONIX_IO_H

#include <onix/types.h>

extern u8 inb(u16 port);  // 输入一个字节
extern u16 inw(u16 port); // 输入一个字
extern u32 inl(u16 port); // 输入一个双字

extern void outb(u16 port, u8 value);  // 输出一个字节
extern void outw(u16 port, u16 value); // 输出一个字
extern void outl(u16 port, u32 value); // 输出一个双字

#define io_mfence() __asm__ __volatile__("mfence":::"memory")

#endif