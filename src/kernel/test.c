#include <onix/types.h>
#include <onix/cpu.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/errno.h>
#include <onix/string.h>
#include <onix/net.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

err_t sys_test()
{
    // 发送测试数据包

    pbuf_t *pbuf = pbuf_get();
    netif_t *netif = netif_get();

    int len = 1500;
    memset(pbuf->eth->payload, 'A', len);
    eth_output(netif, pbuf, "\xff\xff\xff\xff\xff\x00", 0x9000, len);
}
