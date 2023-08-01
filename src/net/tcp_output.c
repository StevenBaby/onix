#include <onix/net/tcp.h>
#include <onix/net.h>
#include <onix/task.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

err_t tcp_send_ack(tcp_pcb_t *pcb, u8 flags)
{
    netif_t *netif = netif_route(pcb->raddr);
    assert(netif);

    pbuf_t *pbuf = pbuf_get();
    ip_t *ip = pbuf->eth->ip;
    tcp_t *tcp = ip->tcp;

    tcp->sport = htons(pcb->lport);
    tcp->dport = htons(pcb->rport);
    tcp->seqno = htonl(pcb->snd_nxt);
    tcp->ackno = htonl(pcb->rcv_nxt);

    tcp->flags = flags;

    tcp->window = htons(pcb->rcv_wnd);
    tcp->urgent = 0;
    tcp->len = sizeof(tcp_t) / 4;

    if (ip_addr_isany(pcb->laddr))
        ip_addr_copy(pcb->laddr, netif->ipaddr);

    int len = sizeof(tcp_t);
    tcp->chksum = 0;
    tcp->chksum = inet_chksum(tcp, len, pcb->raddr, pcb->laddr, IP_PROTOCOL_TCP);

    return ip_output(netif, pbuf, pcb->raddr, IP_PROTOCOL_TCP, len);
}

// 发送 TCP 重置消息
err_t tcp_reset(u32 seqno, u32 ackno, ip_addr_t laddr, u16 lport, ip_addr_t raddr, u16 rport)
{
    netif_t *netif = netif_route(raddr);
    assert(netif);

    pbuf_t *pbuf = pbuf_get();
    ip_t *ip = pbuf->eth->ip;
    tcp_t *tcp = ip->tcp;

    tcp->sport = htons(lport);
    tcp->dport = htons(rport);
    tcp->seqno = htonl(seqno);
    tcp->ackno = htonl(ackno);

    tcp->flags = TCP_ACK | TCP_RST;

    tcp->window = 0;
    tcp->urgent = 0;
    tcp->len = sizeof(tcp_t) / 4;

    int len = sizeof(tcp_t);
    tcp->chksum = 0;
    tcp->chksum = inet_chksum(tcp, len, raddr, laddr, IP_PROTOCOL_TCP);

    return ip_output(netif, pbuf, raddr, IP_PROTOCOL_TCP, len);
}
