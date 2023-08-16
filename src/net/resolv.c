#include <onix/net/types.h>
#include <onix/net.h>
#include <onix/memory.h>
#include <onix/syscall.h>
#include <onix/mutex.h>
#include <onix/fs.h>
#include <onix/string.h>
#include <onix/debug.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// references:
// <https://datatracker.ietf.org/doc/html/rfc1035>

#define PACKET_SIZE 512
static void *buf = NULL;
static ip_addr_t nameserver;
static lock_t resolv_lock;

enum
{
    DNS_OP_QUERY = 0,  // Standard query
    DNS_OP_IQUERY = 1, // Inverse query (deprecated/unsupported)
    DNS_OP_STATUS = 2, // Name server status query (unsupported)
    DNS_OP_NOTIFY = 4, // Zone change notification
    DNS_OP_UPDATE = 5, // Zone update message
};

enum
{
    DNS_ERR_NOERROR = 0,  // No error occurred
    DNS_ERR_FORMERR = 1,  // Format error
    DNS_ERR_SERVFAIL = 2, // Server failure
    DNS_ERR_NXDOMAIN = 3, // Name error
    DNS_ERR_NOTIMPL = 4,  // Unimplemented
    DNS_ERR_REFUSED = 5,  // Operation refused
    DNS_ERR_YXDOMAIN = 6, // Name exists
    DNS_ERR_YXRRSET = 7,  // RRset exists
    DNS_ERR_NXRRSET = 8,  // RRset does not exist
    DNS_ERR_NOTAUTH = 9,  // Not authoritative for zone
    DNS_ERR_NOTZONE = 10, // Zone of record different from zone section
};

enum
{
    DNS_TYPE_INVALID = 0,   //  Cookie
    DNS_TYPE_A = 1,         //  Host address
    DNS_TYPE_NS = 2,        //  Authoritative server
    DNS_TYPE_MD = 3,        //  Mail destination
    DNS_TYPE_MF = 4,        //  Mail forwarder
    DNS_TYPE_CNAME = 5,     //  Canonical name
    DNS_TYPE_SOA = 6,       //  Start of authority zone
    DNS_TYPE_MB = 7,        //  Mailbox domain name
    DNS_TYPE_MG = 8,        //  Mail group member
    DNS_TYPE_MR = 9,        //  Mail rename name
    DNS_TYPE_NULL = 10,     //  Null resource record
    DNS_TYPE_WKS = 11,      //  Well known service
    DNS_TYPE_PTR = 12,      //  Domain name pointer
    DNS_TYPE_HINFO = 13,    //  Host information
    DNS_TYPE_MINFO = 14,    //  Mailbox information
    DNS_TYPE_MX = 15,       //  Mail routing information
    DNS_TYPE_TXT = 16,      //  Text strings
    DNS_TYPE_RP = 17,       //  Responsible person
    DNS_TYPE_AFSDB = 18,    //  AFS cell database
    DNS_TYPE_X25 = 19,      //  X_25 calling address
    DNS_TYPE_ISDN = 20,     //  ISDN calling address
    DNS_TYPE_RT = 21,       //  Router
    DNS_TYPE_NSAP = 22,     //  NSAP address
    DNS_TYPE_NSAP_PTR = 23, //  Reverse NSAP lookup (deprecated)
    DNS_TYPE_SIG = 24,      //  Security signature
    DNS_TYPE_KEY = 25,      //  Security key
    DNS_TYPE_PX = 26,       //  X.400 mail mapping
    DNS_TYPE_GPOS = 27,     //  Geographical position (withdrawn)
    DNS_TYPE_AAAA = 28,     //  Ip6 Address
    DNS_TYPE_LOC = 29,      //  Location Information
    DNS_TYPE_NXT = 30,      //  Next domain (security)
    DNS_TYPE_EID = 31,      //  Endpoint identifier
    DNS_TYPE_NIMLOC = 32,   //  Nimrod Locator
    DNS_TYPE_SRV = 33,      //  Server Selection
    DNS_TYPE_ATMA = 34,     //  ATM Address
    DNS_TYPE_NAPTR = 35,    //  Naming Authority PoinTeR
    DNS_TYPE_KX = 36,       //  Key Exchange
    DNS_TYPE_CERT = 37,     //  Certification record
    DNS_TYPE_A6 = 38,       //  IPv6 address (deprecates AAAA)
    DNS_TYPE_DNAME = 39,    //  Non-terminal DNAME (for IPv6)
    DNS_TYPE_SINK = 40,     //  Kitchen sink (experimentatl)
    DNS_TYPE_OPT = 41,      //  EDNS0 option (meta-RR)

    DNS_TYPE_TSIG = 250,  //  Transaction signature
    DNS_TYPE_IXFR = 251,  //  Incremental zone transfer
    DNS_TYPE_AXFR = 252,  //  Transfer zone of authority
    DNS_TYPE_MAILB = 253, //  Transfer mailbox records
    DNS_TYPE_MAILA = 254, //  Transfer mail agent records
    DNS_TYPE_ANY = 255,   //  Wildcard match
};

enum
{
    DNS_CLASS_INVALID = 0, //  Cookie
    DNS_CLASS_IN = 1,      //  Internet
    DNS_CLASS_CHAOS = 3,   //  MIT Chaos-net
    DNS_CLASS_HS = 4,      //  MIT Hesiod

    DNS_CLASS_NONE = 254, //  For prereq. sections in update request
    DNS_CLASS_ANY = 255,  //  Wildcard match
};

typedef struct dns_t
{
    u16 id; // Query identification number 事务ID

    u8 rd : 1;     // Recursion desired 期望递归
    u8 tc : 1;     // Truncated message 消息可截断
    u8 aa : 1;     // Authoritive answer 授权回答（与缓存回答相对）
    u8 opcode : 4; // Purpose of message 查询类型
    u8 qr : 1;     // Response flag 请求或响应

    u8 rcode : 4;  // Response code
    u8 cd : 1;     // Checking disabled by resolver 禁用安全检查
    u8 ad : 1;     // Authentic data from named 信息已授权
    u8 unused : 1; // Unused bits (MBZ as of 4.9.3a3)
    u8 ra : 1;     // Recursion available

    u16 qdcount; // Number of question entries 问题数量
    u16 ancount; // Number of answer entries 回答数量
    u16 nscount; // Number of authority entries 授权记录数量
    u16 arcount; // Number of resource entries 资源数量

    u8 payload[0];
} dns_t;

static int fill_payload(char *data, const char *name, u16 type, u16 cls)
{
    char *ptr0 = (char *)name;
    char *ptr = (char *)name;
    char *buf = data;

    while (true)
    {
        while (*ptr != 0 && *ptr != '.')
            ptr++;

        u8 size = (u8)(ptr - ptr0);
        *buf = size;
        buf++;

        memcpy(buf, ptr0, size);
        buf += size;

        if (*ptr == 0)
        {
            *buf = 0;
            buf++;
            break;
        }

        ptr++;
        ptr0 = ptr;
    }

    u16 *attr = (u16 *)buf;
    *attr = htons(type);
    attr++;

    *attr = htons(cls);
    attr++;

    return (u32)attr - (u32)data;
}

static void *skip_name(void *data)
{
    char *ptr = data;
    while (*ptr != 0)
    {
        u8 size = *ptr;
        if ((size & 0xC0) == 0xC0)
        {
            u16 offset = ntohs(*(u16 *)ptr) & 0x3FFF;
            ptr += 2;
            return ptr;
        }
        ptr += size + 1;
    }
    ptr++;
    return ptr;
}

static err_t resolve(dns_t *dns, u32 len, ip_addr_t addr)
{
    char *data = dns->payload;
    len -= sizeof(dns_t);

    char *ptr = data;
    char *end = data + len;
    int count = dns->qdcount;

    while (count-- && ptr < end)
    {
        ptr = skip_name(ptr);

        u16 type = ntohs(*(u16 *)ptr);
        ptr += 2;

        u16 cls = ntohs(*(u16 *)ptr);
        ptr += 2;
    }

    count = dns->ancount;
    int ret = -EINVAL;
    while (count-- && ptr < end)
    {
        ptr = skip_name(ptr);
        u16 type = ntohs(*(u16 *)ptr);
        ptr += 2;

        u16 cls = ntohs(*(u16 *)ptr);
        ptr += 2;

        u32 ttl = ntohl(*(u32 *)ptr);
        ptr += 4;

        u16 rdlength = ntohs(*(u16 *)ptr);
        ptr += 2;

        if (type == DNS_TYPE_A && cls == DNS_CLASS_IN && rdlength == 4)
        {
            LOGK("resolved addr %r\n", ptr);
            ip_addr_copy(addr, ptr);
            return EOK;
        }
        if (type == DNS_TYPE_CNAME && cls == DNS_CLASS_IN)
        {
            LOGK("resolved cname %s\n", ptr);
        }
        ptr += rdlength;
    }
    return ret;
}

int sys_resolv(const char *name, ip_addr_t result)
{
    lock_acquire(&resolv_lock);
    err_t ret = -ERROR;
    fd_t fd = socket(AF_INET, SOCK_DGRAM, PROTO_UDP);
    if (fd < 0)
    {
        LOGK("open socket failure...\n");
        ret = fd;
        goto rollback;
    }

    sockaddr_in_t addr;
    ip_addr_copy(addr.addr, nameserver);
    addr.family = AF_INET;
    addr.port = htons(53);

    ret = connect(fd, (sockaddr_t *)&addr, sizeof(sockaddr_in_t));
    LOGK("socket connect %d\n", ret);

    dns_t *dns = (dns_t *)buf;
    memset(dns, 0, sizeof(dns_t));
    u16 id = time() & 0xFFFF;
    dns->id = id;
    dns->rd = 1;
    dns->qdcount = htons(1);

    char *ptr = dns->payload;
    int size = fill_payload(dns->payload, name, DNS_TYPE_A, DNS_CLASS_IN);

    ret = send(fd, dns, sizeof(dns_t) + size, 0);
    LOGK("socket send %d\n", ret);

    while (true)
    {
        ret = recv(fd, (void *)buf, PACKET_SIZE, 0);
        LOGK("socket recv %d\n", ret);
        if (id == dns->id)
            break;
    }

    dns->qdcount = ntohs(dns->qdcount);
    dns->ancount = ntohs(dns->ancount);
    dns->nscount = ntohs(dns->nscount);
    dns->arcount = ntohs(dns->arcount);

    ret = resolve(dns, ret, result);

rollback:
    if (fd > 0)
        close(fd);
    lock_release(&resolv_lock);
    return ret;
}

void resolv_init()
{
    buf = pbuf_get();

    lock_init(&resolv_lock);
    inet_aton("114.114.114.114", nameserver);
    fd_t fd = open("/etc/resolv.conf", O_RDONLY, 0);
    if (fd < 0)
        return;

    int len = read(fd, buf, 1024);
    if (len < EOK)
        goto rollback;

    char *next = buf - 1;

    while (true)
    {
        if (!next)
            break;

        char *ptr = next + 1;
        next = strchr(ptr, '\n');
        if (next)
            *next = 0;

        if (memcmp(ptr, "nameserver=", 11))
            continue;
        if (inet_aton(ptr + 11, nameserver) == EOK)
            break;
    }
rollback:
    if (fd > 0)
        close(fd);
    return;
}
