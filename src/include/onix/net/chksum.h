#ifndef ONIX_NET_CHKSUM_H
#define ONIX_NET_CHKSUM_H

#include <onix/net/types.h>

u32 eth_fcs(void *data, int len);
u16 ip_chksum(void *data, int len);
u16 inet_chksum(void *data, u16 len, ip_addr_t dst, ip_addr_t src, u16 proto);

#endif