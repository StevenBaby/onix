#include <onix/net/raw.h>
#include <onix/net/socket.h>
#include <onix/net.h>
#include <onix/fs.h>
#include <onix/arena.h>
#include <onix/task.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

static list_t raw_pcb_list;

static int raw_socket(socket_t *s, int domain, int type, int protocol)
{
    LOGK("raw socket...\n");
    assert(!s->raw);
    s->raw = (raw_pcb_t *)kmalloc(sizeof(raw_pcb_t));
    memset(s->raw, 0, sizeof(raw_pcb_t));

    raw_pcb_t *pcb = s->raw;
    pcb->protocol = protocol;
    list_init(&pcb->rx_pbuf_list);
    list_push(&raw_pcb_list, &pcb->node);

    return EOK;
}

static int raw_close(socket_t *s)
{
    while (!list_empty(&s->raw->rx_pbuf_list))
    {
        pbuf_t *pbuf = element_entry(pbuf_t, node, list_popback(&s->raw->rx_pbuf_list));
        assert(pbuf->count == 1);
        pbuf_put(pbuf);
    }

    list_remove(&s->raw->node);
    kfree(s->raw);

    LOGK("raw close...\n");
    return EOK;
}

static int raw_bind(socket_t *s, const sockaddr_t *name, int namelen)
{
    sockaddr_in_t *sin = (sockaddr_in_t *)name;
    ip_addr_copy(s->raw->laddr, sin->addr);
    return EOK;
}

static int raw_connect(socket_t *s, const sockaddr_t *name, int namelen)
{
    sockaddr_in_t *sin = (sockaddr_in_t *)name;
    ip_addr_copy(s->raw->raddr, sin->addr);
    return EOK;
}

static int raw_getpeername(socket_t *s, sockaddr_t *name, int *namelen)
{
    sockaddr_in_t *sin = (sockaddr_in_t *)name;

    sin->family = AF_INET;
    sin->port = 0;
    ip_addr_copy(sin->addr, s->raw->raddr);
    *namelen = sizeof(sockaddr_in_t);

    return EOK;
}

static int raw_getsockname(socket_t *s, sockaddr_t *name, int *namelen)
{
    sockaddr_in_t *sin = (sockaddr_in_t *)name;

    sin->family = AF_INET;
    sin->port = 0;
    ip_addr_copy(sin->addr, s->raw->laddr);
    *namelen = sizeof(sockaddr_in_t);

    return EOK;
}

static int raw_recvmsg(socket_t *s, msghdr_t *msg, u32 flags)
{
    err_t ret = EOK;
    if (list_empty(&s->raw->rx_pbuf_list))
    {
        s->raw->rx_waiter = running_task();
        ret = task_block(s->raw->rx_waiter, NULL, TASK_WAITING, s->rcvtimeo);
        s->raw->rx_waiter = NULL;
    }

    if (ret != EOK)
        return ret;

    pbuf_t *pbuf = element_entry(pbuf_t, node, list_popback(&s->raw->rx_pbuf_list));
    ret = iovec_write(msg->iov, msg->iovlen, pbuf->eth->payload, pbuf->length - sizeof(eth_t));

    if (msg->name)
    {
        sockaddr_in_t *sin = (sockaddr_in_t *)msg->name;
        sin->family = AF_INET;
        sin->port = 0;
        ip_addr_copy(sin->addr, pbuf->eth->ip->src);
    }
    msg->namelen = sizeof(sockaddr_in_t);

    pbuf_put(pbuf);
    return ret;
}

static int raw_sendmsg(socket_t *s, msghdr_t *msg, u32 flags)
{
    int ret = EOK;
    size_t size = iovec_size(msg->iov, msg->iovlen);
    if (size > IP_MTU)
        return -EMSGSIZE;

    if (msg->name)
        ret = raw_connect(s, msg->name, msg->namelen);
    if (ret < EOK)
        return ret;

    pbuf_t *pbuf = pbuf_get();

    ret = iovec_read(msg->iov, msg->iovlen, pbuf->eth->payload, size);
    if (ret < EOK)
        return ret;

    netif_t *netif = netif_route(pbuf->eth->ip->dst);

    ret = ip_output(netif, pbuf, pbuf->eth->ip->dst, pbuf->eth->ip->proto, size - sizeof(ip_t));
    if (ret < EOK)
        return ret;
    return size;
}

static int raw_recv(raw_pcb_t *pcb, pbuf_t *pbuf)
{
    ip_t *ip = pbuf->eth->ip;
    if (!ip_addr_isany(pcb->raddr) && !ip_addr_cmp(pcb->raddr, ip->src))
        return false;
    if (!ip_addr_isany(pcb->laddr) && !ip_addr_cmp(pcb->laddr, ip->dst))
        return false;

    if (pcb->protocol != PROTO_IP && pcb->protocol != ip->proto)
        return false;

    pbuf->count++;
    list_push(&pcb->rx_pbuf_list, &pbuf->node);
    if (pcb->rx_waiter)
    {
        task_unblock(pcb->rx_waiter, EOK);
        pcb->rx_waiter = NULL;
    }
    return true;
}

err_t raw_input(netif_t *netif, pbuf_t *pbuf)
{
    int eaten = false;

    list_t *list = &raw_pcb_list;
    for (list_node_t *ptr = list->head.next; ptr != &list->tail; ptr = ptr->next)
    {
        raw_pcb_t *pcb = element_entry(raw_pcb_t, node, ptr);
        err_t ret = raw_recv(pcb, pbuf);
        if (ret < 0)
            return ret;
        if (ret > 0)
        {
            eaten = true;
            break;
        }
    }
    return eaten;
}

static socket_op_t raw_op = {
    raw_socket,
    raw_close,
    fs_default_nosys, // listen,
    fs_default_nosys, // accept,
    raw_bind,
    raw_connect,
    fs_default_nosys, // shutdown,

    raw_getpeername,
    raw_getsockname,
    fs_default_nosys, // getsockopt,
    fs_default_nosys, // setsockopt,

    raw_recvmsg,
    raw_sendmsg,
};

void raw_init()
{
    list_init(&raw_pcb_list);
    socket_register_op(SOCK_TYPE_RAW, &raw_op);
}
