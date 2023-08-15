#include <onix/net/pkt.h>
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

static list_t pkt_pcb_list;

static int pkt_socket(socket_t *s, int domain, int type, int protocol)
{
    LOGK("pkt socket...\n");
    assert(!s->pkt);
    s->pkt = (pkt_pcb_t *)kmalloc(sizeof(pkt_pcb_t));
    memset(s->pkt, 0, sizeof(pkt_pcb_t));

    pkt_pcb_t *pcb = s->pkt;
    pcb->protocol = protocol;

    list_init(&pcb->rx_pbuf_list);
    list_push(&pkt_pcb_list, &pcb->node);
    return EOK;
}

static int pkt_close(socket_t *s)
{
    while (!list_empty(&s->pkt->rx_pbuf_list))
    {
        pbuf_t *pbuf = element_entry(pbuf_t, node, list_popback(&s->pkt->rx_pbuf_list));
        assert(pbuf->count == 1);
        pbuf_put(pbuf);
    }

    list_remove(&s->pkt->node);
    kfree(s->pkt);

    LOGK("pkt close...\n");
    return EOK;
}

static int pkt_bind(socket_t *s, const sockaddr_t *name, int namelen)
{
    sockaddr_ll_t *sin = (sockaddr_ll_t *)name;
    eth_addr_copy(s->pkt->laddr, sin->addr);
    return EOK;
}

static int pkt_connect(socket_t *s, const sockaddr_t *name, int namelen)
{
    sockaddr_ll_t *sin = (sockaddr_ll_t *)name;
    eth_addr_copy(s->pkt->raddr, sin->addr);
    return EOK;
}

static int pkt_getpeername(socket_t *s, sockaddr_t *name, int *namelen)
{
    sockaddr_ll_t *sin = (sockaddr_ll_t *)name;

    sin->family = AF_PACKET;
    eth_addr_copy(sin->addr, s->pkt->raddr);
    *namelen = sizeof(sockaddr_ll_t);
    return EOK;
}

static int pkt_getsockname(socket_t *s, sockaddr_t *name, int *namelen)
{
    sockaddr_ll_t *sin = (sockaddr_ll_t *)name;

    sin->family = AF_PACKET;
    eth_addr_copy(sin->addr, s->pkt->laddr);
    *namelen = sizeof(sockaddr_ll_t);
    return EOK;
}

static int pkt_setsockopt(socket_t *s, int level, int optname, const void *optval, int optlen)
{
    LOGK("pkt setsockopt...\n");
    switch (optname)
    {
    case SO_NETIF:
        if (optlen != 4)
            return -EINVAL;
        netif_t *netif = netif_get(*(int *)optval);
        if (!netif)
            return -EINVAL;
        s->pkt->netif = netif;
        return EOK;
    default:
        break;
    }
    return -ENOSYS;
}

static int pkt_recvmsg(socket_t *s, msghdr_t *msg, u32 flags)
{
    err_t ret = EOK;
    if (list_empty(&s->pkt->rx_pbuf_list))
    {
        s->pkt->rx_waiter = running_task();
        ret = task_block(s->pkt->rx_waiter, NULL, TASK_WAITING, s->rcvtimeo);
        s->pkt->rx_waiter = NULL;
    }

    if (ret != EOK)
        return ret;

    pbuf_t *pbuf = element_entry(pbuf_t, node, list_popback(&s->pkt->rx_pbuf_list));
    ret = iovec_write(msg->iov, msg->iovlen, pbuf->payload, pbuf->length);
    pbuf_put(pbuf);
    return ret;
}

static int pkt_sendmsg(socket_t *s, msghdr_t *msg, u32 flags)
{
    int ret = EOK;
    netif_t *netif = s->pkt->netif;
    if (!netif)
        return -EINVAL;

    size_t size = iovec_size(msg->iov, msg->iovlen);
    if (size > ETH_MTU)
        return -EMSGSIZE;

    if (msg->name)
        ret = pkt_connect(s, msg->name, msg->namelen);
    if (ret < EOK)
        return EOK;

    pbuf_t *pbuf = pbuf_get();

    ret = iovec_read(msg->iov, msg->iovlen, pbuf->payload, size);
    if (ret < EOK)
        return ret;
    pbuf->length = size;

    netif_output(netif, pbuf);
    return size;
}

static int pkt_recv(pkt_pcb_t *pcb, pbuf_t *pbuf)
{
    eth_t *eth = pbuf->eth;
    if (!eth_addr_isany(pcb->raddr) && !eth_addr_cmp(pcb->raddr, eth->src))
        return false;
    if (!eth_addr_isany(pcb->laddr) && !eth_addr_cmp(pcb->laddr, eth->dst))
        return false;

    switch (pcb->protocol)
    {
    case PROTO_IP:
        if (eth->type != ETH_TYPE_IP)
            return false;
        break;
    case PROTO_ICMP:
        if (eth->type != ETH_TYPE_IP)
            return false;
        if (eth->ip->proto != IP_PROTOCOL_ICMP)
            return false;
        break;
    case PROTO_UDP:
        if (eth->type != ETH_TYPE_IP)
            return false;
        if (eth->ip->proto != IP_PROTOCOL_UDP)
            return false;
        break;
    case PROTO_TCP:
        if (eth->type != ETH_TYPE_IP)
            return false;
        if (eth->ip->proto != IP_PROTOCOL_TCP)
            return false;
        break;
    default:
        break;
    }

    pbuf->count++;
    list_push(&pcb->rx_pbuf_list, &pbuf->node);
    if (pcb->rx_waiter)
    {
        task_unblock(pcb->rx_waiter, EOK);
        pcb->rx_waiter = NULL;
    }
    return true;
}

err_t pkt_input(netif_t *netif, pbuf_t *pbuf)
{
    int eaten = false;
    list_t *list = &pkt_pcb_list;
    for (list_node_t *ptr = list->head.next; ptr != &list->tail; ptr = ptr->next)
    {
        pkt_pcb_t *pcb = element_entry(pkt_pcb_t, node, ptr);
        if (netif != pcb->netif)
            continue;
        err_t ret = pkt_recv(pcb, pbuf);
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

static socket_op_t pkt_op = {
    pkt_socket,
    pkt_close,
    fs_default_nosys, // listen
    fs_default_nosys, // accept
    pkt_bind,         // bind
    pkt_connect,      // connect
    fs_default_nosys, // shutdown

    pkt_getsockname,  // getpeername
    pkt_getsockname,  // getsockname
    fs_default_nosys, // getsockopt
    pkt_setsockopt,   // setsockopt

    pkt_recvmsg,
    pkt_sendmsg,
};

void pkt_init()
{
    list_init(&pkt_pcb_list);
    socket_register_op(SOCK_TYPE_PKT, &pkt_op);
}
