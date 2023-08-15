#ifndef ONIX_NET_NETIF_H
#define ONIX_NET_NETIF_H

#include <onix/list.h>
#include <onix/mutex.h>
#include "types.h"

enum
{
    NETIF_LOOPBACK = 0x00000001,

    NETIF_IP_TX_CHECKSUM_OFFLOAD = 0x00010000,
    NETIF_IP_RX_CHECKSUM_OFFLOAD = 0x00020000,
    NETIF_UDP_RX_CHECKSUM_OFFLOAD = 0x00040000,
    NETIF_UDP_TX_CHECKSUM_OFFLOAD = 0x00080000,
    NETIF_TCP_RX_CHECKSUM_OFFLOAD = 0x00100000,
    NETIF_TCP_TX_CHECKSUM_OFFLOAD = 0x00200000,
};

enum
{
    NETIF_RX_PBUF_SIZE = 8,
    NETIF_TX_PBUF_SIZE = 8,
};

typedef struct netif_t
{
    list_node_t node; // 链表节点
    char name[16];    // 名字

    list_t rx_pbuf_list; // 接收缓冲队列
    lock_t rx_lock;      // 接收锁
    int rx_pbuf_size;    // 接收队列大小

    list_t tx_pbuf_list; // 发送缓冲队列
    lock_t tx_lock;      // 发送锁
    int tx_pbuf_size;    // 发送队列大小

    eth_addr_t hwaddr; // MAC 地址

    ip_addr_t ipaddr;    // IP 地址
    ip_addr_t netmask;   // 子网掩码
    ip_addr_t gateway;   // 默认网关
    ip_addr_t broadcast; // 广播地址

    void *nic; // 设备指针
    void (*nic_output)(struct netif_t *netif, pbuf_t *pbuf);

    idx_t index;
    u32 flags;
} netif_t;

typedef struct ifreq_t
{
    char name[16];
    union
    {
        idx_t index;         // 索引
        ip_addr_t ipaddr;    // IP 地址
        ip_addr_t netmask;   // 子网掩码
        ip_addr_t gateway;   // 默认网关
        ip_addr_t broadcast; // 广播地址
        eth_addr_t hwaddr;   // MAC 地址
    };
} ifreq_t;

enum
{
    SIOCGIFNAME,  // 通过网卡索引获取网卡名称
    SIOCGIFINDEX, // 通过网卡名称获取网卡索引

    SIOCGIFADDR, // 获取 IP 地址
    SIOCSIFADDR, // 设置 IP 地址
    SIOCDIFADDR, // 删除 IP 地址

    SIOCGIFBRDADDR, // 获取广播地址
    SIOCSIFBRDADDR, // 设置广播地址

    SIOCGIFNETMASK, // 获取子网掩码
    SIOCSIFNETMASK, // 设置子网掩码

    SIOCGIFGATEWAY, // 获取默认网关
    SIOCSIFGATEWAY, // 设置默认网关

    SIOCGIFHWADDR, // 获取 mac 地址
    SIOCSIFHWADDR, // 设置 mac 地址

    SIOCSIFDHCPSTART, // 启动 DHCP
    SIOCSIFDHCPSTOP,  // 停止 DHCP
};

// 创建虚拟网卡
netif_t *netif_create();

// 初始化虚拟网卡
netif_t *netif_setup(void *nic, eth_addr_t hwaddr, void *output);

err_t netif_ioctl(netif_t *netif, int cmd, void *args, int flags);

// 获取虚拟网卡
netif_t *netif_found(char *name);
netif_t *netif_get(idx_t index);

// IP 路由选择
netif_t *netif_route(ip_addr_t addr);

// 移除虚拟网卡
void netif_remove(netif_t *netif);

// 网卡接收任务输入
void netif_input(netif_t *netif, pbuf_t *pbuf);

// 网卡发送任务输出
void netif_output(netif_t *netif, pbuf_t *pbuf);

#endif