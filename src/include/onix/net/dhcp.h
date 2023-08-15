#ifndef ONIX_NET_DHCP_H
#define ONIX_NET_DHCP_H

#include <onix/net.h>

void dhcp_start(netif_t *netif);
void dhcp_stop(netif_t *netif);

#endif
