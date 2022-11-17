#include <onix/device.h>
#include <onix/syscall.h>
#include <onix/stat.h>
#include <onix/stdio.h>

void dev_init()
{
    mkdir("/dev", 0755);

    device_t *device = NULL;

    device = device_find(DEV_CONSOLE, 0);
    mknod("/dev/console", IFCHR | 0200, device->dev);

    device = device_find(DEV_KEYBOARD, 0);
    mknod("/dev/keyboard", IFCHR | 0400, device->dev);

    char name[32];

    for (size_t i = 0; true; i++)
    {
        device = device_find(DEV_IDE_DISK, i);
        if (!device)
            break;
        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFBLK | 0600, device->dev);
    }

    for (size_t i = 0; true; i++)
    {
        device = device_find(DEV_IDE_PART, i);
        if (!device)
        {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFBLK | 0600, device->dev);
    }
}