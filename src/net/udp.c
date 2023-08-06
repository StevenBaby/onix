#include <onix/net.h>
#include <onix/net/port.h>
#include <onix/net/socket.h>
#include <onix/fs.h>
#include <onix/arena.h>
#include <onix/task.h>
#include <onix/assert.h>
#include <onix/string.h>
#include <onix/debug.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

static list_t udp_pcb_list;
static port_map_t map;

static int udp_socket(socket_t *s, int domain, int type, int protocol)
{
    LOGK("udp socket...\n");
    assert(!s->udp);
    s->udp = kmalloc(sizeof(udp_pcb_t));
    memset(s->udp, 0, sizeof(udp_pcb_t));

    udp_pcb_t *pcb = s->udp;
    list_init(&pcb->rx_pbuf_list);
    list_push(&udp_pcb_list, &pcb->node);
    return EOK;
}

static int udp_close(socket_t *s)
{
    while (!list_empty(&s->udp->rx_pbuf_list))
    {
        pbuf_t *pbuf = element_entry(pbuf_t, node, list_popback(&s->udp->rx_pbuf_list));
        assert(pbuf->count == 1);
        pbuf_put(pbuf);
    }

    list_remove(&s->udp->node);

    if (s->udp->lport != 0)
        port_put(&map, s->udp->lport);

    kfree(s->udp);

    LOGK("udp close...\n");
    return EOK;
}

static int udp_bind(socket_t *s, const sockaddr_t *name, int namelen)
{
    LOGK("udp bind...\n");
    sockaddr_in_t *sin = (sockaddr_in_t *)name;
    if (!ip_addr_isany(sin->addr) && !ip_addr_isown(sin->addr))
        return -EADDR;

    ip_addr_copy(s->udp->laddr, sin->addr);

    int ret = port_get(&map, ntohs(sin->port));
    if (ret < 0)
        return ret;
    s->udp->lport = (u16)ret;
    return EOK;
}

static int udp_connect(socket_t *s, const sockaddr_t *name, int namelen)
{
    LOGK("udp connect...\n");
    sockaddr_in_t *sin = (sockaddr_in_t *)name;

    ip_addr_copy(s->udp->raddr, sin->addr);
    s->udp->rport = ntohs(sin->port);
    s->udp->flags |= UDP_FLAG_CONNECTED;
    if (s->udp->lport == 0)
        s->udp->lport = port_get(&map, 0);

    return EOK;
}

static int udp_getpeername(socket_t *s, sockaddr_t *name, int *namelen)
{
    LOGK("udp getpeername...\n");
    sockaddr_in_t *sin = (sockaddr_in_t *)name;

    sin->family = AF_INET;
    sin->port = htons(s->udp->rport);
    ip_addr_copy(sin->addr, s->udp->raddr);

    *namelen = sizeof(sockaddr_in_t);
    return EOK;
}

static int udp_getsockname(socket_t *s, sockaddr_t *name, int *namelen)
{
    LOGK("udp getsockname...\n");

    sockaddr_in_t *sin = (sockaddr_in_t *)name;

    sin->family = AF_INET;
    sin->port = htons(s->udp->lport);
    ip_addr_copy(sin->addr, s->udp->laddr);

    *namelen = sizeof(sockaddr_in_t);
    return EOK;
}

static udp_pcb_t *find_udp_pcb(ip_addr_t raddr, u16 rport, ip_addr_t laddr, u16 lport)
{
    list_t *list = &udp_pcb_list;
    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next)
    {
        udp_pcb_t *pcb = element_entry(udp_pcb_t, node, node);
        if (pcb->rport != rport)
            continue;
        if (pcb->lport != lport)
            continue;
        if (!ip_addr_isany(pcb->raddr) && !ip_addr_cmp(pcb->raddr, raddr))
            continue;
        if (!ip_addr_isany(pcb->laddr) && !ip_addr_cmp(pcb->laddr, laddr))
            continue;
        return pcb;
    }
    return NULL;
}

static int udp_recvmsg(socket_t *s, msghdr_t *msg, u32 flags)
{
    err_t ret = EOK;
    if (list_empty(&s->udp->rx_pbuf_list))
    {
        s->udp->rx_waiter = running_task();
        ret = task_block(s->udp->rx_waiter, NULL, TASK_WAITING, s->rcvtimeo);
        s->udp->rx_waiter = NULL;
    }

    if (ret != EOK)
        return ret;

    pbuf_t *pbuf = element_entry(pbuf_t, node, list_popback(&s->udp->rx_pbuf_list));
    udp_t *udp = pbuf->eth->ip->udp;

    u16 length = ntohs(udp->length) - sizeof(udp_t);
    ret = iovec_write(msg->iov, msg->iovlen, udp->payload, length);

    if (msg->name)
    {
        sockaddr_in_t *sin = (sockaddr_in_t *)msg->name;
        sin->family = AF_INET;
        sin->port = udp->sport;
        ip_addr_copy(sin->addr, pbuf->eth->ip->src);
    }
    msg->namelen = sizeof(sockaddr_in_t);

    pbuf_put(pbuf);
    return ret;
}

static int udp_output(netif_t *netif, udp_pcb_t *pcb, pbuf_t *pbuf, ip_addr_t dst, u16 dport, u16 len)
{
    if (!dst)
        dst = pcb->raddr;
    if (ip_addr_isany(dst))
        return -EADDR;
    if (dport == 0)
        dport = pcb->rport;
    if (dport == 0)
        return -ENOTCONN;

    if (ip_addr_isbroadcast(dst, netif->netmask) && !(pcb->flags & UDP_FLAG_BROADCAST))
        return -EACCES;

    ip_t *ip = pbuf->eth->ip;
    udp_t *udp = ip->udp;

    udp->sport = htons(pcb->lport);
    udp->dport = htons(dport);

    ip_addr_t *src;
    if (ip_addr_isany(pcb->laddr))
        src = &netif->ipaddr;
    else
        src = &pcb->laddr;

    u16 length = len + sizeof(udp_t);
    udp->length = htons(length);
    udp->chksum = 0;

    if (!(netif->flags & NETIF_UDP_TX_CHECKSUM_OFFLOAD))
    {
        udp->chksum = inet_chksum(udp, length, *src, pcb->raddr, IP_PROTOCOL_UDP);
    }

    return ip_output(netif, pbuf, pcb->raddr, IP_PROTOCOL_UDP, length);
}

static int udp_sendmsg(socket_t *s, msghdr_t *msg, u32 flags)
{
    size_t size = iovec_size(msg->iov, msg->iovlen);
    if (size > (IP_MTU - sizeof(ip_t) - sizeof(udp_t)))
        return -EMSGSIZE;

    int ret = EOK;
    if (s->udp->lport == 0)
    {
        sockaddr_in_t sin;
        memset(&sin, 0, sizeof(sin));
        sin.family = AF_INET;
        ret = udp_bind(s, (sockaddr_t *)&sin, sizeof(sin));
    }
    if (ret < EOK)
        return ret;

    pbuf_t *pbuf = pbuf_get();
    udp_t *udp = pbuf->eth->ip->udp;

    ret = iovec_read(msg->iov, msg->iovlen, udp->payload, size);
    if (ret < EOK)
        return ret;

    if (msg->name)
    {
        sockaddr_in_t *sin = (sockaddr_in_t *)msg->name;
        netif_t *netif = netif_route(sin->addr);
        ret = udp_output(netif, s->udp, pbuf, sin->addr, ntohs(sin->port), ret);
    }
    else
    {
        netif_t *netif = netif_route(s->udp->raddr);
        ret = udp_output(netif, s->udp, pbuf, NULL, 0, ret);
    }
    if (ret < EOK)
        return ret;

    return size;
}

int udp_input(netif_t *netif, pbuf_t *pbuf)
{
    ip_t *ip = pbuf->eth->ip;
    udp_t *udp = ip->udp;

    udp->sport = ntohs(udp->sport);
    udp->dport = ntohs(udp->dport);
    udp_pcb_t *pcb = find_udp_pcb(ip->src, udp->sport, ip->dst, udp->dport);
    if (!pcb)
    {
        LOGK("UDP unreachable.\n"); // TODO:
        return -EADDR;
    }

    pbuf->count++;
    list_push(&pcb->rx_pbuf_list, &pbuf->node);
    if (pcb->rx_waiter)
    {
        task_unblock(pcb->rx_waiter, EOK);
        pcb->rx_waiter = NULL;
    }

    return EOK;
}

static socket_op_t udp_op = {
    udp_socket,
    udp_close,
    fs_default_nosys, // listen
    fs_default_nosys, // accept,
    udp_bind,
    udp_connect,
    fs_default_nosys, // shutdown,

    udp_getpeername,
    udp_getsockname,
    fs_default_nosys, // getsockopt,
    fs_default_nosys, // setsockopt,

    udp_recvmsg,
    udp_sendmsg,
};

void udp_init()
{
    LOGK("udp init...\n");
    list_init(&udp_pcb_list);
    port_init(&map);
    socket_register_op(SOCK_TYPE_UDP, &udp_op);
}
