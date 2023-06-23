#include <onix/net.h>
#include <onix/list.h>
#include <onix/task.h>
#include <onix/device.h>
#include <onix/arena.h>
#include <onix/stdio.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args);

static list_t netif_list; // 虚拟网卡列表

static task_t *neti_task; // 接收任务
static task_t *neto_task; // 发送任务

// 初始化虚拟网卡
netif_t *netif_setup(void *nic, eth_addr_t hwaddr, void *output)
{
    netif_t *netif = kmalloc(sizeof(netif_t));
    sprintf(netif->name, "eth%d", list_size(&netif_list));

    list_push(&netif_list, &netif->node);
    list_init(&netif->rx_pbuf_list);
    list_init(&netif->tx_pbuf_list);

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

// 移除虚拟网卡
void netif_remove(netif_t *netif)
{
    list_remove(&netif->node);
    kfree(netif);
}

// 网卡接收任务输入
void netif_input(netif_t *netif, pbuf_t *pbuf)
{
    list_push(&netif->rx_pbuf_list, &pbuf->node);
    if (neti_task->state == TASK_WAITING)
    {
        task_unblock(neti_task, EOK);
    }
}

// 网卡发送任务输出
void netif_output(netif_t *netif, pbuf_t *pbuf)
{
    list_push(&netif->tx_pbuf_list, &pbuf->node);
    if (neto_task->state == TASK_WAITING)
    {
        task_unblock(neto_task, EOK);
    }
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
                pbuf = element_entry(pbuf_t, node, list_popback(&netif->rx_pbuf_list));
                assert(!pbuf->node.next && !pbuf->node.prev);

                LOGK("ETH RECV [%04X]: %m -> %m %d\n",
                     ntohs(pbuf->eth->type),
                     pbuf->eth->src,
                     pbuf->eth->dst,
                     pbuf->length);

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
                pbuf = element_entry(pbuf_t, node, list_popback(&netif->tx_pbuf_list));
                assert(!pbuf->node.next && !pbuf->node.prev);

                netif->nic_output(netif, pbuf);

                LOGK("ETH SEND [%04X]: %m -> %m %d\n",
                     ntohs(pbuf->eth->type),
                     pbuf->eth->src,
                     pbuf->eth->dst,
                     pbuf->length);

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