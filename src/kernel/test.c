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

int sys_test()
{
    char ch;
    device_t *device;

    device = device_find(DEV_IDE_CD, 0);
    if (!device)
        return 0;

    void *buf = (void *)alloc_kpage(1);
    memset(buf, 0, PAGE_SIZE);
    device_read(device->dev, buf, 2, 0, 0);
    free_kpage((u32)buf, 1);
    return EOK;
}
