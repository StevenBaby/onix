#include <onix/types.h>
#include <onix/cpu.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/errno.h>
#include <onix/string.h>
#include <onix/net.h>
#include <onix/syscall.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

err_t sys_test()
{
    sockaddr_in_t addr;

    int fd = socket(AF_INET, SOCK_STREAM, PROTO_TCP);
    assert(fd > 0);

    int ret;

    inet_aton("192.168.111.33", addr.addr);
    addr.family = AF_INET;
    addr.port = htons(time());

    ret = bind(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    LOGK("socket bind %d\n", ret);

    inet_aton("192.168.111.1", addr.addr);
    addr.family = AF_INET;
    addr.port = htons(7777);

    ret = connect(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    LOGK("socket connect %d\n", ret);

    // close(fd);
    return EOK;
}
