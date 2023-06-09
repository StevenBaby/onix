#ifndef ONIX_NET_PBUF_H
#define ONIX_NET_PBUF_H

#include <onix/types.h>
#include <onix/list.h>
#include <onix/net/eth.h>

typedef struct pbuf_t
{
    list_node_t node; // 列表节点
    size_t length;    // 载荷长度
    u32 count;        // 引用计数
    union
    {
        u8 payload[0]; // 载荷
        eth_t eth[0];  // 以太网帧
    };
} pbuf_t;

pbuf_t *pbuf_get();
void pbuf_put(pbuf_t *pbuf);

#endif