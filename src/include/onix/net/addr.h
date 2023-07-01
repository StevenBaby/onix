#ifndef ONIX_NET_ADDR_H
#define ONIX_NET_ADDR_H

#include <onix/net/types.h>

// MAC 地址拷贝
void eth_addr_copy(eth_addr_t dst, eth_addr_t src);

// IP 地址拷贝
void ip_addr_copy(ip_addr_t dst, ip_addr_t src);

// 字符串转换 IP 地址
err_t inet_aton(const char *str, ip_addr_t addr);

// 比较两 ip 地址是否相等
bool ip_addr_cmp(ip_addr_t addr1, ip_addr_t addr2);

// 比较两地址是否在同一子网
bool ip_addr_maskcmp(ip_addr_t addr1, ip_addr_t addr2, ip_addr_t mask);

#endif
