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
    int fd = socket(AF_PACKET, SOCK_RAW, 0);
    if (fd < EOK)
    {
        printf("open socket error\n");
        goto rollback;
    }

    int opt = 10000;
    int ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &opt, 4);
    if (ret < 0)
    {
        printf("set timeout error.\n");
        goto rollback;
    }

    ifreq_t req;
    strcpy(req.name, "eth1");
    ret = ioctl(fd, SIOCGIFINDEX, (int)&req);
    if (ret < 0)
    {
        printf("get netif error.\n");
        goto rollback;
    }

    opt = req.index;
    printf("get netif index %d\n", opt);
    ret = setsockopt(fd, SOL_SOCKET, SO_NETIF, &opt, 4);
    if (ret < 0)
    {
        printf("set netif error.\n");
        goto rollback;
    }

    msghdr_t msg;
    iovec_t iov;

    sockaddr_ll_t saddr;
    eth_addr_copy(saddr.addr, "\x5a\x5a\x5a\x5a\x5a\x33");
    saddr.family = AF_PACKET;

    ret = bind(fd, (sockaddr_t *)&saddr, sizeof(sockaddr_ll_t));
    if (ret < EOK)
    {
        printf("bind error\n");
        goto rollback;
    }

    msg.name = (sockaddr_t *)&saddr;
    msg.namelen = sizeof(saddr);

    msg.iov = &iov;
    msg.iovlen = 1;

    iov.base = buf;
    iov.size = sizeof(buf);

    printf("receiving...\n");
    ret = recvmsg(fd, &msg, 0);

    if (ret < 0)
    {
        printf("recvmsg error %d\n", ret);
        goto rollback;
    }

    printf("recvmsg %d\n", ret);
    eth_t *eth = (eth_t *)iov.base;

    printf("recv eth %m -> %m : %#x\n", eth->src, eth->dst, ntohs(eth->type));

    eth_addr_t addr;
    eth_addr_copy(addr, eth->dst);
    eth_addr_copy(eth->dst, eth->src);
    eth_addr_copy(eth->src, addr);

    char payload[] = "this is ack message"; // acknowledgement

    strcpy(eth->payload, payload);

    iov.size = sizeof(eth_t) + sizeof(payload);

    sendmsg(fd, &msg, 0);

rollback:
    if (fd > 0)
    {
        close(fd);
    }
    return EOK;
}
