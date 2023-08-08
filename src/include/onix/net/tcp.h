#ifndef ONIX_NET_TCP_H
#define ONIX_NET_TCP_H

#include <onix/net/types.h>
#include <onix/list.h>

#define TCP_MSS (1500 - 40) // 默认 MSS 大小
#define TCP_WINDOW 8192     // 默认窗口大小
#define TCP_DEFAULT_MSS 536 // 默认发送 MSS 大小

// 序列号比较
#define TCP_SEQ_LT(a, b) ((int)((a) - (b)) < 0)
#define TCP_SEQ_LEQ(a, b) ((int)((a) - (b)) <= 0)
#define TCP_SEQ_GT(a, b) ((int)((a) - (b)) > 0)
#define TCP_SEQ_GEQ(a, b) ((int)((a) - (b)) >= 0)

enum
{
    TF_ACK_DELAY = 0x01, // 可以等等再发 ACK
    TF_ACK_NOW = 0x02,   // 尽快马上发送 ACK
};

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

    u32 snd_una; // 已发送未收到 ack 的最小字节
    u32 snd_nxt; // 需要发送的下一个字节
    u16 snd_wnd; // 发送窗口
    u16 snd_mss; // Maximum segment size
    u32 snd_nbb; // Next Buffer Byte
    u32 snd_max; // 已发送的最大字节序号

    u32 rcv_nxt; // 期望收到的下一个字节
    u16 rcv_wnd; // 接收窗口
    u16 rcv_mss; // Maximum segment size

    list_t unsent;  // 未发送的报文段列表
    list_t unacked; // 以发送未应答的报文段列表
    list_t outseq;  // 已收到的无序报文
    list_t recved;  // 已收到的有序报文

    struct task_t *ac_waiter; // 接受等待进程
    struct task_t *tx_waiter; // 写等待进程
    struct task_t *rx_waiter; // 读等待进程
} tcp_pcb_t;

// 获取初始序列号
u32 tcp_next_isn();

// 获取协议控制块
tcp_pcb_t *tcp_pcb_get();

// 释放协议控制块
void tcp_pcb_put(tcp_pcb_t *pcb);

// 查找协议控制块
tcp_pcb_t *tcp_find_pcb(ip_addr_t laddr, u16 lport, ip_addr_t raddr, u16 rport);

// TCP 输入
err_t tcp_input(netif_t *netif, pbuf_t *pbuf);

// 发送 ACK 数据包
err_t tcp_send_ack(tcp_pcb_t *pcb, u8 flags);

// TCP 连接重置
err_t tcp_reset(u32 seqno, u32 ackno, ip_addr_t laddr, u16 lport, ip_addr_t raddr, u16 rport);

// TCP 输出
err_t tcp_output(tcp_pcb_t *pcb);

// TCP 入队列
err_t tcp_enqueue(tcp_pcb_t *pcb, void *data, size_t size, int flags);

// 解析 TCP 选项
err_t tcp_parse_option(tcp_pcb_t *pcb, tcp_t *tcp);

// 写入 TCP 选项
err_t tcp_write_option(tcp_pcb_t *pcb, tcp_t *tcp);

#endif