#include <onix/net/tcp.h>
#include <onix/net/port.h>
#include <onix/net/socket.h>
#include <onix/net.h>
#include <onix/list.h>
#include <onix/arena.h>
#include <onix/task.h>
#include <onix/syscall.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

list_t tcp_pcb_create_list;   // 创建 pcb 列表
list_t tcp_pcb_active_list;   // 活动 pcb 列表
list_t tcp_pcb_listen_list;   // 监听 pcb 列表
list_t tcp_pcb_timewait_list; // 等待 pcb 列表

static port_map_t map; // 端口位图

u32 tcp_next_isn()
{
    return time() ^ ONIX_MAGIC;
}

tcp_pcb_t *tcp_pcb_get()
{
    LOGK("tcp pcb get...\n");
    tcp_pcb_t *pcb = (tcp_pcb_t *)kmalloc(sizeof(tcp_pcb_t));
    memset(pcb, 0, sizeof(tcp_pcb_t));

    list_init(&pcb->unsent);
    list_init(&pcb->unacked);
    list_init(&pcb->outseq);
    list_init(&pcb->recved);

    list_push(&tcp_pcb_create_list, &pcb->node);
    return pcb;
}

void tcp_pcb_put(tcp_pcb_t *pcb)
{
    if (!pcb)
        return;
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

static int tcp_socket(socket_t *s, int domain, int type, int protocol)
{
    LOGK("tcp socket...\n");
    assert(!s->tcp);
    s->tcp = tcp_pcb_get();
    return EOK;
}

static int tcp_close(socket_t *s)
{
    LOGK("tcp close...\n");

    tcp_pcb_put(s->tcp);
    return EOK;
}

static int tcp_listen(socket_t *s, int backlog)
{
    LOGK("tcp listen...\n");
    return EOK;
}

static int tcp_accept(socket_t *s, sockaddr_t *addr, int *addrlen, socket_t **ns)
{
    LOGK("tcp accept...\n");
    return -EINVAL;
}

static int tcp_bind(socket_t *s, const sockaddr_t *name, int namelen)
{
    LOGK("tcp bind...\n");
    sockaddr_in_t *sin = (sockaddr_in_t *)name;
    if (!ip_addr_isany(sin->addr) && !ip_addr_isown(sin->addr))
        return -EADDR;

    int ret = port_get(&map, ntohs(sin->port));
    if (ret < 0)
        return ret;

    u16 lport = (u16)ret;

    tcp_pcb_t *pcb = tcp_find_pcb(sin->addr, lport, NULL, 0);
    if (pcb != NULL)
        return -EOCCUPIED;

    pcb = s->tcp;
    ip_addr_copy(pcb->laddr, sin->addr);
    pcb->lport = lport;
    return EOK;
}

static int tcp_connect(socket_t *s, const sockaddr_t *name, int namelen)
{
    LOGK("tcp connect...\n");
    sockaddr_in_t *sin = (sockaddr_in_t *)name;
    tcp_pcb_t *pcb = s->tcp;

    if (pcb->state != CLOSED)
    {
        return -EINVAL;
    }

    if (ip_addr_isany(sin->addr) || ntohs(sin->port) == 0)
        return -EADDR;

    ip_addr_copy(pcb->raddr, sin->addr);
    pcb->rport = ntohs(sin->port);

    if (!pcb->lport)
        pcb->lport = port_get(&map, 0);

    pcb->rcv_nxt = 0;
    pcb->rcv_mss = TCP_MSS;
    pcb->rcv_wnd = TCP_WINDOW;

    pcb->snd_nxt = tcp_next_isn();

    pcb->state = SYN_SENT;

    list_remove(&pcb->node);
    list_push(&tcp_pcb_active_list, &pcb->node);

    tcp_send_ack(pcb, TCP_SYN);

    pcb->ac_waiter = running_task();
    int ret = task_block(pcb->ac_waiter, NULL, TASK_WAITING, s->sndtimeo);
    return ret;
}

static int tcp_shutdown(socket_t *s, int how)
{
    LOGK("tcp shutdown...\n");
    return -ENOSYS;
}

static int tcp_getpeername(socket_t *s, sockaddr_t *name, int *namelen)
{
    LOGK("tcp getpeername...\n");
    sockaddr_in_t *sin = (sockaddr_in_t *)name;

    sin->family = AF_INET;
    sin->port = htons(s->tcp->rport);
    ip_addr_copy(sin->addr, s->tcp->raddr);

    *namelen = sizeof(sockaddr_in_t);
    return EOK;
}

static int tcp_getsockname(socket_t *s, sockaddr_t *name, int *namelen)
{
    LOGK("tcp getsockname...\n");

    sockaddr_in_t *sin = (sockaddr_in_t *)name;

    sin->family = AF_INET;
    sin->port = htons(s->tcp->lport);
    ip_addr_copy(sin->addr, s->tcp->laddr);

    *namelen = sizeof(sockaddr_in_t);
    return EOK;
}

static int tcp_getsockopt(socket_t *s, int level, int optname, void *optval, int *optlen)
{
    LOGK("tcp getsockopt...\n");
    return -ENOSYS;
}

static int tcp_setsockopt(socket_t *s, int level, int optname, const void *optval, int optlen)
{
    LOGK("tcp setsockopt...\n");
    return -ENOSYS;
}

static int tcp_recvmsg(socket_t *s, msghdr_t *msg, u32 flags)
{
    LOGK("tcp recvmsg...\n");
    return -EINVAL;
}

static int tcp_sendmsg(socket_t *s, msghdr_t *msg, u32 flags)
{
    LOGK("tcp sendmsg...\n");
    return -EINVAL;
}

static socket_op_t tcp_op = {
    tcp_socket,
    tcp_close,
    tcp_listen,
    tcp_accept,
    tcp_bind,
    tcp_connect,
    tcp_shutdown,

    tcp_getpeername,
    tcp_getsockname,
    tcp_getsockopt,
    tcp_setsockopt,

    tcp_recvmsg,
    tcp_sendmsg,
};

void tcp_init()
{
    LOGK("TCP init...\n");
    list_init(&tcp_pcb_create_list);
    list_init(&tcp_pcb_active_list);
    list_init(&tcp_pcb_timewait_list);
    list_init(&tcp_pcb_listen_list);

    port_init(&map);

    socket_register_op(SOCK_TYPE_TCP, &tcp_op);
}