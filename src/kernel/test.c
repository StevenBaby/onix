#include <onix/types.h>
#include <onix/cpu.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/errno.h>
#include <onix/string.h>
#include <onix/net.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

err_t sys_test()
{
    // 发送测试数据包

    ip_addr_t addr;
    assert(inet_aton("8.8.8.8", addr) == EOK);

    pbuf_t *pbuf = pbuf_get();
    netif_t *netif = netif_route(addr);

    icmp_echo(netif, pbuf, addr);
}
