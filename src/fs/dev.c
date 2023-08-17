#include <onix/device.h>
#include <onix/syscall.h>
#include <onix/stat.h>
#include <onix/stdio.h>
#include <onix/assert.h>
#include <onix/fs.h>
#include <onix/string.h>
#include <onix/net.h>

extern file_t file_table[];

void net_init()
{
    bool dhcp = true;

    fd_t netif = open("/dev/net/eth1", O_RDONLY, 0);
    if (netif < EOK)
        return;

    fd_t fd = open("/etc/network.conf", O_RDONLY, 0);
    if (fd < EOK)
        goto rollback;

    char buf[512];
    int len = read(fd, buf, sizeof(buf));
    if (len < EOK)
        goto rollback;

    ifreq_t req;

    char *next = buf - 1;

    while (true)
    {
        if (!next)
            break;

        char *ptr = next + 1;
        next = strchr(ptr, '\n');
        if (next)
            *next = 0;

        if (!memcmp(ptr, "ipaddr=", 7))
        {
            if (inet_aton(ptr + 7, req.ipaddr) < EOK)
                continue;
            ioctl(netif, SIOCSIFADDR, (int)&req);
            req.ipaddr[3] = 255;
            ioctl(netif, SIOCSIFBRDADDR, (int)&req);
            dhcp = false;
        }
        else if (!memcmp(ptr, "netmask=", 8))
        {
            if (inet_aton(ptr + 8, req.netmask) < EOK)
                continue;
            ioctl(netif, SIOCSIFNETMASK, (int)&req);
        }
        else if (!memcmp(ptr, "gateway=", 8))
        {
            if (inet_aton(ptr + 8, req.gateway) < EOK)
                continue;
            ioctl(netif, SIOCSIFGATEWAY, (int)&req);
        }
    }
rollback:
    if (dhcp && netif > 0)
        ioctl(netif, SIOCSIFDHCPSTART, (int)&req);

    if (netif > 0)
        close(netif);
    if (fd > 0)
        close(fd);
}

void dev_init()
{
    stat_t statbuf;
    if (stat("/dev", &statbuf) < 0)
    {
        assert(mkdir("/dev", 0755) == EOK);
    }

    device_t *device = NULL;

    // 第一个虚拟磁盘作为 /dev 文件系统
    device = device_find(DEV_RAMDISK, 0);
    assert(device);
    fs_get_op(FS_TYPE_MINIX)->mkfs(device->dev, 0);

    super_t *super = read_super(device->dev);
    assert(super->iroot);
    assert(super->iroot->nr == 1);
    super->imount = namei("/dev");
    assert(super->imount);
    super->imount->mount = device->dev;

    device = device_find(DEV_CONSOLE, 0);
    assert(device);
    assert(mknod("/dev/console", IFCHR | 0200, device->dev) == EOK);

    device = device_find(DEV_KEYBOARD, 0);
    assert(device);
    assert(mknod("/dev/keyboard", IFCHR | 0400, device->dev) == EOK);

    device = device_find(DEV_MOUSE, 0);
    if (device)
        mknod("/dev/mouse", IFCHR | 0400, device->dev);

    device = device_find(DEV_TTY, 0);
    assert(device);
    assert(mknod("/dev/tty", IFCHR | 0600, device->dev) == EOK);

    char name[32];

    for (size_t i = 0; true; i++)
    {
        device = device_find(DEV_IDE_DISK, i);
        if (!device)
            break;
        sprintf(name, "/dev/%s", device->name);
        assert(mknod(name, IFBLK | 0600, device->dev) == EOK);
    }

    for (size_t i = 0; true; i++)
    {
        device = device_find(DEV_IDE_PART, i);
        if (!device)
        {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        assert(mknod(name, IFBLK | 0600, device->dev) == EOK);
    }

    for (size_t i = 0; true; i++)
    {
        device = device_find(DEV_IDE_CD, i);
        if (!device)
        {
            break;
        }
        sprintf(name, "/dev/cd%d", i);
        mknod(name, IFBLK | 0400, device->dev);
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
        assert(mknod(name, IFBLK | 0600, device->dev) == EOK);
    }

    for (size_t i = 0; true; i++)
    {
        device = device_find(DEV_FLOPPY, i);
        if (!device)
        {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        assert(mknod(name, IFBLK | 0600, device->dev) == EOK);
    }

    for (size_t i = 0; true; i++)
    {
        device = device_find(DEV_SERIAL, i);
        if (!device)
        {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        assert(mknod(name, IFCHR | 0600, device->dev) == EOK);
    }

    for (size_t i = 0; true; i++)
    {
        device = device_find(DEV_SB16, i);
        if (!device)
        {
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        assert(mknod(name, IFCHR | 0200, device->dev) == EOK);
    }

    assert(mkdir("/dev/net", 0755) == EOK);
    for (size_t i = 0; true; i++)
    {
        device = device_find(DEV_NETIF, i);
        if (!device)
        {
            break;
        }
        sprintf(name, "/dev/net/%s", device->name);
        mknod(name, IFSOCK | 0400, device->dev);
    }

    assert(link("/dev/tty", "/dev/stdout") == EOK);
    assert(link("/dev/tty", "/dev/stderr") == EOK);
    assert(link("/dev/tty", "/dev/stdin") == EOK);

    file_t *file;
    inode_t *inode;
    file = &file_table[STDIN_FILENO];
    inode = namei("/dev/stdin");
    assert(inode);
    file->inode = inode;
    file->flags = O_RDONLY;
    file->offset = 0;

    file = &file_table[STDOUT_FILENO];
    inode = namei("/dev/stdout");
    assert(inode);
    file->inode = inode;
    file->flags = O_WRONLY;
    file->offset = 0;

    file = &file_table[STDERR_FILENO];
    inode = namei("/dev/stderr");
    assert(inode);
    file->inode = inode;
    file->flags = O_WRONLY;
    file->offset = 0;
}