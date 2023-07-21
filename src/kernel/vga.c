#include <onix/types.h>
#include <onix/stdlib.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define MEM_BASE 0xA0000
#define MEM_SIZE 0x10000

#define WIDTH 320
#define HEIGHT 200

void vga_reset()
{
    for (size_t i = MEM_BASE; i < (MEM_BASE + WIDTH * HEIGHT); i++)
    {
        *(char *)i = i & 0x0F;
    }
}

void vga_init()
{
}