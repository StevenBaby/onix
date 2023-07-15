#ifndef ONIX_NET_ICMP_H
#define ONIX_NET_ICMP_H

#include <onix/net/types.h>

enum
{
    ICMP_ER = 0,   // Echo reply
    ICMP_DUR = 3,  // Destination unreachable
    ICMP_SQ = 4,   // Source quench
    ICMP_RD = 5,   // Redirect
    ICMP_ECHO = 8, // Echo
    ICMP_TE = 11,  // Time exceeded
    ICMP_PP = 12,  // Parameter problem
    ICMP_TS = 13,  // Timestamp
    ICMP_TSR = 14, // Timestamp reply
    ICMP_IRQ = 15, // Information request
    ICMP_IR = 16,  // Information reply
};

typedef struct icmp_echo_t
{
    u8 type;       // 类型
    u8 code;       // 状态码
    u16 chksum;    // 校验和
    u16 id;        // 标识
    u16 seq;       // 序号
    u8 payload[0]; // 可能是特殊的字符串
} _packed icmp_echo_t;

typedef struct icmp_t
{
    u8 type;       // 类型
    u8 code;       // 状态码
    u16 chksum;    // 校验和
    u32 RESERVED;  // 保留
    u8 payload[0]; // 载荷
} _packed icmp_t;

err_t icmp_input(netif_t *netif, pbuf_t *pbuf);
err_t icmp_echo(netif_t *netif, pbuf_t *pbuf, ip_addr_t dst);

#endif