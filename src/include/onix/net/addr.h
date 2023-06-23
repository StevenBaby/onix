#ifndef ONIX_NET_ADDR_H
#define ONIX_NET_ADDR_H

#include <onix/net/types.h>

// MAC 地址拷贝
void eth_addr_copy(eth_addr_t dst, eth_addr_t src);

// IP 地址拷贝
void ip_addr_copy(ip_addr_t dst, ip_addr_t src);

// 字符串转换 IP 地址
err_t inet_aton(const char *str, ip_addr_t addr);

#endif
