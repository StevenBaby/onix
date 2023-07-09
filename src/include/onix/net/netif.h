#ifndef ONIX_NET_NETIF_H
#define ONIX_NET_NETIF_H

#include <onix/list.h>
#include "types.h"

typedef struct netif_t
{
    list_node_t node; // 链表节点
    char name[16];    // 名字

    list_t rx_pbuf_list; // 接收缓冲队列
    list_t tx_pbuf_list; // 发送缓冲队列

    eth_addr_t hwaddr; // MAC 地址

    ip_addr_t ipaddr;  // IP 地址
    ip_addr_t netmask; // 子网掩码
    ip_addr_t gateway; // 默认网关

    void *nic; // 设备指针
    void (*nic_output)(struct netif_t *netif, pbuf_t *pbuf);
} netif_t;

// 初始化虚拟网卡
netif_t *netif_setup(void *nic, eth_addr_t hwaddr, void *output);

// 获取虚拟网卡
netif_t *netif_get();

// IP 路由选择
netif_t *netif_route(ip_addr_t addr);

// 移除虚拟网卡
void netif_remove(netif_t *netif);

// 网卡接收任务输入
void netif_input(netif_t *netif, pbuf_t *pbuf);

// 网卡发送任务输出
void netif_output(netif_t *netif, pbuf_t *pbuf);

#endif