#include <onix/types.h>
#include <onix/cpu.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/errno.h>
#include <onix/string.h>
#include <onix/stdio.h>
#include <onix/net.h>
#include <onix/syscall.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define BUFLEN 2048

static char tx_buf[BUFLEN];
static char rx_buf[BUFLEN];

void test_sendrecv()
{
    sockaddr_in_t addr;

    int fd = socket(AF_INET, SOCK_STREAM, PROTO_TCP);
    assert(fd > 0);

    inet_aton("192.168.111.33", addr.addr);
    addr.family = AF_INET;
    addr.port = htons((u16)time());

    int ret = bind(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    LOGK("socket bind %d\n", ret);

    inet_aton("192.168.111.1", addr.addr);
    addr.family = AF_INET;
    addr.port = htons(7777);
    ret = connect(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    LOGK("socket connect %d\n", ret);

    while (true)
    {
        int len = sprintf(tx_buf, "hello tcp server %d", time());
        send(fd, tx_buf, len, 0);

        ret = recv(fd, rx_buf, BUFLEN, 0);
        if (ret > 0)
        {
            rx_buf[ret] = 0;
            LOGK("received %d bytes: %s.\n", ret, rx_buf);
        }
        sleep(1000);
    }
}

void test_connect()
{
    sockaddr_in_t addr;

    int fd = socket(AF_INET, SOCK_STREAM, PROTO_TCP);
    assert(fd > 0);

    inet_aton("192.168.111.33", addr.addr);
    addr.family = AF_INET;
    addr.port = htons((u16)time());

    int ret = bind(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    LOGK("socket bind %d\n", ret);

    inet_aton("192.168.111.1", addr.addr);
    addr.family = AF_INET;
    addr.port = htons(7777);

    ret = connect(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    LOGK("socket connect %d\n", ret);

    // sleep(1000);

    close(fd);
}

void test_server()
{
    sockaddr_in_t addr;

    int fd = socket(AF_INET, SOCK_STREAM, PROTO_TCP);
    assert(fd > 0);

    fd_t client = -1;

    inet_aton("0.0.0.0", addr.addr);
    addr.family = AF_INET;
    addr.port = htons(6666);

    int ret = bind(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    LOGK("socket bind %d\n", ret);
    if (ret < 0)
        goto rollback;

    ret = listen(fd, 1);
    LOGK("socket listen %d\n", ret);
    if (ret < 0)
        goto rollback;

    LOGK("waiting for client...\n");
    client = accept(fd, (sockaddr_t *)&addr, 0);
    if (client < 0)
    {
        LOGK("accept failure...\n");
        goto rollback;
    }
    LOGK("socket acccept %d\n", client);

    int len = sprintf(tx_buf, "hello tcp client %d", time());
    send(client, tx_buf, len, 0);

    ret = recv(client, rx_buf, BUFLEN, 0);
    if (ret > 0)
    {
        rx_buf[ret] = 0;
        LOGK("received %d bytes: %s.\n", ret, rx_buf);
    }
    sleep(1000);

rollback:
    if (client > 0)
        close(client);
    if (fd > 0)
        close(fd);
}

err_t sys_test()
{
    // test_sendrecv();
    // test_connect();
    // test_server();

    return EOK;
}
