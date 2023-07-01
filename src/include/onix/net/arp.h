#ifndef ONIX_NET_ARP_H
#define ONIX_NET_ARP_H

#include <onix/net/types.h>
#include <onix/mutex.h>

enum
{
    ARP_OP_REQUEST = 1, // ARP 请求
    ARP_OP_REPLY = 2,   // ARP 回复
};

#define ARP_HARDWARE_ETH 1     // ARP 以太网
#define ARP_HARDWARE_ETH_LEN 6 // ARP 以太网地址长度

#define ARP_PROTOCOL_IP 0x0800 // ARP 协议 IP
#define ARP_PROTOCOL_IP_LEN 4  // ARP IP 地址长度 4

#define ARP_ENTRY_TIMEOUT 600  // ARP 缓冲失效时间秒
#define ARP_RETRY 5            // ARP 请求重试次数
#define ARP_DELAY 2            // ARP 请求延迟秒
#define ARP_REFRESH_DELAY 1000 // ARP 刷新间隔毫秒

typedef struct arp_t
{
    u16 hwtype;       // 硬件类型
    u16 proto;        // 协议类型
    u8 hwlen;         // 硬件地址长度
    u8 protolen;      // 协议地址长度
    u16 opcode;       // 操作类型
    eth_addr_t hwsrc; // 源 MAC 地址
    ip_addr_t ipsrc;  // 源 IP 地址
    eth_addr_t hwdst; // 目的 MAC 地址
    ip_addr_t ipdst;  // 目的 IP 地址
} _packed arp_t;

err_t arp_input(netif_t *netif, pbuf_t *pbuf);
err_t arp_eth_output(netif_t *netif, pbuf_t *pbuf, ip_addr_t addr, u16 type, u32 len);

#endif