#include <onix/types.h>
#include <onix/stdio.h>
#include <onix/syscall.h>
#include <onix/string.h>
#include <onix/fs.h>
#include <onix/net.h>

#define BUFLEN 4096

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

    memset(tx_buf, 'A', sizeof(tx_buf));

    u32 send_timing = time();
    u32 recv_timing = time();
    u32 send_count = 0;
    u32 recv_count = 0;

    pid_t pid = fork();

    while (true)
    {
        if (pid)
        {
            send_count++;
            ret = send(fd, tx_buf, sizeof(tx_buf), 0);
            // printf("message sent %d...\n", ret);
            if (ret < EOK)
                goto rollback;

            u32 now = time();
            int offset = now - send_timing;
            send_timing = now;
            if (offset > 0)
            {
                printf("send speed: %dKB/s\n", send_count * 4 / offset);
                send_count = 0;
            }
        }
        else
        {
            recv_count++;
            ret = recv(fd, rx_buf, BUFLEN, 0);
            // printf("message recv %d...\n", ret);
            if (ret < EOK)
                goto rollback;

            u32 now = time();
            int offset = now - recv_timing;
            recv_timing = now;
            if (offset > 0)
            {
                printf("recv speed: %dKB/s\n", recv_count * 4 / offset);
                recv_count = 0;
            }
        }
    }

rollback:
    if (fd > 0)
        close(fd);
}
