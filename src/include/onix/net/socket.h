#ifndef ONIX_NET_SOCKET_H
#define ONIX_NET_SOCKET_H

#include <onix/stdlib.h>
#include <onix/net/types.h>
#include <onix/net/pkt.h>
#include <onix/net/raw.h>
#include <onix/net/udp.h>
#include <onix/net/tcp.h>

enum
{
    AF_UNSPEC = 0,
    AF_PACKET,
    AF_INET,
};

enum
{
    SOCK_STREAM = 1,
    SOCK_DGRAM = 2,
    SOCK_RAW = 3,
};

enum
{
    PROTO_ALL = -1,
    PROTO_IP = 0,
    PROTO_ICMP = 1,
    PROTO_TCP = 6,
    PROTO_UDP = 17,
};

typedef enum socktype_t
{
    SOCK_TYPE_NONE = 0,
    SOCK_TYPE_PKT,
    SOCK_TYPE_RAW,
    SOCK_TYPE_TCP,
    SOCK_TYPE_UDP,
    SOCK_TYPE_NUM,
} socktype_t;

enum
{
    SOL_SOCKET = 0xffff,

    SO_REUSEADDR = 0x0001,
    SO_KEEPALIVE = 0x0002,
    SO_BROADCAST = 0x0004,
    SO_SNDTIMEO = 0x0010,
    SO_RCVTIMEO = 0x0020,
    SO_LINGER = 0x0040,

    SO_TCP_NODELAY = 0x2008,
    SO_TCP_QUICKACK = 0x2010,

    SO_NETIF = 0x10000,
};

typedef struct sockaddr_t
{
    u16 family;
    char data[14];
} sockaddr_t;

typedef struct sockaddr_in_t
{
    u16 family;
    u16 port;
    ip_addr_t addr;
    u8 zero[8];
} sockaddr_in_t;

typedef struct sockaddr_ll_t
{
    u16 family;
    eth_addr_t addr;
    u8 zero[8];
} sockaddr_ll_t;

typedef struct iovec_t
{
    size_t size;
    void *base;
} iovec_t;

typedef struct msghdr_t
{
    sockaddr_t *name;
    int namelen;
    iovec_t *iov;
    int iovlen;
} msghdr_t;

typedef struct socket_t
{
    socktype_t type; // socket 类型
    int sndtimeo;    // 发送超时
    int rcvtimeo;    // 接收超时
    union
    {
        pkt_pcb_t *pkt;
        raw_pcb_t *raw;
        udp_pcb_t *udp;
        tcp_pcb_t *tcp;
    };
} socket_t;

typedef struct socket_op_t
{
    int (*socket)(socket_t *s, int domain, int type, int protocol);
    int (*close)(socket_t *s);
    int (*listen)(socket_t *s, int backlog);
    int (*accept)(socket_t *s, sockaddr_t *addr, int *addrlen, socket_t **ns);
    int (*bind)(socket_t *s, const sockaddr_t *name, int namelen);
    int (*connect)(socket_t *s, const sockaddr_t *name, int namelen);
    int (*shutdown)(socket_t *s, int how);

    int (*getpeername)(socket_t *s, sockaddr_t *name, int *namelen);
    int (*getsockname)(socket_t *s, sockaddr_t *name, int *namelen);
    int (*getsockopt)(socket_t *s, int level, int optname, void *optval, int *optlen);
    int (*setsockopt)(socket_t *s, int level, int optname, const void *optval, int optlen);

    int (*recvmsg)(socket_t *s, msghdr_t *msg, u32 flags);
    int (*sendmsg)(socket_t *s, msghdr_t *msg, u32 flags);
} socket_op_t;

void socket_register_op(socktype_t type, socket_op_t *op);
socket_t *socket_create();

err_t iovec_check(iovec_t *iov, int iovlen, int write);
size_t iovec_size(iovec_t *iov, int iovlen);
iovec_t *iovec_dup(iovec_t *iov, int iovlen);
int iovec_read(iovec_t *iov, int iovlen, char *buf, size_t count);
int iovec_write(iovec_t *iov, int iovlen, char *buf, size_t count);

#endif
