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

    list_node_t tcpnode; // TCP 缓冲节点
    u8 *data;            // TCP 数据指针
    size_t total;        // TCP 总长度 头 + 数据长度
    size_t size;         // TCP 数据长度
    u32 seqno;           // TCP 序列号

    union
    {
        u8 payload[0]; // 载荷
        eth_t eth[0];  // 以太网帧
    };
} pbuf_t;

pbuf_t *pbuf_get();
void pbuf_put(pbuf_t *pbuf);

#endif