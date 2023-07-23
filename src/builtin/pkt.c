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

    msghdr_t msg;
    iovec_t iov;

    sockaddr_ll_t saddr;
    eth_addr_copy(saddr.addr, "\x5a\x5a\x5a\x5a\x5a\x33");
    saddr.family = AF_PACKET;

    int ret = bind(fd, (sockaddr_t *)&saddr, sizeof(sockaddr_ll_t));
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
