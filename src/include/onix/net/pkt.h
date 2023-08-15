#ifndef ONIX_NET_PKT_H
#define ONIX_NET_PKT_H

#include <onix/net/types.h>
#include <onix/net/pbuf.h>
#include <onix/list.h>

typedef struct pkt_pcb_t
{
    list_node_t node;
    list_t rx_pbuf_list;
    eth_addr_t laddr;
    eth_addr_t raddr;
    netif_t *netif;

    struct task_t *rx_waiter;
} pkt_pcb_t;

err_t pkt_input(netif_t *netif, pbuf_t *pbuf);

#endif
