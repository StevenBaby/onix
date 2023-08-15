#include <onix/net.h>
#include <onix/string.h>
#include <onix/list.h>
#include <onix/arena.h>
#include <onix/task.h>
#include <onix/timer.h>
#include <onix/syscall.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// DHCP OPTION 选项
enum
{
    OPT_PAD = 0,                // 填充
    OPT_SUBNET_MASK = 1,        // 子网掩码
    OPT_TIME_OFFSET = 2,        // 与 UTC 时间的偏移
    OPT_ROUTER = 3,             // 路由器（网关）
    OPT_TIME_SERVER = 4,        // 时间服务器
    OPT_NAME_SERVER = 5,        // 名字服务器 (older IEN 116 Internet Name Servers)
    OPT_DOMAIN_NAME_SERVER = 6, // 域名服务器
    OPT_LOG_SERVER = 7,         // 日志服务器
    OPT_COOKIE_SERVER = 8,      // 缓存服务器
    OPT_LPR_SERVER = 9,
    OPT_IMPRESS_SERVER = 10,
    OPT_RESOURCE_LOCATION_SERVER = 11,
    OPT_HOST_NAME = 12, // 主机名
    OPT_BOOT_SIZE = 13,
    OPT_MERIT_DUMP = 14,
    OPT_DOMAIN_NAME = 15,
    OPT_SWAP_SERVER = 16,
    OPT_ROOT_PATH = 17,
    OPT_EXTENSIONS_PATH = 18,
    OPT_IP_FORWARDING = 19,
    OPT_NON_LOCAL_SOURCE_ROUTING = 20,
    OPT_POLICY_FILTER = 21,
    OPT_MAX_DGRAM_REASSEMBLY = 22,
    OPT_DEFAULT_IP_TTL = 23,
    OPT_PATH_MTU_AGING_TIMEOUT = 24,
    OPT_PATH_MTU_PLATEAU_TABLE = 25,
    OPT_INTERFACE_MTU = 26,
    OPT_ALL_SUBNETS_LOCAL = 27,
    OPT_BROADCAST_ADDRESS = 28, // 广播地址
    OPT_PERFORM_MASK_DISCOVERY = 29,
    OPT_MASK_SUPPLIER = 30,
    OPT_ROUTER_DISCOVERY = 31,
    OPT_ROUTER_SOLICITATION_ADDRESS = 32,
    OPT_STATIC_ROUTES = 33,
    OPT_TRAILER_ENCAPSULATION = 34,
    OPT_ARP_CACHE_TIMEOUT = 35,
    OPT_IEEE802_3_ENCAPSULATION = 36,
    OPT_DEFAULT_TCP_TTL = 37,
    OPT_TCP_KEEPALIVE_INTERVAL = 38,
    OPT_TCP_KEEPALIVE_GARBAGE = 39,
    OPT_NIS_DOMAIN = 40,
    OPT_NIS_SERVER = 41,
    OPT_NTP_SERVER = 42,
    OPT_VENDOR_ENCAPSULATED_OPTIONS = 43,
    OPT_NETBIOS_NAME_SERVER = 44,
    OPT_NETBIOS_DD_SERVER = 45,
    OPT_NETBIOS_NODE_TYPE = 46,
    OPT_NETBIOS_SCOPE = 47,
    OPT_FONT_SERVER = 48,
    OPT_X_DISPLAY_MANAGER = 49,
    OPT_DHCP_REQUESTED_ADDRESS = 50,
    OPT_DHCP_LEASE_TIME = 51, // DHCP 租赁时间
    OPT_OPT_OVERLOAD = 52,
    OPT_DHCP_MESSAGE_TYPE = 53,           // DHCP 消息类型
    OPT_DHCP_SERVER_IDENTIFIER = 54,      // DHCP 服务器
    OPT_DHCP_PARAMETER_REQUEST_LIST = 55, // DHCP 请求参数列表
    OPT_DHCP_MESSAGE = 56,                // DHCP 消息
    OPT_DHCP_MAX_MESSAGE_SIZE = 57,
    OPT_DHCP_RENEWAL_TIME = 58,
    OPT_DHCP_REBINDING_TIME = 59,
    OPT_VENDOR_CLASS_IDENTIFIER = 60,
    OPT_DHCP_CLIENT_IDENTIFIER = 61, // DHCP 客户端
    OPT_NWIP_DOMAIN_NAME = 62,
    OPT_NWIP_SUBOPTIONS = 63,
    OPT_USER_CLASS = 77,
    OPT_FQDN = 81,
    OPT_DHCP_AGENT_OPTIONS = 82,
    OPT_END = 255, // 结束
};

enum
{
    DHCP_BOOTREQUEST = 1, // BOOTP 请求
    DHCP_BOOTREPLY = 2,   // BOOTP 响应
};

// DHCP 消息类型
enum
{
    DHCP_DISCOVER = 1, // 客户端 发现 DHCP 服务器
    DHCP_OFFER = 2,    // 服务器相应 DISCOVER 报文，以及提供一些配置参数
    DHCP_REQUEST = 3,  // 客户端请求配置，或租借续期
    DHCP_DECLINE = 4,  // 客户端向服务器说明网络地址已被使用
    DHCP_ACK = 5,      // 服务器对 REQUEST 报文的响应，包括网络地址；
    DHCP_NAK = 6,      // 服务器对 REQUEST 报文的相应，表示网络地址不正确或者租期超限；
    DHCP_RELEASE = 7,  // 客户端要释放地址来通知服务器
    DHCP_INFORM = 8,   // 客户端向服务器，只要求本地配置参数，客户端已经有外部配置的网络地址；
};

#define DHCP_TIMEOUT 10000
#define DHCP_MAGIC 0x63825363

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

typedef struct dhcp_t
{
    u8 op;            // BOOTP 操作类型
    u8 htype;         // 硬件类型，以太网为 1
    u8 hlen;          // 硬件长度，以太网为 6
    u8 hops;          // 数据包每经过一个路由器加 1
    u32 xid;          // Transaction ID，随机数，用于匹配消息请求和响应对应
    u16 secs;         // seconds 请求开始的时间
    u16 flags;        // 最高位 1 << 15，表示广播
    ip_addr_t ciaddr; // client ip address 客户端 IP 地址，如果客户端知道自己的的 IP 则填入，否则为 0
    ip_addr_t yiaddr; // your ip Address 响应 IP 地址
    ip_addr_t siaddr; // server ip address 服务端地址
    ip_addr_t giaddr; // relay agent ip address
    u8 chaddr[16];    // client hardware address
    u8 sname[64];     // host name 服务器主机名
    u8 file[128];     // boot file name 引导文件名
    u32 magic;        // 魔数
    u8 options[0];    // 选项
} _packed dhcp_t;

typedef struct dhcp_opt_t
{
    u8 type;
    u8 size;
    u8 value[0];
} _packed dhcp_opt_t;

typedef struct dhcp_pcb_t
{
    list_node_t node; // 链表节点
    netif_t *netif;   // 网卡
    ip_addr_t server; // DHCP 服务器
    int expires;      // 超时时间
    u32 lease_time;
    u32 renewal_time;
    u32 rebinding_time;
} dhcp_pcb_t;

static list_t dhcp_list;
static task_t *dhcp_task;

static dhcp_pcb_t *find_pcb(netif_t *netif)
{
    list_t *list = &dhcp_list;
    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next)
    {
        dhcp_pcb_t *pcb = element_entry(dhcp_pcb_t, node, node);
        if (pcb->netif == netif)
            return pcb;
    }
    return NULL;
}

static size_t dhcp_release_option(dhcp_pcb_t *pcb, dhcp_t *dhcp)
{
    char *ptr = dhcp->options;
    dhcp_opt_t *opt;
    opt = (dhcp_opt_t *)ptr;

    opt->type = OPT_DHCP_MESSAGE_TYPE;
    opt->size = 1;
    *opt->value = DHCP_RELEASE;

    ptr += opt->size + 2;
    opt = (dhcp_opt_t *)ptr;

    opt->type = OPT_DHCP_SERVER_IDENTIFIER;
    opt->size = 4;
    ip_addr_copy(opt->value, pcb->server);

    ptr += opt->size + 2;
    opt = (dhcp_opt_t *)ptr;

    opt->type = OPT_END;
    ptr += 1;

    return (u32)ptr - (u32)dhcp->options;
}

void dhcp_release(dhcp_pcb_t *pcb)
{
    if (ip_addr_isany(pcb->server))
        return;
    if (ip_addr_isany(pcb->netif->ipaddr))
        return;

    fd_t fd = socket(AF_INET, SOCK_DGRAM, PROTO_UDP);
    if (fd < EOK)
    {
        LOGK("Create packet socket failure...\n");
        return;
    }

    pbuf_t *pbuf = NULL;
    int opt = 5000;
    err_t ret;
    ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &opt, 4);
    if (ret < EOK)
        goto rollback;

    sockaddr_in_t addr;
    ip_addr_copy(addr.addr, pcb->netif->ipaddr);
    addr.family = AF_INET;
    addr.port = htons(DHCP_CLIENT_PORT);
    ret = bind(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    if (ret < EOK)
        goto rollback;

    ip_addr_copy(addr.addr, pcb->server);
    addr.family = AF_INET;
    addr.port = htons(DHCP_SERVER_PORT);
    ret = connect(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    if (ret < EOK)
        goto rollback;

    pbuf = pbuf_get();

    dhcp_t *dhcp = (dhcp_t *)pbuf->payload;

    memset(dhcp, 0, sizeof(dhcp_t));
    u32 xid = time();

    dhcp->op = DHCP_BOOTREQUEST;
    dhcp->hlen = ARP_HARDWARE_ETH_LEN;
    dhcp->htype = ARP_HARDWARE_ETH;
    dhcp->xid = xid;

    ip_addr_copy(dhcp->ciaddr, pcb->netif->ipaddr);
    eth_addr_copy(dhcp->chaddr, pcb->netif->hwaddr);

    dhcp->magic = htonl(DHCP_MAGIC);
    size_t size = dhcp_release_option(pcb, dhcp);

    u16 len = sizeof(dhcp_t) + size;
    ret = send(fd, dhcp, len, 0);
    if (ret < EOK)
        goto rollback;
    LOGK("socket send %d\n", ret);

rollback:
    pbuf_put(pbuf);
    close(fd);
}

void dhcp_start(netif_t *netif)
{
    dhcp_pcb_t *pcb = find_pcb(netif);
    if (pcb)
        return;
    pcb = kmalloc(sizeof(dhcp_pcb_t));
    memset(pcb, 0, sizeof(dhcp_pcb_t));

    pcb->netif = netif;
    pcb->expires = 0;
    list_push(&dhcp_list, &pcb->node);
    task_unblock(dhcp_task, EOK);
}

void dhcp_stop(netif_t *netif)
{
    dhcp_pcb_t *pcb = find_pcb(netif);
    if (!pcb)
        return;

    dhcp_release(pcb);

    ip_addr_copy(netif->ipaddr, "\x00\x00\x00\x00");
    ip_addr_copy(netif->netmask, "\x00\x00\x00\x00");
    ip_addr_copy(netif->gateway, "\x00\x00\x00\x00");
    ip_addr_copy(netif->broadcast, "\x00\x00\x00\x00");

    list_remove(&pcb->node);
    kfree(pcb);
}

static void dhcp_parse_option(dhcp_pcb_t *pcb, dhcp_t *dhcp)
{
    char *ptr = dhcp->options;

    while (true)
    {
        dhcp_opt_t *opt = (dhcp_opt_t *)ptr;
        switch (opt->type)
        {
        case OPT_PAD:
            break;
        case OPT_END:
            goto success;
            break;
        case OPT_SUBNET_MASK:
            if (opt->size != 4)
                goto rollback;
            ip_addr_copy(pcb->netif->netmask, opt->value);
            LOGK("DHCP get netmask %r\n", opt->value);
            break;
        case OPT_TIME_OFFSET:
            break;
        case OPT_ROUTER:
            if (opt->size != 4)
                goto rollback;
            ip_addr_copy(pcb->netif->gateway, opt->value);
            LOGK("DHCP get gateway %r\n", opt->value);
            break;
        case OPT_TIME_SERVER:
            LOGK("DHCP time server...\n");
            break;
        case OPT_NAME_SERVER:
            LOGK("DHCP name server...\n");
            break;
        case OPT_DOMAIN_NAME_SERVER:
            LOGK("DHCP domain server %r...\n", opt->value);
            break;
        case OPT_LOG_SERVER:
            break;
        case OPT_DOMAIN_NAME:
            u8 val = opt->value[opt->size];
            opt->value[opt->size] = 0;
            LOGK("DHCP domain name %s...\n", opt->value);
            opt->value[opt->size] = val;
            break;
        case OPT_BROADCAST_ADDRESS:
            ip_addr_copy(pcb->netif->broadcast, opt->value);
            LOGK("DHCP boardcast address %r...\n", opt->value);
            break;
        case OPT_NETBIOS_NAME_SERVER:
            LOGK("DHCP netbios name server %r...\n", opt->value);
            break;
        case OPT_DHCP_LEASE_TIME:
            if (opt->size != 4)
                goto rollback;
            // value 的单位是秒，时间过半就续租
            pcb->lease_time = ntohl(*(u32 *)opt->value);
            pcb->expires = timer_expire_jiffies(pcb->lease_time * 500);
            LOGK("DHCP get lease time %d\n", pcb->lease_time);
            break;
        case OPT_DHCP_MESSAGE_TYPE:
            if (opt->size != 1)
                goto rollback;
            if (*opt->value != DHCP_OFFER && *opt->value != DHCP_ACK)
                goto rollback;
            break;
        case OPT_DHCP_SERVER_IDENTIFIER:
            if (opt->size != 4)
                goto rollback;
            ip_addr_copy(pcb->server, opt->value);
            LOGK("DHCP get dhcp server %r\n", opt->value);
            break;
        case OPT_DHCP_RENEWAL_TIME:
            pcb->renewal_time = ntohl(*(u32 *)opt->value);
            LOGK("DHCP get renewal time %d\n", pcb->renewal_time);
            break;
        case OPT_DHCP_REBINDING_TIME:
            pcb->rebinding_time = ntohl(*(u32 *)opt->value);
            LOGK("DHCP get rebinding time %d\n", pcb->rebinding_time);
            break;
        default:
            LOGK("UNKNOWN dhcp option %d\n", opt->type);
            break;
        }
        ptr += opt->size + 2;
    }
success:
    if (!ip_addr_isany(dhcp->yiaddr))
    {
        ip_addr_copy(pcb->netif->ipaddr, dhcp->yiaddr);
        dhcp->yiaddr[3] = 255;
        ip_addr_copy(pcb->netif->broadcast, dhcp->yiaddr);
        LOGK("DHCP get ip address %r\n", dhcp->yiaddr);
    }
rollback:
    return;
}

static size_t dhcp_discover_option(dhcp_t *dhcp)
{
    char *ptr = dhcp->options;
    dhcp_opt_t *opt;
    opt = (dhcp_opt_t *)ptr;

    opt->type = OPT_DHCP_MESSAGE_TYPE;
    opt->size = 1;
    *opt->value = DHCP_DISCOVER;

    ptr += opt->size + 2;
    opt = (dhcp_opt_t *)ptr;

    opt->type = OPT_DHCP_PARAMETER_REQUEST_LIST;
    opt->size = 3;
    opt->value[0] = OPT_ROUTER;
    opt->value[1] = OPT_SUBNET_MASK;
    opt->value[2] = OPT_DOMAIN_NAME_SERVER;

    ptr += opt->size + 2;
    opt = (dhcp_opt_t *)ptr;

    opt->type = OPT_END;
    ptr += 1;

    return (u32)ptr - (u32)dhcp->options;
}

static void dhcp_discover(dhcp_pcb_t *pcb)
{
    LOGK("dhcp request for netif %s\n", pcb->netif->name);
    fd_t fd = socket(AF_PACKET, SOCK_RAW, PROTO_UDP);
    if (fd < EOK)
    {
        LOGK("Create packet socket failure...\n");
        return;
    }

    pbuf_t *pbuf = NULL;
    int opt = 5000;
    err_t ret;
    ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &opt, 4);
    if (ret < EOK)
        goto rollback;

    ifreq_t req;
    strcpy(req.name, "eth1");
    ret = ioctl(fd, SIOCGIFINDEX, (int)&req);
    if (ret < 0)
    {
        LOGK("get netif error.\n");
        goto rollback;
    }

    opt = req.index;
    ret = setsockopt(fd, SOL_SOCKET, SO_NETIF, &opt, 4);
    if (ret < 0)
    {
        LOGK("set netif error.\n");
        goto rollback;
    }

    pbuf = pbuf_get();

    eth_t *eth = pbuf->eth;
    ip_t *ip = eth->ip;
    udp_t *udp = ip->udp;
    dhcp_t *dhcp = (dhcp_t *)udp->payload;

    memset(dhcp, 0, sizeof(dhcp_t));
    dhcp->op = DHCP_BOOTREQUEST;

    u32 xid = time();

    dhcp->xid = xid;
    dhcp->hlen = ARP_HARDWARE_ETH_LEN;
    dhcp->htype = ARP_HARDWARE_ETH;
    dhcp->magic = htonl(DHCP_MAGIC);

    eth_addr_copy(dhcp->chaddr, pcb->netif->hwaddr);
    size_t size = dhcp_discover_option(dhcp);
    // memcpy(dhcp->options, "\x35\x01\x01\x37\x02\x03\x06\xff", 8);

    u16 len = sizeof(dhcp_t) + size;

    udp->sport = htons(DHCP_CLIENT_PORT);
    udp->dport = htons(DHCP_SERVER_PORT);

    len += sizeof(udp_t);
    udp->length = htons(len);
    udp->chksum = 0;
    udp->chksum = inet_chksum(udp, len, IP_ANY, IP_BROADCAST, IP_PROTOCOL_UDP);

    ip->version = 4;
    ip->header = sizeof(ip_t) / 4;
    ip->tos = 0;

    len += sizeof(ip_t);
    ip->length = htons(len);

    ip->id = 0;
    ip->flags = 0;
    ip->offset0 = 0;
    ip->offset1 = 0;

    ip->ttl = IP_TTL;
    ip->proto = IP_PROTOCOL_UDP;

    ip_addr_copy(ip->dst, IP_BROADCAST);
    ip_addr_copy(ip->src, IP_ANY);

    ip->chksum = 0;
    ip->chksum = ip_chksum(ip, sizeof(ip_t));

    eth_addr_copy(eth->src, pcb->netif->hwaddr);
    eth_addr_copy(eth->dst, ETH_BROADCAST);
    eth->type = htons(ETH_TYPE_IP);

    len += sizeof(eth_t);
    ret = send(fd, eth, len, 0);
    if (ret < 0)
    {
        LOGK("send dhcp packet failure...\n");
        goto rollback;
    }

    while (true)
    {
        ret = recv(fd, eth, ETH_MTU, 0);
        if (ret < 0)
        {
            LOGK("recv dhcp packet failure...\n");
            goto rollback;
        }
        if (dhcp->xid == xid)
            break;
    }

    if (dhcp->magic != ntohl(DHCP_MAGIC))
        goto rollback;

    if (dhcp->op != DHCP_BOOTREPLY)
        goto rollback;

    dhcp_parse_option(pcb, dhcp);

rollback:
    pbuf_put(pbuf);
    close(fd);
}

static size_t dhcp_request_option(dhcp_pcb_t *pcb, dhcp_t *dhcp)
{
    char *ptr = dhcp->options;
    dhcp_opt_t *opt;
    opt = (dhcp_opt_t *)ptr;

    opt->type = OPT_DHCP_MESSAGE_TYPE;
    opt->size = 1;
    *opt->value = DHCP_REQUEST;

    ptr += opt->size + 2;
    opt = (dhcp_opt_t *)ptr;

    opt->type = OPT_DHCP_REQUESTED_ADDRESS;
    opt->size = 4;
    ip_addr_copy(opt->value, pcb->netif->ipaddr);

    ptr += opt->size + 2;
    opt = (dhcp_opt_t *)ptr;

    opt->type = OPT_DHCP_SERVER_IDENTIFIER;
    opt->size = 4;
    ip_addr_copy(opt->value, pcb->server);

    ptr += opt->size + 2;
    opt = (dhcp_opt_t *)ptr;

    opt->type = OPT_END;
    ptr += 1;

    return (u32)ptr - (u32)dhcp->options;
}

static void dhcp_request(dhcp_pcb_t *pcb)
{
    fd_t fd = socket(AF_INET, SOCK_DGRAM, PROTO_UDP);
    if (fd < EOK)
    {
        LOGK("Create packet socket failure...\n");
        return;
    }

    pbuf_t *pbuf = NULL;
    int opt = 5000;
    err_t ret;
    ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &opt, 4);
    if (ret < EOK)
        goto rollback;

    sockaddr_in_t addr;
    ip_addr_copy(addr.addr, pcb->netif->ipaddr);
    addr.family = AF_INET;
    addr.port = htons(DHCP_CLIENT_PORT);
    ret = bind(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    if (ret < EOK)
        goto rollback;

    ip_addr_copy(addr.addr, pcb->server);
    addr.family = AF_INET;
    addr.port = htons(DHCP_SERVER_PORT);
    ret = connect(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    if (ret < EOK)
        goto rollback;

    pbuf = pbuf_get();

    dhcp_t *dhcp = (dhcp_t *)pbuf->payload;

    memset(dhcp, 0, sizeof(dhcp_t));
    u32 xid = time();

    dhcp->op = DHCP_BOOTREQUEST;
    dhcp->hlen = ARP_HARDWARE_ETH_LEN;
    dhcp->htype = ARP_HARDWARE_ETH;
    dhcp->xid = xid;

    ip_addr_copy(dhcp->ciaddr, pcb->netif->ipaddr);
    eth_addr_copy(dhcp->chaddr, pcb->netif->hwaddr);

    dhcp->magic = htonl(DHCP_MAGIC);
    size_t size = dhcp_request_option(pcb, dhcp);

    u16 len = sizeof(dhcp_t) + size;
    ret = send(fd, dhcp, len, 0);
    if (ret < EOK)
        goto rollback;
    LOGK("socket send %d\n", ret);

    while (true)
    {
        LOGK("receiving from dhcp server\n");
        ret = recv(fd, dhcp, 1000, 0);
        if (ret < 0)
            goto rollback;

        if (dhcp->xid == xid)
            break;
    }

    if (dhcp->magic != ntohl(DHCP_MAGIC))
        goto rollback;

    if (dhcp->op != DHCP_BOOTREPLY)
        goto rollback;

    dhcp_parse_option(pcb, dhcp);

rollback:
    pbuf_put(pbuf);
    close(fd);
}

static void dhcp_query(dhcp_pcb_t *pcb)
{
    if (ip_addr_isany(pcb->netif->ipaddr))
        return dhcp_discover(pcb);
    dhcp_request(pcb);
}

void dhcp_timer()
{
    while (true)
    {
        assert(dhcp_task == running_task());
        list_t *list = &dhcp_list;
        for (list_node_t *node = list->head.next; node != &list->tail; node = node->next)
        {
            dhcp_pcb_t *pcb = element_entry(dhcp_pcb_t, node, node);
            if (!timer_is_expires(pcb->expires))
                continue;
            dhcp_query(pcb);
        }
        task_block(dhcp_task, NULL, TASK_BLOCKED, DHCP_TIMEOUT);
    }
}

void dhcp_init()
{
    LOGK("DHCP init...\n");
    list_init(&dhcp_list);
    dhcp_task = task_create(dhcp_timer, "dhcp", 5, KERNEL_USER);
}
