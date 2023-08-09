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

    tcp_enqueue(pcb, NULL, 0, TCP_SYN);
    tcp_output(pcb);

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
    err_t ret = EOK;

    tcp_pcb_t *pcb = s->tcp;

    if (list_empty(&pcb->recved))
    {
        pcb->rx_waiter = running_task();
        ret = task_block(pcb->rx_waiter, NULL, TASK_WAITING, s->rcvtimeo);
        pcb->rx_waiter = NULL;
    }

    if (ret < EOK)
        return ret;

    if (msg->name)
    {
        sockaddr_in_t *sin = (sockaddr_in_t *)msg->name;
        sin->family = AF_INET;
        sin->port = pcb->rport;
        ip_addr_copy(sin->addr, pcb->raddr);
    }

    msg->namelen = sizeof(sockaddr_in_t);

    size_t size = iovec_size(msg->iov, msg->iovlen);
    assert(size > 0);
    size_t left = size;

    list_t *list = &pcb->recved;
    for (list_node_t *node = list->head.next; node != &list->tail;)
    {
        pbuf_t *pbuf = element_entry(pbuf_t, tcpnode, node);
        node = node->next;

        int len = (left < pbuf->size) ? left : pbuf->size;
        ret = iovec_write(msg->iov, msg->iovlen, pbuf->data, len);
        assert(ret == len);

        pcb->rcv_wnd += len;
        assert(pcb->rcv_wnd <= TCP_WINDOW);

        left -= ret;

        if (len < pbuf->size)
        {
            pbuf->data += left;
            pbuf->size -= left;
            break;
        }
        else
        {
            assert(pbuf->count == 1);
            list_remove(&pbuf->tcpnode);
            pbuf_put(pbuf);
        }
    }
    return size - left;
}

static int tcp_sendmsg(socket_t *s, msghdr_t *msg, u32 flags)
{
    LOGK("tcp sendmsg...\n");
    err_t ret = EOK;
    tcp_pcb_t *pcb = s->tcp;
    if (pcb->state != ESTABLISHED)
        return -EINVAL;

    size_t size = iovec_size(msg->iov, msg->iovlen);
    if (size > pcb->snd_wnd)
        return -EMSGSIZE;

    size_t left = size;

    iovec_t *iov = msg->iov;
    size_t iovlen = msg->iovlen;

    for (; left > 0 && iovlen > 0; iov++, iovlen--)
    {
        if (iov->size <= 0)
            continue;

        int len = left < iov->size ? left : iov->size;
        tcp_enqueue(pcb, iov->base, left, flags);
        left -= len;
    }
    tcp_output(pcb);

    pcb->tx_waiter = running_task();
    ret = task_block(pcb->tx_waiter, NULL, TASK_WAITING, s->sndtimeo);
    pcb->tx_waiter = NULL;

    if (ret < 0)
        return ret;

    return size - left;
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
