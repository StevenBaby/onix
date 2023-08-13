#include <onix/net/tcp.h>
#include <onix/net.h>
#include <onix/net/socket.h>
#include <onix/task.h>
#include <onix/string.h>
#include <onix/arena.h>
#include <onix/stdlib.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern list_t tcp_pcb_active_list;

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

static void tcp_update_wnd(tcp_pcb_t *pcb, tcp_t *tcp)
{
    if (TCP_SEQ_LT(pcb->snd_wl1, tcp->seqno) ||
        (tcp->seqno == pcb->snd_wl1 && TCP_SEQ_LEQ(pcb->snd_wl2, tcp->ackno)))
    {
        pcb->snd_wnd = tcp->window;
        pcb->snd_wl1 = tcp->seqno;
        pcb->snd_wl2 = tcp->ackno;
        if (tcp->window > pcb->snd_mwnd)
            pcb->snd_mwnd = tcp->window;

        if (pcb->snd_wnd == 0)
            pcb->timers[TCP_TIMER_PERSIST] = TCP_TO_PERSMIN;
        else
            pcb->timers[TCP_TIMER_PERSIST] = 0;
    }
}

static void tcp_handle_duppacks(tcp_pcb_t *pcb)
{
    if (pcb->dupacks < TCP_REXMIT_THREASH)
        return;

    if (pcb->dupacks == TCP_REXMIT_THREASH)
    {
        int segs = MIN(pcb->snd_wnd, pcb->snd_cwnd) / 2 / pcb->snd_mss;
        if (segs < 2)
            segs = 2;

        pcb->snd_ssthresh = segs * pcb->snd_mss;
        pcb->snd_cwnd = pcb->snd_mss;
        pcb->rtt = 0; // 取消 RTT 估计

        tcp_rexmit(pcb);
        // 启用快速重传
        pcb->snd_cwnd = pcb->snd_ssthresh + pcb->snd_mss * pcb->dupacks;
        return;
    }

    if (pcb->dupacks > TCP_REXMIT_THREASH)
    {
        pcb->snd_cwnd += pcb->snd_mss;
        tcp_rexmit(pcb); // 快速重传
        return;
    }
}

static void tcp_update_ack(tcp_pcb_t *pcb, pbuf_t *pbuf, tcp_t *tcp)
{
    // 重复 ACK
    if (TCP_SEQ_GEQ(pcb->snd_una, tcp->ackno))
    {
        if (pbuf->size == 0)
        {
            pcb->dupacks++;
            tcp_handle_duppacks(pcb);
        }
        else
            pcb->dupacks = 0;
        return;
    }

    // 丢包大于阈值，拥塞窗口调整为阈值；
    if (pcb->dupacks > TCP_REXMIT_THREASH && pcb->snd_cwnd > pcb->snd_ssthresh)
        pcb->snd_cwnd = pcb->snd_ssthresh;

    pcb->dupacks = 0;

    int cwnd = pcb->snd_cwnd;
    int unit = pcb->snd_mss;
    // 拥塞窗口大于阈值，线性增加
    if (cwnd > pcb->snd_ssthresh)
    {
        unit = unit * unit / cwnd;
    }

    // 慢启动
    pcb->snd_cwnd = MIN(cwnd + unit, TCP_MAX_WINDOW);

    if (pcb->rtt && tcp->ackno == pcb->rtt_seq)
        tcp_xmit_timer(pcb, pcb->rtt - 1);

    // 启动 RTT 估计
    pcb->rtt = 1;
    pcb->snd_una = tcp->ackno;
    pcb->rtx_cnt = 0;

    list_t *lists[2] = {&pcb->unacked, &pcb->unsent};

    // 释放已经确认的段
    for (size_t i = 0; i < 2; i++)
    {
        list_t *list = lists[i];
        for (list_node_t *node = list->head.next; node != &list->tail;)
        {
            pbuf_t *pbuf = element_entry(pbuf_t, tcpnode, node);
            node = node->next;

            u32 ackno = pbuf->seqno + pbuf->size;
            if (TCP_SEQ_GT(ackno, tcp->ackno))
                continue;

            list_remove(&pbuf->tcpnode);
            assert(pbuf->count <= 2);
            pbuf_put(pbuf);
        }
    }

    if (pcb->snd_buf && pcb->snd_buf->size > 0 && tcp->ackno == pcb->snd_max)
    {
        list_insert_sort(
            &pcb->unsent, &pcb->snd_buf->tcpnode,
            element_node_offset(pbuf_t, tcpnode, seqno));
        pcb->snd_buf = NULL;
    }

    if (list_empty(&pcb->unacked) && list_empty(&pcb->unsent) && pcb->tx_waiter)
    {
        task_unblock(pcb->tx_waiter, EOK);
        pcb->tx_waiter = NULL;
        pcb->timers[TCP_TIMER_REXMIT] = 0;
    }
}

static void tcp_update_buf(tcp_pcb_t *pcb, pbuf_t *pbuf, tcp_t *tcp)
{
    u32 rcv_nxt = tcp->seqno + pbuf->size;

    // 已经收到此包
    if (TCP_SEQ_LEQ(rcv_nxt, pcb->rcv_nxt))
    {
        LOGK("drop acked packet...\n");
        pcb->flags |= TF_ACK_DELAY;
        return;
    }

    // 乱序数据包
    if (TCP_SEQ_GT(tcp->seqno, pcb->rcv_nxt))
    {
        LOGK("drop outseq packet...\n");
        pcb->flags |= TF_ACK_DELAY;
        return;
    }

    // 不规则数据包
    if (TCP_SEQ_LT(tcp->seqno, pcb->rcv_nxt))
    {
        int offset = pcb->rcv_nxt - tcp->seqno;
        pbuf->data += offset;
        pbuf->size -= offset;
    }

    // 用于保活机制探测
    if (pbuf->size == 0)
    {
        pcb->flags |= TF_ACK_DELAY;
        return;
    }

    // 进入接收缓冲队列
    pcb->rcv_nxt = rcv_nxt;
    assert(pbuf->count == 1);
    pbuf->count++;

    // 修改接收窗口
    pcb->rcv_wnd -= pbuf->size;

    pcb->flags |= TF_ACK_DELAY;
    if (pcb->flags & TF_QUICKACK)
        tcp_send_ack(pcb, TCP_ACK);

    if (pcb->state > ESTABLISHED)
        return;

    list_insert_sort(
        &pcb->recved,
        &pbuf->tcpnode,
        element_node_offset(pbuf_t, tcpnode, seqno));

    if (pcb->rx_waiter)
    {
        task_unblock(pcb->rx_waiter, EOK);
        pcb->rx_waiter = NULL;
    }
}

static err_t tcp_receive(tcp_pcb_t *pcb, pbuf_t *pbuf, tcp_t *tcp)
{
    if (tcp->ack)
    {
        tcp_update_wnd(pcb, tcp);
        tcp_update_ack(pcb, pbuf, tcp);
    }

    if (pbuf->size > 0)
        tcp_update_buf(pcb, pbuf, tcp);
}

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
    pcb->snd_nbb = tcp->ackno;

    pcb->flags |= TF_ACK_DELAY;
    if (pcb->flags & TF_QUICKACK)
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

    tcp_pcb_purge(pcb, -ERESET);
    pcb->state = CLOSED;

    return -ERESET;
}

static err_t tcp_handle_listen(
    tcp_pcb_t *pcb,
    netif_t *netif, pbuf_t *pbuf,
    ip_t *ip, tcp_t *tcp)
{
    if (tcp->flags != TCP_SYN)
        return -EPROTO;
    if (pcb->backcnt == pcb->backlog)
        return -EPROTO;

    tcp_pcb_t *npcb = tcp_pcb_get();

    ip_addr_copy(npcb->laddr, netif->ipaddr);
    ip_addr_copy(npcb->raddr, ip->src);
    npcb->lport = pcb->lport;
    npcb->rport = tcp->sport;
    npcb->state = SYN_RCVD;

    npcb->snd_wnd = tcp->window;
    npcb->snd_mss = pcb->snd_mss;
    if (npcb->snd_mss < TCP_DEFAULT_MSS)
        npcb->snd_mss = TCP_DEFAULT_MSS;

    npcb->rcv_nxt = tcp->seqno + 1;

    npcb->listen = pcb;

    list_push(&pcb->acclist, &npcb->accnode);
    pcb->backcnt++;

    list_remove(&npcb->node);
    list_push(&tcp_pcb_active_list, &npcb->node);

    tcp_enqueue(npcb, NULL, 0, TCP_SYN | TCP_ACK);
    tcp_output(npcb);

    pcb->timers[TCP_TIMER_SYN] = TCP_TO_SYN;

    LOGK("TCP SYN_RCVD\n");
    return EOK;
}

static err_t tcp_process(tcp_pcb_t *pcb, netif_t *netif, pbuf_t *pbuf, ip_t *ip, tcp_t *tcp)
{
    if (tcp->ack && pcb->state != TIME_WAIT)
        tcp_receive(pcb, pbuf, tcp);

    switch (pcb->state)
    {
    case LISTEN:
        return tcp_handle_listen(pcb, netif, pbuf, ip, tcp);
    case SYN_SENT:
        return tcp_handle_syn_sent(pcb, tcp);
    case SYN_RCVD:
        if (!tcp->ack || tcp->ackno != pcb->snd_nxt + 1)
            break;
        LOGK("TCP ESTABLISHED server\n");

        pcb->state = ESTABLISHED;
        pcb->snd_nxt = tcp->ackno;
        pcb->snd_nbb = tcp->ackno;

        assert(pcb->listen);
        if (pcb->listen->ac_waiter)
        {
            task_unblock(pcb->listen->ac_waiter, EOK);
            pcb->listen->ac_waiter;
        }
        break;
    case CLOSE_WAIT:
    case ESTABLISHED:
        if (tcp->fin)
        {
            pcb->flags |= TF_ACK_NOW;
            pcb->state = CLOSE_WAIT;
            pcb->rcv_nxt = tcp->seqno + 1;
            LOGK("TCP CLOSE_WAIT\n");
        }
        break;
    case LAST_ACK:
        if (tcp->ack)
        {
            pcb->state = CLOSED;
            tcp_pcb_put(pcb);
            LOGK("TCP CLOSED\n");
        }
        break;
    case FIN_WAIT1:
        if (tcp->fin && tcp->ack && tcp->ackno == pcb->snd_nxt + 1)
        {
            pcb->snd_nxt = pcb->snd_nxt + 1;
            pcb->snd_nbb = pcb->snd_nxt;
            tcp_pcb_timewait(pcb);
            LOGK("TCP TIME_WAIT\n");
        }
        else if (tcp->fin)
        {
            pcb->state = CLOSING;
            LOGK("TCP CLOSING\n");
        }
        else if (tcp->ack && tcp->ackno == pcb->snd_nxt + 1)
        {
            pcb->snd_nxt = pcb->snd_nxt + 1;
            pcb->snd_nbb = pcb->snd_nxt;
            pcb->state = FIN_WAIT2;
            pcb->timers[TCP_TIMER_FIN_WAIT2] = TCP_TO_FIN_WAIT2;
            LOGK("TCP FIN_WAIT2\n");
        }
        if (tcp->fin)
        {
            pcb->rcv_nxt = tcp->seqno + 1;
            pcb->flags |= TF_ACK_NOW;
        }
        break;
    case FIN_WAIT2:
        if (tcp->fin)
        {
            pcb->rcv_nxt = tcp->seqno + 1;
            pcb->flags |= TF_ACK_NOW;
            tcp_pcb_timewait(pcb);
            LOGK("TCP TIME_WAIT\n");
        }
        break;
    case CLOSING:
        if (tcp->ack && tcp->ackno == pcb->snd_nxt + 1)
        {
            pcb->snd_nxt = pcb->snd_nxt + 1;
            pcb->snd_nbb = pcb->snd_nxt;
            tcp_pcb_timewait(pcb);
            LOGK("TCP TIME_WAIT\n");
        }
        break;
    case TIME_WAIT:
        if (TCP_SEQ_GT(tcp->seqno + pbuf->size, pcb->rcv_nxt))
            pcb->rcv_nxt = tcp->seqno + pbuf->size;
        if (pbuf->size > 0)
            pcb->flags |= TF_ACK_NOW;
        break;
    default:
        break;
    }
    return EOK;
}

// 检查输入合法性
static err_t tcp_validate(tcp_pcb_t *pcb, pbuf_t *pbuf, tcp_t *tcp)
{
    if (tcp->syn)
        return EOK;

    if (TCP_SEQ_GT(pbuf->seqno + pbuf->size, pcb->rcv_nxt + pcb->rcv_wnd))
    {
        return -EPROTO;
    }

    if (TCP_SEQ_LT(pbuf->seqno + pbuf->size, pcb->rcv_nxt))
    {
        return -EPROTO;
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

    pbuf->seqno = tcp->seqno;

    tcp_pcb_t *pcb = tcp_find_pcb(ip->dst, tcp->dport, ip->src, tcp->sport);
    if (!pcb)
        return -EADDR;

    if (tcp_validate(pcb, pbuf, tcp) < EOK)
        return -EPROTO;

    if (pcb->state == CLOSED)
        return tcp_reset(tcp->ackno + 1, tcp->seqno + pbuf->size, ip->dst, tcp->dport, ip->src, tcp->sport);

    if (tcp->rst)
        return tcp_handle_reset(pcb, pbuf, ip, tcp);

    err_t ret = tcp_parse_option(pcb, tcp);
    if (ret < EOK)
        return ret;

    // 空闲结束
    pcb->idle = 0;

    // 重置保活定时器
    if (pcb->flags & TF_KEEPALIVE)
        pcb->timers[TCP_TIMER_KEEPALIVE] = TCP_TO_KEEP_IDLE;

    assert(pcb->state >= CLOSED && pcb->state <= TIME_WAIT);

    LOGK("TCP [%s]: %r:%d -> %r:%d %d bytes\n",
         tcp_state_names[pcb->state],
         ip->src, tcp->sport, ip->dst, tcp->dport, pbuf->size);

    return tcp_process(pcb, netif, pbuf, ip, tcp);
}
