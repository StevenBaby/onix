#include <onix/net/tcp.h>
#include <onix/list.h>
#include <onix/task.h>
#include <onix/stdlib.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern list_t tcp_pcb_active_list;   // 活动 pcb 列表
extern list_t tcp_pcb_timewait_list; // 等待 pcb 列表

static task_t *tcp_slow; // TCP 慢速任务
static task_t *tcp_fast; // TCP 快速任务

// 指数退避
static u32 tcp_backoff[TCP_MAXRXTCNT + 1];

// 范围取值
static _inline u32 tcp_range_value(u32 val, u32 min, u32 max)
{
    if (val < min)
        return min;
    if (val > max)
        return max;
    return val;
}

static void tcp_fastimo()
{
    list_t *lists[] = {&tcp_pcb_active_list, &tcp_pcb_timewait_list};
    while (true)
    {
        for (size_t i = 0; i < 2; i++)
        {
            list_t *list = lists[i];
            for (list_node_t *node = list->head.next; node != &list->tail; node = node->next)
            {
                tcp_pcb_t *pcb = element_entry(tcp_pcb_t, node, node);
                if (pcb->flags & TF_ACK_DELAY)
                    pcb->flags |= TF_ACK_NOW;
                if (pcb->flags & TF_ACK_NOW)
                    tcp_output(pcb);
            }
        }
        task_sleep(TCP_FAST_INTERVAL);
    }
}

static void tcp_persist(tcp_pcb_t *pcb)
{
    LOGK("tcp persist...\n");
    u32 value = ((pcb->srtt >> 2) + pcb->rttvar) >> 1;
    pcb->timers[TCP_TIMER_PERSIST] = tcp_range_value(
        value * tcp_backoff[pcb->rtx_cnt], TCP_TO_PERMAX, TCP_TO_PERMAX);
    if (pcb->rtx_cnt < TCP_MAXRXTCNT)
        pcb->rtx_cnt++;

    tcp_response(pcb, pcb->snd_una, pcb->rcv_nxt, TCP_ACK);
}

static void tcp_keepalive(tcp_pcb_t *pcb)
{
    LOGK("keep alive...\n");
    tcp_response(pcb, pcb->snd_una - 1, pcb->rcv_nxt, TCP_ACK);
    pcb->timers[TCP_TIMER_KEEPALIVE] = TCP_TO_KEEP_INTERVAL;
}

u32 tcp_update_rto(tcp_pcb_t *pcb, int backoff)
{
    u32 value = pcb->srtt + (pcb->rttvar << 2);
    return tcp_range_value(value * backoff, pcb->rttmin, TCP_TO_REXMIT_MAX);
}

void tcp_xmit_timer(tcp_pcb_t *pcb, u32 rtt)
{
    int err = rtt - pcb->srtt;
    pcb->srtt += err >> TCP_RTT_SHIFT;
    if (pcb->srtt <= 0)
        pcb->srtt = 1;

    pcb->rttvar += (ABS(err) - pcb->rttvar) >> TCP_RTTVAR_SHIFT;
    if (pcb->rttvar <= 0)
        pcb->rttvar = 1;

    pcb->rtt = 0;
    pcb->rtx_cnt = 0;
    pcb->rto = tcp_update_rto(pcb, 1);
    LOGK("RTT %d update RTO %d\n", rtt, pcb->rto);
}

static void tcp_timeout(tcp_pcb_t *pcb, int type)
{
    switch (type)
    {
    case TCP_TIMER_SYN:
        LOGK("tcp timeout syn\n");
        if (pcb->state != SYN_SENT && pcb->state != SYN_RCVD)
            return;
        tcp_pcb_purge(pcb, -ETIME);
        pcb->state = CLOSED;
        break;
    case TCP_TIMER_REXMIT:
        LOGK("tcp timeout rexmit\n");
        pcb->rtx_cnt++;
        if (pcb->rtx_cnt > TCP_MAXRXTCNT)
        {
            tcp_pcb_purge(pcb, -ETIME);
            pcb->state = CLOSED;
            return;
        }
        pcb->rto = tcp_update_rto(pcb, tcp_backoff[pcb->rtx_cnt]);
        LOGK("TCP update RTO %d\n", pcb->rto);
        tcp_rexmit(pcb);
        break;
    case TCP_TIMER_PERSIST:
        tcp_persist(pcb);
        break;
    case TCP_TIMER_KEEPALIVE:
        LOGK("keepalive...\n");
        if (!(pcb->flags & TF_KEEPALIVE))
            return;
        if (pcb->state < ESTABLISHED)
            return;
        if (pcb->idle > TCP_TO_KEEP_INTERVAL * TCP_TO_KEEPCNT)
        {
            tcp_pcb_purge(pcb, -ETIME);
            pcb->state = CLOSED;
            return;
        }
        tcp_keepalive(pcb);
        break;
    case TCP_TIMER_FIN_WAIT2:
    case TCP_TIMER_TIMEWAIT:
        tcp_pcb_put(pcb);
        break;
    default:
        break;
    }
}

static void tcp_slowtimo()
{
    list_t *lists[] = {&tcp_pcb_active_list, &tcp_pcb_timewait_list};
    while (true)
    {
        for (size_t i = 0; i < 2; i++)
        {
            list_t *list = lists[i];
            for (list_node_t *node = list->head.next; node != &list->tail;)
            {
                tcp_pcb_t *pcb = element_entry(tcp_pcb_t, node, node);
                node = node->next;

                pcb->idle++;
                if (pcb->rtt)
                    pcb->rtt++;

                for (size_t i = 0; i < TCP_TIMER_NUM; i++)
                {
                    if (pcb->timers[i] && (--pcb->timers[i]) == 0)
                    {
                        tcp_timeout(pcb, i);
                    }
                }
            }
        }
        task_sleep(TCP_SLOW_INTERVAL);
    }
}

void tcp_timer_init()
{
    tcp_fast = task_create(tcp_fastimo, "tcp_fast", 5, KERNEL_USER);
    tcp_slow = task_create(tcp_slowtimo, "tcp_slow", 5, KERNEL_USER);

    for (size_t i = 0; i <= TCP_MAXRXTCNT; i++)
    {
        if (i < 6)
            tcp_backoff[i] = 1 << i;
        else
            tcp_backoff[i] = 64;
    }
}