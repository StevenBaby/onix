#include <onix/types.h>
#include <onix/stdio.h>
#include <onix/syscall.h>
#include <onix/string.h>
#include <onix/fs.h>
#include <onix/net.h>

#define BUFLEN 2048

static char tx_buf[BUFLEN];
static char rx_buf[BUFLEN];

int main(int argc, char const *argv[])
{
    int fd = socket(AF_INET, SOCK_DGRAM, PROTO_UDP);

    int ret;
    sockaddr_in_t addr;
    inet_aton("0.0.0.0", addr.addr);
    addr.family = AF_INET;
    addr.port = htons(6666);

    ret = bind(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));

    printf("socket bind %d\n", ret);

    inet_aton("192.168.111.11", addr.addr);
    addr.family = AF_INET;
    addr.port = htons(7777);

    ret = connect(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    printf("socket connect %d\n", ret);

    u16 length = sprintf(tx_buf, "hello udp server %d\n", time());
    ret = send(fd, tx_buf, length, 0);
    printf("socket send %d\n", ret);

    printf("receiving\n");
    ret = recv(fd, rx_buf, sizeof(rx_buf), 0);
    printf("socket recv %d bytes\n", ret);
    rx_buf[ret] = 0;
    printf("msg: %s\n", rx_buf);

    close(fd);
    return EOK;
}
