#ifndef ONIX_NET_TCP_H
#define ONIX_NET_TCP_H

#include <onix/net/types.h>
#include <onix/list.h>

typedef enum tcp_state_t
{
    CLOSED = 0,
    LISTEN = 1,
    SYN_SENT = 2,
    SYN_RCVD = 3,
    ESTABLISHED = 4,
    FIN_WAIT1 = 5,
    FIN_WAIT2 = 6,
    CLOSE_WAIT = 7,
    CLOSING = 8,
    LAST_ACK = 9,
    TIME_WAIT = 10
} tcp_state_t;

typedef enum tcp_flag_t
{
    TCP_FIN = 0x01,
    TCP_SYN = 0x02,
    TCP_RST = 0x04,
    TCP_PSH = 0x08,
    TCP_ACK = 0x10,
    TCP_URG = 0x20,
    TCP_ECE = 0x40,
    TCP_CWR = 0x80,
} tcp_flag_t;

typedef struct tcp_t
{
    u16 sport; // 源端口号
    u16 dport; // 目的端口号
    u32 seqno; // 该数据包发送的第一个字节的序号
    u32 ackno; // 确认序号，期待收到的下一个字节序号，表示之前的字节已收到

    u8 RSV : 4; // 保留
    u8 len : 4; // 首部长度，单位 4 字节

    union
    {
        u8 flags;
        struct
        {
            u8 fin : 1; // 终止 Finish
            u8 syn : 1; // 同步 Synchronize
            u8 rst : 1; // 重新建立连接 Reset
            u8 psh : 1; // 向上传递，不要等待 Push
            u8 ack : 1; // 确认标志 Acknowledgement
            u8 urg : 1; // 紧急标志 Urgent
            u8 ece : 1; // ECN-Echo Explicit Congestion Notification Echo
            u8 cwr : 1; // Congestion Window Reduced
        } _packed;
    };

    u16 window;    // 窗口大小
    u16 chksum;    // 校验和
    u16 urgent;    // 紧急指针
    u8 options[0]; // TCP 选项
} _packed tcp_t;

typedef struct tcp_pcb_t
{
    list_node_t node;  // 链表结点
    ip_addr_t laddr;   // 本地地址
    ip_addr_t raddr;   // 远程地址
    u16 lport;         // 本地端口号
    u16 rport;         // 远程端口号
    tcp_state_t state; // 连接状态

    u32 flags; // 控制块状态

    list_t unsent;  // 未发送的报文段列表
    list_t unacked; // 以发送未应答的报文段列表
    list_t outseq;  // 已收到的无序报文
    list_t recved;  // 已收到的有序报文

    struct task_t *ac_waiter; // 接受等待进程
    struct task_t *tx_waiter; // 写等待进程
    struct task_t *rx_waiter; // 读等待进程
} tcp_pcb_t;

err_t tcp_input(netif_t *netif, pbuf_t *pbuf);

#endif