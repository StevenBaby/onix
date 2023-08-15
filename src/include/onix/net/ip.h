#ifndef ONIX_NET_IP_H
#define ONIX_NET_IP_H

#include <onix/net/types.h>
#include <onix/net/eth.h>
#include <onix/net/icmp.h>
#include <onix/net/udp.h>
#include <onix/net/tcp.h>

#define IP_VERSION_4 4
#define IP_TTL 64

#define IP_ANY "\x00\x00\x00\x00"
#define IP_BROADCAST "\xFF\xFF\xFF\xFF"

enum
{
    IP_PROTOCOL_NONE = 0,
    IP_PROTOCOL_ICMP = 1,
    IP_PROTOCOL_TCP = 6,
    IP_PROTOCOL_UDP = 17,
    IP_PROTOCOL_TEST = 254,
};

#define IP_FLAG_NOFRAG 0b10
#define IP_FLAG_NOLAST 0b100

typedef struct ip_t
{
    u8 header : 4;  // 头长度
    u8 version : 4; // 版本号
    u8 tos;         // type of service 服务类型
    u16 length;     // 数据包长度

    // 以下用于分片
    u16 id;         // 标识，每发送一个分片该值加 1
    u8 offset0 : 5; // 分片偏移量高 5 位，以 8字节 为单位
    u8 flags : 3;   // 标志位，1：保留，2：不分片，4：不是最后一个分片
    u8 offset1;     // 分片偏移量低 8 位，以 8字节 为单位

    u8 ttl;        // 生存时间 Time To Live，每经过一个路由器该值减一，到 0 则丢弃
    u8 proto;      // 上层协议，1：ICMP 6：TCP 17：UDP
    u16 chksum;    // 校验和
    ip_addr_t src; // 源 IP 地址
    ip_addr_t dst; // 目的 IP 地址

    union
    {
        u8 payload[0];       // 载荷
        icmp_t icmp[0];      // ICMP 协议
        icmp_echo_t echo[0]; // ICMP ECHO 协议
        udp_t udp[0];        // UDP 协议
        tcp_t tcp[0];        // TCP 协议
    };

} _packed ip_t;

err_t ip_input(netif_t *netif, pbuf_t *pbuf);
err_t ip_output(netif_t *netif, pbuf_t *pbuf, ip_addr_t dst, u8 proto, u16 len);

#endif
