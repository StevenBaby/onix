#include <onix/net/tcp.h>
#include <onix/net.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

err_t tcp_input(netif_t *netif, pbuf_t *pbuf)
{
    ip_t *ip = pbuf->eth->ip;
    tcp_t *tcp = ip->tcp;
    // TCP 头部不合法
    if (tcp->len * 4 < sizeof(tcp_t))
        return -EPROTO;

    // 干掉广播
    if (ip_addr_isbroadcast(ip->dst, netif->netmask))
        return EOK;

    // 干掉多播
    if (ip_addr_ismulticast(ip->dst))
        return EOK;

    // 转换格式
    tcp->sport = ntohs(tcp->sport);
    tcp->dport = ntohs(tcp->dport);
    tcp->seqno = ntohl(tcp->seqno);
    tcp->ackno = ntohl(tcp->ackno);
    tcp->window = ntohs(tcp->window);
    tcp->urgent = ntohs(tcp->urgent);

    // TCP 头长度
    int tcphlen = tcp->len * 4;

    // TCP 数据长度
    pbuf->total = ip->length - sizeof(ip_t);
    pbuf->size = pbuf->total - tcphlen;

    // TCP 数据指针
    pbuf->data = ip->payload + tcphlen;
    pbuf->data[pbuf->size] = 0;

    LOGK("TCP: %s\n", pbuf->data);
}