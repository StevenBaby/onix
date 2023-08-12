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
    sockaddr_in_t addr;

    int fd = socket(AF_INET, SOCK_STREAM, PROTO_TCP);
    if (fd < EOK)
    {
        printf("create client socket failure...\n");
        return fd;
    }

    inet_aton("192.168.111.33", addr.addr);
    addr.family = AF_INET;
    addr.port = htons((u16)time());

    int ret = bind(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    printf("socket bind %d\n", ret);
    if (ret < EOK)
        goto rollback;

    int val = 1;
    ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, 4);
    printf("socket keepalive %d\n", ret);
    if (ret < EOK)
        goto rollback;

    val = 1;
    ret = setsockopt(fd, SOL_SOCKET, SO_TCP_NODELAY, &val, 4);
    printf("socket nodelay %d\n", ret);
    if (ret < EOK)
        goto rollback;

    inet_aton("192.168.111.1", addr.addr);
    addr.family = AF_INET;
    addr.port = htons(7777);
    ret = connect(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    printf("socket connect %d\n", ret);

    // 测试 nagle
    int count = 0;
    while (true)
    {
        count++;
        tx_buf[0] = (time() % 26) + 'A';
        ret = send(fd, tx_buf, 1, 0);
        printf("message sent %d/%d...\n", ret, count);
        if (ret < EOK)
            goto rollback;
    }

rollback:
    if (fd > 0)
        close(fd);
}
