#include <onix/types.h>
#include <onix/stdio.h>
#include <onix/syscall.h>
#include <onix/string.h>
#include <onix/fs.h>
#include <onix/net.h>

#define BUFLEN 2048

static char rx_buf[BUFLEN];

char response[] = "HTTP/1.1 200 OK\r\n"
                  "Content-Type: text/html; charset=ascii\r\n"
                  "\r\n"
                  "<!DOCTYPE html>"
                  "<html>"
                  "<head><title>Onix HTTP Server</title></head>"
                  "<body>"
                  "<h1 style='color:#e03997;'><center>hello onix!!!</center></h1>"
                  "</body></html>";

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
    addr.port = htons(80);

    int ret = bind(server, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    printf("socket bind %d\n", ret);
    if (ret < 0)
        goto rollback;

    ret = listen(server, 16);
    printf("socket listen %d\n", ret);
    if (ret < 0)
        goto rollback;

    while (true)
    {
        printf("waiting for client...\n");
        client = accept(server, (sockaddr_t *)&addr, 0);
        if (client < 0)
        {
            printf("accept failure...\n");
            goto rollback;
        }
        printf("socket acccept %d\n", client);

        ret = recv(client, rx_buf, BUFLEN, 0);
        if (ret < EOK)
        {
            goto rollback;
        }

        rx_buf[ret] = 0;
        printf("received %d bytes: \n--------------------\n", ret);
        printf(rx_buf);

        if (memcmp(rx_buf, "GET /", 5))
        {
            close(client);
            continue;
        }

        ret = send(client, response, sizeof(response), 0);
        if (ret < EOK)
        {
            goto rollback;
        }
        close(client);
        continue;
    }

rollback:
    if (client > 0)
        close(client);
    if (server > 0)
        close(server);
    return ret;
}
