#include <onix/mio.h>

u8 minb(u32 addr)
{
    return *((volatile u8 *)addr);
}

u16 minw(u32 addr)
{
    return *((volatile u16 *)addr);
}

u32 minl(u32 addr)
{
    return *((volatile u32 *)addr);
}

void moutb(u32 addr, u8 value)
{
    *((volatile u8 *)addr) = value;
}

void moutw(u32 addr, u16 value)
{
    *((volatile u16 *)addr) = value;
}

void moutl(u32 addr, u32 value)
{
    *((volatile u32 *)addr) = value;
}
