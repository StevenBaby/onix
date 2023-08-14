#include <onix/net.h>
#include <onix/net/arp.h>
#include <onix/list.h>
#include <onix/arena.h>
#include <onix/syscall.h>
#include <onix/string.h>
#include <onix/task.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// ARP 缓存队列
static list_t arp_entry_list;

// ARP 刷新任务
static task_t *arp_task;

// arp 缓存
typedef struct arp_entry_t
{
    list_node_t node;  // 链表结点
    eth_addr_t hwaddr; // MAC 地址
    ip_addr_t ipaddr;  // IP 地址
    u32 expires;       // 失效时间
    u32 used;          // 使用次数
    u32 query;         // 查询时间
    u32 retry;         // 重试次数
    list_t pbuf_list;  // 等待队列
    netif_t *netif;    // 虚拟网卡
} arp_entry_t;

// 获取 ARP 缓存
static arp_entry_t *arp_entry_get(netif_t *netif, ip_addr_t addr)
{
    arp_entry_t *entry = (arp_entry_t *)kmalloc(sizeof(arp_entry_t));
    entry->netif = netif;
    ip_addr_copy(entry->ipaddr, addr);
    eth_addr_copy(entry->hwaddr, ETH_BROADCAST);

    entry->expires = 0;
    entry->retry = 0;
    entry->used = 1;

    list_init(&entry->pbuf_list);
    list_insert_sort(
        &arp_entry_list,
        &entry->node,
        element_node_offset(arp_entry_t, node, expires));
    return entry;
}

// 释放 ARP 缓存
static void arp_entry_put(arp_entry_t *entry)
{
    list_t *list = &entry->pbuf_list;
    while (!list_empty(list))
    {
        pbuf_t *pbuf = element_entry(pbuf_t, node, list_popback(list));
        assert(pbuf->count <= 2);
        pbuf_put(pbuf);
    }

    list_remove(&entry->node);
    kfree(entry);
}

static arp_entry_t *arp_lookup(netif_t *netif, ip_addr_t addr)
{
    ip_addr_t query;
    if (!ip_addr_maskcmp(netif->ipaddr, addr, netif->netmask))
        ip_addr_copy(query, netif->gateway);
    else
        ip_addr_copy(query, addr);

    if (ip_addr_isany(query))
        return NULL;

    list_t *list = &arp_entry_list;
    arp_entry_t *entry = NULL;

    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next)
    {
        entry = element_entry(arp_entry_t, node, node);
        if (ip_addr_cmp(entry->ipaddr, query) && (netif == entry->netif))
        {
            return entry;
        }
    }

    entry = arp_entry_get(netif, query);
    return entry;
}

// ARP 查询
static err_t arp_query(arp_entry_t *entry)
{
    if (entry->query + ARP_DELAY > time())
    {
        return -ETIME;
    }

    LOGK("ARP query %r...\n", entry->ipaddr);

    entry->query = time();
    entry->retry++;

    if (entry->retry > 2)
    {
        eth_addr_copy(entry->hwaddr, ETH_BROADCAST);
    }

    pbuf_t *pbuf = pbuf_get();
    arp_t *arp = pbuf->eth->arp;

    arp->opcode = htons(ARP_OP_REQUEST);
    arp->hwtype = htons(ARP_HARDWARE_ETH);
    arp->hwlen = ARP_HARDWARE_ETH_LEN;
    arp->proto = htons(ARP_PROTOCOL_IP);
    arp->protolen = ARP_PROTOCOL_IP_LEN;

    eth_addr_copy(arp->hwdst, entry->hwaddr);
    ip_addr_copy(arp->ipdst, entry->ipaddr);
    ip_addr_copy(arp->ipsrc, entry->netif->ipaddr);
    eth_addr_copy(arp->hwsrc, entry->netif->hwaddr);

    eth_output(entry->netif, pbuf, entry->hwaddr, ETH_TYPE_ARP, sizeof(arp_t));
    return EOK;
}

// 刷新 ARP 缓存
static err_t arp_refresh(netif_t *netif, pbuf_t *pbuf)
{
    arp_t *arp = pbuf->eth->arp;

    if (!ip_addr_maskcmp(arp->ipsrc, netif->ipaddr, netif->netmask))
        return -EADDR;

    arp_entry_t *entry = arp_lookup(netif, arp->ipsrc);
    if (!entry)
        return -EADDR;

    eth_addr_copy(entry->hwaddr, arp->hwsrc);
    entry->expires = time() + ARP_ENTRY_TIMEOUT;
    entry->retry = 0;
    entry->used = 0;

    list_remove(&entry->node);
    list_insert_sort(
        &arp_entry_list,
        &entry->node,
        element_node_offset(arp_entry_t, node, expires));

    list_t *list = &entry->pbuf_list;
    while (!list_empty(list))
    {
        pbuf_t *pbuf = element_entry(pbuf_t, node, list_popback(list));
        eth_addr_copy(pbuf->eth->dst, entry->hwaddr);
        netif_output(netif, pbuf);
    }

    LOGK("ARP reply %r -> %m \n", arp->ipsrc, arp->hwsrc);
    return EOK;
}

static err_t arp_reply(netif_t *netif, pbuf_t *pbuf)
{
    arp_t *arp = pbuf->eth->arp;

    LOGK("ARP Request from %r\n", arp->ipsrc);

    arp->opcode = htons(ARP_OP_REPLY);

    eth_addr_copy(arp->hwdst, arp->hwsrc);
    ip_addr_copy(arp->ipdst, arp->ipsrc);

    eth_addr_copy(arp->hwsrc, netif->hwaddr);
    ip_addr_copy(arp->ipsrc, netif->ipaddr);

    pbuf->count++;
    return eth_output(netif, pbuf, arp->hwdst, ETH_TYPE_ARP, sizeof(arp_t));
}

err_t arp_input(netif_t *netif, pbuf_t *pbuf)
{
    arp_t *arp = pbuf->eth->arp;

    // 只支持 以太网
    if (ntohs(arp->hwtype) != ARP_HARDWARE_ETH)
        return -EPROTO;

    // 只支持 IP
    if (ntohs(arp->proto) != ARP_PROTOCOL_IP)
        return -EPROTO;

    // 如果请求的目的地址不是本机 ip 则忽略
    if (!ip_addr_cmp(netif->ipaddr, arp->ipdst))
        return -EPROTO;

    u16 type = ntohs(arp->opcode);
    switch (type)
    {
    case ARP_OP_REQUEST:
        return arp_reply(netif, pbuf);
    case ARP_OP_REPLY:
        return arp_refresh(netif, pbuf);
    default:
        return -EPROTO;
    }
    return EOK;
}

// 发送数据包到对应的 IP 地址
err_t arp_eth_output(netif_t *netif, pbuf_t *pbuf, ip_addr_t addr, u16 type, u32 len)
{
    pbuf->eth->type = htons(type);
    eth_addr_copy(pbuf->eth->src, netif->hwaddr);
    pbuf->length = sizeof(eth_t) + len;

    if (netif->flags & NETIF_LOOPBACK)
    {
        eth_addr_copy(pbuf->eth->dst, "\x00\x00\x00\x00\x00\x00");
        netif_output(netif, pbuf);
        return EOK;
    }

    if (ip_addr_isown(addr))
    {
        eth_addr_copy(pbuf->eth->dst, netif->hwaddr);
        netif_input(netif, pbuf);
        return EOK;
    }

    arp_entry_t *entry = arp_lookup(netif, addr);
    if (!entry)
        return -EADDR;

    if (entry->expires > time())
    {
        entry->used += 1;
        eth_addr_copy(pbuf->eth->dst, entry->hwaddr);
        netif_output(netif, pbuf);
        return EOK;
    }

    list_push(&entry->pbuf_list, &pbuf->node);
    arp_query(entry);
    return EOK;
}

// ARP 刷新线程
static void arp_thread()
{
    list_t *list = &arp_entry_list;
    while (true)
    {
        // LOGK("ARP timer %d...\n", time());
        task_sleep(ARP_REFRESH_DELAY);
        for (list_node_t *node = list->tail.prev; node != &list->head;)
        {
            arp_entry_t *entry = element_entry(arp_entry_t, node, node);

            node = node->prev;

            if (entry->expires > time())
                continue;

            if (entry->retry > ARP_RETRY)
            {
                LOGK("ARP retries %d times for %r...\n",
                     entry->retry, entry->ipaddr);
                arp_entry_put(entry);
                continue;
            }

            if (entry->used == 0)
            {
                LOGK("ARP entry %r timeout...\n", entry->ipaddr);
                arp_entry_put(entry);
                continue;
            }

            arp_query(entry);
        }
    }
}

// 初始化 ARP 协议
void arp_init()
{
    LOGK("Address Resolution Protocol init...\n");
    list_init(&arp_entry_list);
    arp_task = task_create(arp_thread, "arp", 5, KERNEL_USER);
}
