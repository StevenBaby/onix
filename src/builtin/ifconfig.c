#include <onix/types.h>
#include <onix/stdio.h>
#include <onix/syscall.h>
#include <onix/string.h>
#include <onix/fs.h>
#include <onix/net.h>

#define BUFLEN 0x1000

static char buf[BUFLEN];

int main(int argc, char const *argv[])
{
    ifreq_t ifreq;

    fd_t dir = open("/dev/net", O_RDONLY, 0);
    if (dir < EOK)
        return EOF;

    if (lseek(dir, 0, SEEK_SET) != 0)
        goto rollback;

    fd_t netif = -1;
    char filename[256];

    dentry_t entry;
    while (true)
    {
        int len = readdir(dir, &entry, 1);
        if (len < EOK)
            break;
        if (!entry.nr)
            continue;
        if (!strcmp(entry.name, ".") || !strcmp(entry.name, ".."))
            continue;

        sprintf(filename, "/dev/net/%s", entry.name);
        stat_t statbuf;
        if (stat(filename, &statbuf) < EOK)
            continue;

        if (!(statbuf.mode & IFSOCK))
            continue;

        if (netif > 0)
            close(netif);

        netif = open(filename, O_RDONLY, 0);
        if (netif < EOK)
            continue;

        printf("%s:\n", entry.name);
        strcpy(ifreq.name, entry.name);

        if (ioctl(netif, SIOCGIFADDR, (int)&ifreq) < EOK)
            continue;
        printf("    inet: %r\n", ifreq.ipaddr);

        if (ioctl(netif, SIOCGIFNETMASK, (int)&ifreq) < EOK)
            continue;
        printf("    netmask: %r\n", ifreq.netmask);

        if (ioctl(netif, SIOCGIFGATEWAY, (int)&ifreq) < EOK)
            continue;
        printf("    gateway: %r\n", ifreq.gateway);

        if (ioctl(netif, SIOCGIFBRDADDR, (int)&ifreq) < EOK)
            continue;
        printf("    broadcast: %r\n", ifreq.broadcast);

        if (ioctl(netif, SIOCGIFHWADDR, (int)&ifreq) < EOK)
            continue;
        printf("    ether: %m\n", ifreq.hwaddr);
    }

rollback:
    if (dir < 0)
        close(dir);
}
