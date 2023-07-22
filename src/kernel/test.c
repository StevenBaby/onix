#include <onix/types.h>
#include <onix/cpu.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/errno.h>
#include <onix/string.h>
#include <onix/net.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

err_t sys_test()
{
    return EOK;
}
