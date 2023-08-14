#ifndef ONIX_NET_TCP_H
#define ONIX_NET_TCP_H

#include <onix/net/types.h>
#include <onix/list.h>

#define TCP_MSS (1500 - 40)  // 默认 MSS 大小
#define TCP_WINDOW 8192      // 默认窗口大小
#define TCP_MAX_WINDOW 65535 // 最大窗口大小
#define TCP_DEFAULT_MSS 536  // 默认发送 MSS 大小

// 序列号比较
#define TCP_SEQ_LT(a, b) ((int)((a) - (b)) < 0)
#define TCP_SEQ_LEQ(a, b) ((int)((a) - (b)) <= 0)
#define TCP_SEQ_GT(a, b) ((int)((a) - (b)) > 0)
#define TCP_SEQ_GEQ(a, b) ((int)((a) - (b)) >= 0)

enum
{
    TF_ACK_DELAY = 0x01, // 可以等等再发 ACK
    TF_ACK_NOW = 0x02,   // 尽快马上发送 ACK
    TF_KEEPALIVE = 0x04, // 启用保活机制
    TF_NODELAY = 0x08,   // 禁用 NAGLE 算法
    TF_QUICKACK = 0x10   // 快速 ACK
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

enum
{
    TCP_TIMER_SYN,       // 建立连接定时器
    TCP_TIMER_REXMIT,    // 重传定时器
    TCP_TIMER_PERSIST,   // 持续定时器
    TCP_TIMER_KEEPALIVE, // 保活定时器
    TCP_TIMER_FIN_WAIT2, // FIN_WAIT2 定时器
    TCP_TIMER_TIMEWAIT,  // TIMEWAIT 定时器
    TCP_TIMER_NUM,
};

enum
{
    TCP_FAST_INTERVAL = 200, // 单位毫秒
    TCP_SLOW_INTERVAL = 500, // 单位毫秒
    TCP_FASTHZ = 5,          // 每秒次数
    TCP_SLOWHZ = 2,          // 每秒次数
};

enum
{
    TCP_TO_SYN = 75 * TCP_SLOWHZ,             // 建立连接超时
    TCP_TO_FIN_WAIT2 = 10 * 60 * TCP_SLOWHZ,  // FIN_WAIT2 超时
    TCP_TO_TIMEWAIT = 4 * 60 * TCP_SLOWHZ,    // TIMEWAIT 超时
    TCP_TO_PERSMIN = 5 * TCP_SLOWHZ,          // 最小持续超时
    TCP_TO_PERMAX = 60 * TCP_SLOWHZ,          // 最大持续超时
    TCP_TO_KEEP_IDLE = 120 * 60 * TCP_SLOWHZ, // 保活启动
    TCP_TO_KEEP_INTERVAL = 75 * TCP_SLOWHZ,   // 保活测试间隔
    TCP_TO_KEEPCNT = 8,                       // 保活测试次数
    TCP_TO_RTT_DEFAULT = 1 * TCP_SLOWHZ,      // 默认重传时间
    TCP_TO_MIN = 1 * TCP_SLOWHZ,              // 最小重传时间
    TCP_TO_REXMIT_MAX = 64 * TCP_SLOWHZ,      // 最大重传时间
    TCP_MAXRXTCNT = 12,                       // 最大重传次数
};

#define TCP_RTT_SHIFT 3
#define TCP_RTT_SCALE (1 << TCP_RTT_SHIFT)
#define TCP_RTTVAR_SHIFT 2
#define TCP_RTTVAR_SACLE (1 << TCP_RTTVAR_SHIFT)
#define TCP_REXMIT_THREASH 3

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

    u32 snd_una;     // 已发送未收到 ack 的最小字节
    u32 snd_nxt;     // 需要发送的下一个字节
    u16 snd_wnd;     // 发送窗口
    u16 snd_mss;     // Maximum segment size
    u32 snd_nbb;     // Next Buffer Byte
    u32 snd_max;     // 已发送的最大字节序号
    u32 snd_wl1;     // 上次更新窗口的序列号
    u32 snd_wl2;     // 上次更新窗口的确认号
    u32 snd_mwnd;    // 最大发送窗口
    pbuf_t *snd_buf; // nagle 发送缓存

    u32 rcv_nxt; // 期望收到的下一个字节
    u16 rcv_wnd; // 接收窗口
    u16 rcv_mss; // Maximum segment size

    u32 timers[TCP_TIMER_NUM]; // 计时器

    u32 idle;    // 空闲时间
    u32 rto;     // 当前重传时限 RTO(Retransmission TimeOut)
    u32 rtx_cnt; // 重传次数

    u32 rtt;     // round trip time 往返时间
    u32 rtt_seq; // 用于计时的序列号
    int srtt;    // 已平滑的 RTT 估计器
    int rttvar;  // 已平滑的 RTT 平均偏差估计器
    u32 rttmin;  // 最小往返时间

    u32 dupacks;      // 重复 ACK
    u32 snd_cwnd;     // 拥塞窗口
    u32 snd_ssthresh; // 线性增加阈值

    list_t acclist; // 客户端 PCB 链表
    int backlog;    // 客户端数量
    int backcnt;    // 当前数量

    list_node_t accnode;      // 客户端节点
    struct tcp_pcb_t *listen; // 监听 PCB

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

// 清空协议控制块
void tcp_pcb_purge(tcp_pcb_t *pcb, err_t reason);

// 进入 TIMEWAIT 状态
void tcp_pcb_timewait(tcp_pcb_t *pcb);

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

// TCP 响应
err_t tcp_response(tcp_pcb_t *pcb, u32 seqno, u32 ackno, u8 flags);

// TCP 输出
err_t tcp_output(tcp_pcb_t *pcb);

// TCP 入队列
err_t tcp_enqueue(tcp_pcb_t *pcb, void *data, size_t size, int flags);

// TCP 重传
err_t tcp_rexmit(tcp_pcb_t *pcb);

// 解析 TCP 选项
err_t tcp_parse_option(tcp_pcb_t *pcb, tcp_t *tcp);

// 写入 TCP 选项
err_t tcp_write_option(tcp_pcb_t *pcb, tcp_t *tcp);

// 跟新 TCP RTO
u32 tcp_update_rto(tcp_pcb_t *pcb, int backoff);

// 重新计算往返时间
void tcp_xmit_timer(tcp_pcb_t *pcb, u32 rtt);

#endif