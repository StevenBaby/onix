#include <onix/types.h>
#include <onix/cpu.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/errno.h>
#include <onix/string.h>
#include <onix/net.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern void send_packet();

err_t sys_test()
{
    // 发送测试数据包

    pbuf_t *pbuf = pbuf_get();

    memcpy(pbuf->eth->dst, "\xff\xff\xff\xff\xff\x00", 6);
    memcpy(pbuf->eth->src, "\x5a\xab\xcc\x5a\x5a\x33", 6);
    pbuf->eth->type = 0x0033;

    int len = 1500;
    pbuf->length = len + sizeof(eth_t);
    memset(pbuf->eth->payload, 'A', len);
    send_packet(pbuf);
}
