
#include <onix/net.h>
#include <onix/assert.h>
#include <onix/device.h>
#include <onix/string.h>

static netif_t *loopif = NULL;

static void send_packet(netif_t *netif, pbuf_t *pbuf)
{
    ip_addr_t addr;
    ip_addr_copy(addr, pbuf->eth->ip->dst);
    ip_addr_copy(pbuf->eth->ip->dst, pbuf->eth->ip->src);
    ip_addr_copy(pbuf->eth->ip->src, addr);
    netif_input(netif, pbuf);
}

void loopif_init()
{
    loopif = netif_create();
    loopif->nic_output = send_packet;

    strcpy(loopif->name, "loopback");

    assert(inet_aton("127.0.0.1", loopif->ipaddr) == EOK);
    assert(inet_aton("255.0.0.0", loopif->netmask) == EOK);

    assert(eth_addr_isany(loopif->hwaddr));
    assert(ip_addr_isany(loopif->gateway));

    loopif->flags = (NETIF_LOOPBACK |
                     NETIF_IP_RX_CHECKSUM_OFFLOAD |
                     NETIF_IP_TX_CHECKSUM_OFFLOAD |
                     NETIF_UDP_RX_CHECKSUM_OFFLOAD |
                     NETIF_UDP_TX_CHECKSUM_OFFLOAD |
                     NETIF_TCP_RX_CHECKSUM_OFFLOAD |
                     NETIF_TCP_TX_CHECKSUM_OFFLOAD);

    device_install(DEV_NET, DEV_NETIF, loopif, loopif->name, 0, netif_ioctl, NULL, NULL);
}
