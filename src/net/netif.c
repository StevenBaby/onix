#include <onix/net.h>
#include <onix/list.h>
#include <onix/task.h>
#include <onix/device.h>
#include <onix/arena.h>
#include <onix/string.h>
#include <onix/stdio.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/syscall.h>
#include <onix/debug.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args);

static list_t netif_list; // 虚拟网卡列表

static task_t *neti_task; // 接收任务
static task_t *neto_task; // 发送任务

netif_t *netif_create()
{
    netif_t *netif = kmalloc(sizeof(netif_t));
    memset(netif, 0, sizeof(netif_t));
    lock_init(&netif->rx_lock);
    lock_init(&netif->tx_lock);

    sprintf(netif->name, "eth%d", list_size(&netif_list));

    list_push(&netif_list, &netif->node);
    list_init(&netif->rx_pbuf_list);
    list_init(&netif->tx_pbuf_list);
    return netif;
}

// 初始化虚拟网卡
netif_t *netif_setup(void *nic, eth_addr_t hwaddr, void *output)
{
    netif_t *netif = netif_create();
    eth_addr_copy(netif->hwaddr, hwaddr);

    assert(inet_aton("192.168.111.33", netif->ipaddr) == EOK);
    assert(inet_aton("255.255.255.0", netif->netmask) == EOK);
    assert(inet_aton("192.168.111.2", netif->gateway) == EOK);

    netif->nic = nic;
    netif->nic_output = output;

    return netif;
}

// 获取虚拟网卡
netif_t *netif_get()
{
    list_t *list = &netif_list;
    if (list_empty(list))
        return NULL;

    list_node_t *ptr = list->head.next;
    netif_t *netif = element_entry(netif_t, node, ptr);
    return netif;
}

netif_t *netif_route(ip_addr_t addr)
{
    list_t *list = &netif_list;
    for (list_node_t *ptr = list->head.next; ptr != &list->tail; ptr = ptr->next)
    {
        netif_t *netif = element_entry(netif_t, node, ptr);
        if (ip_addr_maskcmp(addr, netif->ipaddr, netif->netmask))
        {
            return netif;
        }
    }
    return netif_get(); // TODO route
}

// 移除虚拟网卡
void netif_remove(netif_t *netif)
{
    list_remove(&netif->node);
    kfree(netif);
}

// 判断 IP 地址是不是自己
bool ip_addr_isown(ip_addr_t addr)
{
    list_t *list = &netif_list;
    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next)
    {
        netif_t *netif = element_entry(netif_t, node, node);
        if (ip_addr_cmp(netif->ipaddr, addr))
            return true;
    }
    return false;
}

// 网卡接收任务输入
void netif_input(netif_t *netif, pbuf_t *pbuf)
{
    lock_acquire(&netif->rx_lock);

    while (netif->rx_pbuf_size >= NETIF_RX_PBUF_SIZE)
    {
        pbuf_t *nbuf = element_entry(pbuf_t, node, list_popback(&netif->rx_pbuf_list));
        netif->rx_pbuf_size--;
        pbuf_put(nbuf);
    }

    list_push(&netif->rx_pbuf_list, &pbuf->node);
    netif->rx_pbuf_size++;
    assert(netif->rx_pbuf_size <= NETIF_RX_PBUF_SIZE);

    if (neti_task->state == TASK_WAITING)
    {
        task_unblock(neti_task, EOK);
    }
    lock_release(&netif->rx_lock);
}

// 网卡发送任务输出
void netif_output(netif_t *netif, pbuf_t *pbuf)
{
    lock_acquire(&netif->tx_lock);
    list_push(&netif->tx_pbuf_list, &pbuf->node);
    netif->tx_pbuf_size++;
    if (neto_task->state == TASK_WAITING)
    {
        task_unblock(neto_task, EOK);
    }
    lock_release(&netif->tx_lock);
}

// 接收任务
static void neti_thread()
{
    list_t *list = &netif_list;
    pbuf_t *pbuf;
    netif_t *netif;
    while (true)
    {
        int count = 0;
        for (list_node_t *ptr = list->head.next; ptr != &list->tail; ptr = ptr->next)
        {
            netif = element_entry(netif_t, node, ptr);

            if (!list_empty(&netif->rx_pbuf_list))
            {
                lock_acquire(&netif->rx_lock);
                pbuf = element_entry(pbuf_t, node, list_popback(&netif->rx_pbuf_list));
                assert(!pbuf->node.next && !pbuf->node.prev);
                netif->rx_pbuf_size--;
                lock_release(&netif->rx_lock);

                // LOGK("ETH RECV [%04X]: %m -> %m %d\n",
                //      ntohs(pbuf->eth->type),
                //      pbuf->eth->src,
                //      pbuf->eth->dst,
                //      pbuf->length);

                eth_input(netif, pbuf);

                pbuf_put(pbuf);
                count++;
            }
        }

        // 没有数据包，阻塞处理线程
        if (!count)
        {
            task_t *task = running_task();
            assert(task == neti_task);
            int ret = task_block(task, NULL, TASK_WAITING, TIMELESS);
            assert(ret == EOK);
        }
    }
}

// 发送任务
static void neto_thread()
{
    list_t *list = &netif_list;
    pbuf_t *pbuf;
    netif_t *netif;
    while (true)
    {
        int count = 0;
        for (list_node_t *ptr = list->head.next; ptr != &list->tail; ptr = ptr->next)
        {
            netif = element_entry(netif_t, node, ptr);
            if (!list_empty(&netif->tx_pbuf_list))
            {
                lock_acquire(&netif->tx_lock);
                pbuf = element_entry(pbuf_t, node, list_popback(&netif->tx_pbuf_list));
                assert(!pbuf->node.next && !pbuf->node.prev);
                netif->tx_pbuf_size--;
                lock_release(&netif->tx_lock);

                netif->nic_output(netif, pbuf);

                // if (ntohs(pbuf->eth->type) == ETH_TYPE_IP &&
                //     pbuf->eth->ip->proto == IP_PROTOCOL_TCP)
                // {
                //     // 测试 RTT
                //     task_sleep((time() & 0x7) * 500);
                // }

                // LOGK("ETH SEND [%04X]: %m -> %m %d\n",
                //      ntohs(pbuf->eth->type),
                //      pbuf->eth->src,
                //      pbuf->eth->dst,
                //      pbuf->length);

                count++;
            }
        }

        // 没有数据包，阻塞处理线程
        if (!count)
        {
            task_t *task = running_task();
            assert(task == neto_task);
            int ret = task_block(task, NULL, TASK_WAITING, TIMELESS);
            assert(ret == EOK);
        }
    }
}

// 初始化虚拟网卡
void netif_init()
{
    list_init(&netif_list);
    neti_task = task_create(neti_thread, "neti", 5, KERNEL_USER);
    neto_task = task_create(neto_thread, "neio", 5, KERNEL_USER);
}