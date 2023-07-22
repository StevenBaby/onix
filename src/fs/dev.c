#include <onix/device.h>
#include <onix/syscall.h>
#include <onix/stat.h>
#include <onix/stdio.h>
#include <onix/assert.h>
#include <onix/fs.h>

extern file_t file_table[];

void dev_init()
{
    mkdir("/dev", 0755);

    device_t *device = NULL;

    // 第一个虚拟磁盘作为 /dev 文件系统
    device = device_find(DEV_RAMDISK, 0);
    assert(device);
    fs_get_op(FS_TYPE_MINIX)->mkfs(device->dev, 0);

    super_t *super = read_super(device->dev);
    assert(super->iroot);
    super->imount = namei("/dev");
    super->imount->mount = device->dev;

    device = device_find(DEV_CONSOLE, 0);
    mknod("/dev/console", IFCHR | 0200, device->dev);

    device = device_find(DEV_KEYBOARD, 0);
    mknod("/dev/keyboard", IFCHR | 0400, device->dev);

    device = device_find(DEV_TTY, 0);
    mknod("/dev/tty", IFCHR | 0600, device->dev);

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

    // 防止误操作，去掉 mda，保护 dev 文件系统
    for (size_t i = 1; true; i++)
    {
        device = device_find(DEV_RAMDISK, i);
        if (!device)
        {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFBLK | 0600, device->dev);
    }

    for (size_t i = 0; true; i++)
    {
        device = device_find(DEV_FLOPPY, i);
        if (!device)
        {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFBLK | 0600, device->dev);
    }

    for (size_t i = 0; true; i++)
    {
        device = device_find(DEV_SERIAL, i);
        if (!device)
        {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFCHR | 0600, device->dev);
    }

    for (size_t i = 0; true; i++)
    {
        device = device_find(DEV_SB16, i);
        if (!device)
        {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        mknod(name, IFCHR | 0200, device->dev);
    }

    link("/dev/tty", "/dev/stdout");
    link("/dev/tty", "/dev/stderr");
    link("/dev/tty", "/dev/stdin");

    file_t *file;
    inode_t *inode;
    file = &file_table[STDIN_FILENO];
    inode = namei("/dev/stdin");
    file->inode = inode;
    file->flags = O_RDONLY;
    file->offset = 0;

    file = &file_table[STDOUT_FILENO];
    inode = namei("/dev/stdout");
    file->inode = inode;
    file->flags = O_WRONLY;
    file->offset = 0;

    file = &file_table[STDERR_FILENO];
    inode = namei("/dev/stderr");
    file->inode = inode;
    file->flags = O_WRONLY;
    file->offset = 0;
}