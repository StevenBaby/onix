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

extern list_t tcp_pcb_active_list;   // 活动 pcb 列表
extern list_t tcp_pcb_listen_list;   // 监听 pcb 列表
extern list_t tcp_pcb_timewait_list; // 等待 pcb 列表

static port_map_t tcp_port_map; // 端口位图

u32 tcp_next_isn()
{
    return time() ^ ONIX_MAGIC;
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

    int ret = port_get(&tcp_port_map, ntohs(sin->port));
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
        pcb->lport = port_get(&tcp_port_map, 0);

    pcb->state = SYN_SENT;

    list_remove(&pcb->node);
    list_push(&tcp_pcb_active_list, &pcb->node);

    tcp_send_ack(pcb, TCP_SYN);

    pcb->ac_waiter = running_task();
    int ret = task_block(pcb->ac_waiter, NULL, TASK_WAITING, s->sndtimeo);
    pcb->ac_waiter = NULL;

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

extern void tcp_pcb_init();

void tcp_init()
{
    LOGK("TCP init...\n");

    tcp_pcb_init();

    port_init(&tcp_port_map);

    socket_register_op(SOCK_TYPE_TCP, &tcp_op);
}