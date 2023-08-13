#include <onix/net/tcp.h>
#include <onix/net/port.h>
#include <onix/net.h>
#include <onix/list.h>
#include <onix/arena.h>
#include <onix/task.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

list_t tcp_pcb_create_list;   // 创建 pcb 列表
list_t tcp_pcb_active_list;   // 活动 pcb 列表
list_t tcp_pcb_listen_list;   // 监听 pcb 列表
list_t tcp_pcb_timewait_list; // 等待 pcb 列表

extern port_map_t tcp_port_map; // 端口位图

tcp_pcb_t *tcp_pcb_get()
{
    LOGK("tcp pcb get...\n");
    tcp_pcb_t *pcb = (tcp_pcb_t *)kmalloc(sizeof(tcp_pcb_t));
    memset(pcb, 0, sizeof(tcp_pcb_t));

    pcb->rcv_nxt = 0;
    pcb->rcv_mss = TCP_MSS;
    pcb->rcv_wnd = TCP_WINDOW;

    pcb->snd_nxt = tcp_next_isn();
    pcb->snd_una = pcb->snd_nxt;
    pcb->snd_mss = TCP_DEFAULT_MSS;
    pcb->snd_wnd = TCP_DEFAULT_MSS;
    pcb->snd_nbb = pcb->snd_nxt;

    pcb->rtt = 0;
    pcb->rtt_seq = pcb->snd_una;
    pcb->srtt = 0;
    pcb->rttvar = TCP_TO_RTT_DEFAULT;
    pcb->rttmin = TCP_TO_MIN;
    pcb->rto = tcp_update_rto(pcb, 1);

    pcb->snd_cwnd = TCP_DEFAULT_MSS;
    pcb->snd_ssthresh = TCP_MAX_WINDOW;

    list_init(&pcb->unsent);
    list_init(&pcb->unacked);
    list_init(&pcb->outseq);
    list_init(&pcb->recved);
    list_init(&pcb->acclist);

    list_push(&tcp_pcb_create_list, &pcb->node);
    return pcb;
}

// 释放 TCP 缓存
static void tcp_list_put(list_t *list)
{
    for (list_node_t *node = list->head.next; node != &list->tail;)
    {
        pbuf_t *pbuf = element_entry(pbuf_t, tcpnode, node);
        node = node->next;
        list_remove(&pbuf->tcpnode);
        assert(pbuf->count == 1);
        pbuf_put(pbuf);
    }
}

void tcp_pcb_purge(tcp_pcb_t *pcb, err_t reason)
{
    tcp_list_put(&pcb->recved);
    tcp_list_put(&pcb->outseq);
    tcp_list_put(&pcb->unsent);
    tcp_list_put(&pcb->unacked);

    list_t *list = &pcb->acclist;
    for (list_node_t *node = list->head.next; node != &list->tail;)
    {
        tcp_pcb_t *npcb = element_entry(tcp_pcb_t, accnode, node);
        node = node->next;
        list_remove(&npcb->accnode);
        tcp_pcb_put(npcb);
    }

    if (pcb->ac_waiter)
    {
        task_unblock(pcb->ac_waiter, reason);
        pcb->ac_waiter = NULL;
    }

    if (pcb->rx_waiter)
    {
        task_unblock(pcb->rx_waiter, reason);
        pcb->rx_waiter = NULL;
    }

    if (pcb->tx_waiter)
    {
        task_unblock(pcb->tx_waiter, reason);
        pcb->tx_waiter = NULL;
    }
    LOGK("TCP PURGE %#p\n", pcb);
}

void tcp_pcb_timewait(tcp_pcb_t *pcb)
{
    pcb->state = TIME_WAIT;
    tcp_pcb_purge(pcb, -ETIME);
    list_remove(&pcb->node);
    pcb->timers[TCP_TIMER_TIMEWAIT] = TCP_TO_TIMEWAIT;
    list_insert_sort(&tcp_pcb_timewait_list, &pcb->node, element_node_offset(tcp_pcb_t, node, timers[TCP_TIMER_TIMEWAIT]));
}

void tcp_pcb_put(tcp_pcb_t *pcb)
{
    if (!pcb)
        return;
    if (pcb->lport != 0 && !pcb->listen)
        port_put(&tcp_port_map, pcb->lport);
    tcp_pcb_purge(pcb, -ETIME);
    list_remove(&pcb->node);
    kfree(pcb);
    LOGK("tcp pcb put...\n");
}

tcp_pcb_t *tcp_find_pcb(ip_addr_t laddr, u16 lport, ip_addr_t raddr, u16 rport)
{
    list_t *lists[3] = {
        &tcp_pcb_active_list,
        &tcp_pcb_timewait_list,
        &tcp_pcb_listen_list};

    u16 rports[3] = {rport, rport, 0};

    for (size_t i = 0; i < 3; i++)
    {
        list_t *list = lists[i];
        rport = rports[i];

        for (list_node_t *node = list->head.next; node != &list->tail; node = node->next)
        {
            tcp_pcb_t *pcb = element_entry(tcp_pcb_t, node, node);
            if (pcb->lport != lport)
                continue;
            if (!ip_addr_isany(pcb->laddr) && !ip_addr_cmp(pcb->laddr, laddr))
                continue;

            if (rport == 0)
                return pcb;

            if (pcb->rport != rport)
                continue;
            if (!ip_addr_isany(pcb->raddr) && !ip_addr_cmp(pcb->raddr, raddr))
                continue;
            return pcb;
        }
    }
    return NULL;
}

void tcp_pcb_init()
{
    list_init(&tcp_pcb_create_list);
    list_init(&tcp_pcb_active_list);
    list_init(&tcp_pcb_timewait_list);
    list_init(&tcp_pcb_listen_list);
}