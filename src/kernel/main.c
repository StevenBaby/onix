#include <onix/onix.h>
#include <onix/types.h>
#include <onix/io.h>

// - CRT 地址寄存器 0x3D4
// - CRT 数据寄存器 0x3D5
// - CRT 光标位置 - 高位 0xE
// - CRT 光标位置 - 低位 0xF

#define CRT_ADDR_REG 0x3d4
#define CRT_DATA_REG 0x3d5

#define CRT_CURSOR_H 0xe
#define CRT_CURSOR_L 0xf

void kernel_init()
{
    outb(CRT_ADDR_REG, CRT_CURSOR_H);
    u16 pos = inb(CRT_DATA_REG) << 8;
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    pos |= inb(CRT_DATA_REG);

    outb(CRT_ADDR_REG, CRT_CURSOR_H);
    outb(CRT_DATA_REG, 0);
    outb(CRT_ADDR_REG, CRT_CURSOR_L);
    outb(CRT_DATA_REG, 123);

    return;
}
