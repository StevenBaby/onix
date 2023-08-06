#include <onix/net/tcp.h>
#include <onix/net.h>
#include <onix/task.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

static const char *tcp_state_names[] = {
    "CLOSED",
    "LISTEN",
    "SYN_SENT",
    "SYN_RCVD",
    "ESTABLISHED",
    "FIN_WAIT1",
    "FIN_WAIT2",
    "CLOSE_WAIT",
    "CLOSING",
    "LAST_ACK",
    "TIME_WAIT",
};

static err_t tcp_handle_syn_sent(tcp_pcb_t *pcb, tcp_t *tcp)
{
    if (tcp->flags != (TCP_SYN | TCP_ACK))
        return -EPROTO;
    if (tcp->ackno != pcb->snd_nxt + 1)
        return -EPROTO;

    pcb->state = ESTABLISHED;

    pcb->rcv_nxt = tcp->seqno + 1;

    pcb->snd_nxt = tcp->ackno;
    pcb->snd_wnd = tcp->window;
    pcb->snd_una = tcp->ackno;

    tcp_send_ack(pcb, TCP_ACK);

    if (pcb->ac_waiter)
    {
        task_unblock(pcb->ac_waiter, EOK);
        pcb->ac_waiter = NULL;
    }

    LOGK("TCP ESTABLISHED client\n");
    return EOK;
}

static err_t tcp_handle_reset(tcp_pcb_t *pcb, pbuf_t *pbuf, ip_t *ip, tcp_t *tcp)
{
    if (pcb->state <= SYN_SENT)
        return -ERESET;

    // TODO: close pcb

    return -ERESET;
}

static err_t tcp_process(tcp_pcb_t *pcb, netif_t *netif, pbuf_t *pbuf, ip_t *ip, tcp_t *tcp)
{
    switch (pcb->state)
    {
    case SYN_SENT:
        return tcp_handle_syn_sent(pcb, tcp);
    default:
        break;
    }
    return EOK;
}

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

    // TCP 总长度
    pbuf->total = ip->length - sizeof(ip_t);

    if (!(netif->flags & NETIF_TCP_RX_CHECKSUM_OFFLOAD) &&
        inet_chksum(tcp, pbuf->total, ip->dst, ip->src, IP_PROTOCOL_TCP) != 0)
        return -ECHKSUM;

    // TCP 头长度
    int tcphlen = tcp->len * 4;
    // TCP 数据长度
    pbuf->size = pbuf->total - tcphlen;

    // TCP 数据指针
    pbuf->data = ip->payload + tcphlen;
    pbuf->data[pbuf->size] = 0;

    // 转换格式
    tcp->sport = ntohs(tcp->sport);
    tcp->dport = ntohs(tcp->dport);
    tcp->seqno = ntohl(tcp->seqno);
    tcp->ackno = ntohl(tcp->ackno);
    tcp->window = ntohs(tcp->window);
    tcp->urgent = ntohs(tcp->urgent);

    tcp_pcb_t *pcb = tcp_find_pcb(ip->dst, tcp->dport, ip->src, tcp->sport);
    if (!pcb)
        return -EADDR;

    if (tcp->rst)
        return tcp_handle_reset(pcb, pbuf, ip, tcp);

    err_t ret = tcp_parse_option(pcb, tcp);
    if (ret < EOK)
        return ret;

    assert(pcb->state >= CLOSED && pcb->state <= TIME_WAIT);

    LOGK("TCP [%s]: %r:%d -> %r:%d %d bytes\n",
         tcp_state_names[pcb->state],
         ip->src, tcp->sport, ip->dst, tcp->dport, pbuf->size);

    return tcp_process(pcb, netif, pbuf, ip, tcp);
}
