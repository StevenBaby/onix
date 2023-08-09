#include <onix/net/tcp.h>
#include <onix/net/socket.h>
#include <onix/net.h>
#include <onix/task.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

err_t tcp_output_seg(tcp_pcb_t *pcb, pbuf_t *pbuf)
{
    assert(pbuf->count <= 2);

    ip_t *ip = pbuf->eth->ip;
    tcp_t *tcp = ip->tcp;

    // 处理 ACK
    tcp->ackno = htonl(pcb->rcv_nxt);
    if (!tcp->syn)
    {
        tcp->ack = true;
        pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
    }

    // Silly Window Syndrome Avoidance
    if (pcb->rcv_wnd < pcb->rcv_mss)
        tcp->window = 0;
    else
        tcp->window = htons(pcb->rcv_wnd);

    netif_t *netif = netif_route(pcb->raddr);
    if (ip_addr_isany(pcb->laddr))
        ip_addr_copy(pcb->laddr, netif->ipaddr);

    tcp->chksum = 0;
    if (!(netif->flags & NETIF_TCP_TX_CHECKSUM_OFFLOAD))
        tcp->chksum = inet_chksum(tcp, pbuf->total, pcb->raddr, pcb->laddr, IP_PROTOCOL_TCP);

    return ip_output(netif, pbuf, ip->dst, IP_PROTOCOL_TCP, pbuf->total);
}

err_t tcp_enqueue(tcp_pcb_t *pcb, void *data, size_t size, int flags)
{
    int seglen = pcb->snd_mss;
    if (seglen < TCP_DEFAULT_MSS)
        seglen = TCP_DEFAULT_MSS;

    int left = size;
    tcp_t *tcp;
    pbuf_t *pbuf;

    do
    {
        pbuf = pbuf_get();
        ip_t *ip = pbuf->eth->ip;
        ip_addr_copy(ip->dst, pcb->raddr);

        tcp = ip->tcp;

        pbuf->seqno = pcb->snd_nbb;

        tcp->sport = htons(pcb->lport);
        tcp->dport = htons(pcb->rport);
        tcp->seqno = htonl(pcb->snd_nbb);
        tcp->flags = flags;
        tcp->urgent = 0;

        int hlen = tcp_write_option(pcb, tcp);
        int len = left < seglen ? left : seglen;

        pcb->snd_nbb += len;

        pbuf->data = ip->payload + hlen;
        pbuf->size = len;
        pbuf->total = hlen + len;

        if (left > 0)
            memcpy(pbuf->data, data, len);
        left -= len;

        list_insert_sort(
            &pcb->unsent, &pbuf->tcpnode,
            element_node_offset(pbuf_t, tcpnode, seqno));
    } while (left > 0);

    if (pbuf->size > 0)
        tcp->psh = true;

    return EOK;
}

err_t tcp_output(tcp_pcb_t *pcb)
{
    list_t *list = &pcb->unsent;

    u32 wnd = pcb->snd_wnd;

    for (list_node_t *node = list->head.next; node != &list->tail;)
    {
        pbuf_t *pbuf = element_entry(pbuf_t, tcpnode, node);
        node = node->next;

        u32 snd_nxt = pbuf->seqno + pbuf->size;
        if (TCP_SEQ_GT(snd_nxt, pcb->snd_una + wnd))
            break;

        tcp_t *tcp = pbuf->eth->ip->tcp;
        pcb->snd_nxt = snd_nxt;
        if (TCP_SEQ_GT(snd_nxt, pcb->snd_max))
            pcb->snd_max = snd_nxt;

        assert(pbuf->count == 1);
        pbuf->count++;
        list_remove(&pbuf->tcpnode);
        list_insert_sort(
            &pcb->unacked, &pbuf->tcpnode,
            element_node_offset(pbuf_t, tcpnode, seqno));
        tcp_output_seg(pcb, pbuf);

        pcb->timers[TCP_TIMER_REXMIT] = pcb->rto;
    }

    if (pcb->flags & TF_ACK_NOW)
        tcp_send_ack(pcb, TCP_ACK);

    return EOK;
}

err_t tcp_rexmit(tcp_pcb_t *pcb)
{
    list_t *list = &pcb->unacked;
    for (list_node_t *node = list->head.next; node != &list->tail;)
    {
        pbuf_t *pbuf = element_entry(pbuf_t, tcpnode, node);
        node = node->next;
        if (pbuf->count > 1)
            continue;

        list_remove(&pbuf->tcpnode);
        list_insert_sort(
            &pcb->unsent,
            &pbuf->tcpnode,
            element_node_offset(pbuf_t, tcpnode, seqno));
    }

    tcp_output(pcb);
    pcb->timers[TCP_TIMER_REXMIT] = pcb->rto;
    return EOK;
}

err_t tcp_send_ack(tcp_pcb_t *pcb, u8 flags)
{
    pbuf_t *pbuf = pbuf_get();
    ip_t *ip = pbuf->eth->ip;
    ip_addr_copy(ip->dst, pcb->raddr);

    tcp_t *tcp = ip->tcp;

    tcp->sport = htons(pcb->lport);
    tcp->dport = htons(pcb->rport);
    tcp->seqno = htonl(pcb->snd_nxt);

    tcp->flags = flags;

    tcp->len = sizeof(tcp_t) / 4;
    tcp->urgent = 0;

    pbuf->size = 0;
    pbuf->total = sizeof(tcp_t);
    pbuf->data = tcp->options;

    return tcp_output_seg(pcb, pbuf);
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

    if (!(netif->flags & NETIF_TCP_TX_CHECKSUM_OFFLOAD))
        tcp->chksum = inet_chksum(tcp, len, raddr, laddr, IP_PROTOCOL_TCP);

    return ip_output(netif, pbuf, raddr, IP_PROTOCOL_TCP, len);
}
