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
    fd_t client = -1;
    fd_t server = -1;

    server = socket(AF_INET, SOCK_STREAM, PROTO_TCP);
    if (server < EOK)
    {
        printf("create server socket failure...\n");
        return server;
    }

    inet_aton("0.0.0.0", addr.addr);
    addr.family = AF_INET;
    addr.port = htons(6666);

    int ret = bind(server, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    printf("socket bind %d\n", ret);
    if (ret < 0)
        goto rollback;

    ret = listen(server, 1);
    printf("socket listen %d\n", ret);
    if (ret < 0)
        goto rollback;

    printf("waiting for client...\n");
    client = accept(server, (sockaddr_t *)&addr, 0);
    if (client < 0)
    {
        printf("accept failure...\n");
        goto rollback;
    }
    printf("socket acccept %d\n", client);

    int len = sprintf(tx_buf, "hello tcp client %d", time());
    send(client, tx_buf, len, 0);

    ret = recv(client, rx_buf, BUFLEN, 0);
    if (ret > 0)
    {
        rx_buf[ret] = 0;
        printf("received %d bytes: %s.\n", ret, rx_buf);
    }
    sleep(1000);

rollback:
    if (client > 0)
        close(client);
    if (server > 0)
        close(server);
}
