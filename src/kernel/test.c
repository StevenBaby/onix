#include <onix/types.h>
#include <onix/cpu.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/errno.h>
#include <onix/string.h>
#include <onix/stdio.h>
#include <onix/device.h>
#include <onix/memory.h>
#include <onix/net.h>
#include <onix/syscall.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern void vesa_show();
extern void vga_show();

err_t sys_test()
{
    return EOK;
}
