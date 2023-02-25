#include <onix/stdarg.h>
#include <onix/console.h>
#include <onix/stdio.h>
#include <onix/device.h>

static char buf[1024];

int printk(const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);

    i = vsprintf(buf, fmt, args);

    va_end(args);

    device_t *device = device_find(DEV_CONSOLE, 0);
    device_write(device->dev, buf, i, 0, 0);

    return i;
}
