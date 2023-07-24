

#ifndef ONIX_NET_RAW_H
#define ONIX_NET_RAW_H

#include <onix/net/types.h>
#include <onix/net/pbuf.h>
#include <onix/list.h>

#define RAW_TTL 255

typedef struct raw_pcb_t
{
    list_node_t node;
    ip_addr_t laddr;
    ip_addr_t raddr;
    u16 protocol;

    list_t rx_pbuf_list;
    struct task_t *rx_waiter;
} raw_pcb_t;

err_t raw_input(netif_t *netif, pbuf_t *pbuf);

#endif
