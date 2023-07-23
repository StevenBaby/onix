#include <onix/net/addr.h>
#include <onix/string.h>
#include <onix/stdlib.h>
#include <onix/errno.h>

// MAC 地址拷贝
void eth_addr_copy(eth_addr_t dst, eth_addr_t src)
{
    memcpy(dst, src, ETH_ADDR_LEN);
}

bool eth_addr_isany(eth_addr_t addr)
{
    if (addr == NULL)
        return true;

    for (size_t i = 0; i < ETH_ADDR_LEN; i++)
    {
        if (addr[i] != 0)
            return false;
    }
    return true;
}

bool eth_addr_cmp(eth_addr_t addr1, eth_addr_t addr2)
{
    for (size_t i = 0; i < ETH_ADDR_LEN; i++)
    {
        if (addr1[i] != addr2[i])
            return false;
    }
    return true;
}

// IP 地址拷贝
void ip_addr_copy(ip_addr_t dst, ip_addr_t src)
{
    *(u32 *)dst = *(u32 *)src;
}

// 字符串转换 IP 地址
err_t inet_aton(const char *str, ip_addr_t addr)
{
    const char *ptr = str;

    u8 parts[4];

    for (size_t i = 0; i < 4 && *ptr != '\0'; i++, ptr++)
    {
        int value = 0;
        int k = 0;
        for (; true; ptr++, k++)
        {
            if (*ptr == '.' || *ptr == '\0')
            {
                break;
            }
            if (!isdigit(*ptr))
            {
                return -EADDR;
            }
            value = value * 10 + ((*ptr) - '0');
        }
        if (k == 0 || value < 0 || value > 255)
        {
            return -EADDR;
        }
        parts[i] = value;
    }

    ip_addr_copy(addr, parts);
    return EOK;
}

// 比较两 ip 地址是否相等
bool ip_addr_cmp(ip_addr_t addr1, ip_addr_t addr2)
{
    u32 a1 = *(u32 *)addr1;
    u32 a2 = *(u32 *)addr2;
    return a1 == a2;
}

// 比较两地址是否在同一子网
bool ip_addr_maskcmp(ip_addr_t addr1, ip_addr_t addr2, ip_addr_t mask)
{
    u32 a1 = *(u32 *)addr1;
    u32 a2 = *(u32 *)addr2;
    u32 m = *(u32 *)mask;
    return (a1 & m) == (a2 & m);
}

// 判断地址是否是广播地址
bool ip_addr_isbroadcast(ip_addr_t addr, ip_addr_t mask)
{
    u32 a = *(u32 *)addr;
    u32 m = *(u32 *)mask;
    return (a & ~m) == (-1 & (~m)) || a == -1 || a == 0;
}

// 判断地址是否全为 0
bool ip_addr_isany(ip_addr_t addr)
{
    if (addr == NULL)
        return true;
    u32 a = *(u32 *)addr;
    return a == 0;
}

// 判断地址是否为多播地址
bool ip_addr_ismulticast(ip_addr_t addr)
{
    u32 a = *(u32 *)addr;
    return (a & ntohl(0xF0000000)) == ntohl(0xE0000000);
}
