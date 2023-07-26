#ifndef ONIX_NET_UDP_H
#define ONIX_NET_UDP_H

#include <onix/net/types.h>
#include <onix/list.h>

enum
{
    UDP_FLAG_NOCHKSUM = 0x01,
    UDP_FLAG_BROADCAST = 0x02,
    UDP_FLAG_CONNECTED = 0x04
};

typedef struct udp_pcb_t udp_pcb_t;

typedef struct udp_t
{
    u16 sport;     // 源端口号
    u16 dport;     // 目的端口号
    u16 length;    // 长度
    u16 chksum;    // 校验和
    u8 payload[0]; // 载荷
} _packed udp_t;

typedef struct udp_pcb_t
{
    list_node_t node; // 链表结点

    ip_addr_t laddr; // 本地地址
    ip_addr_t raddr; // 远程地址
    u16 lport;       // 本地端口号
    u16 rport;       // 远程端口号

    u32 flags; // 状态

    list_t rx_pbuf_list;      // 接收缓冲队列
    struct task_t *rx_waiter; // 等待进程
} udp_pcb_t;

int udp_input(netif_t *netif, pbuf_t *pbuf);

#endif