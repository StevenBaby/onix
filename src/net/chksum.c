#include <onix/net/chksum.h>
#include <onix/net/types.h>

#define CRC_POLY 0xEDB88320

u32 eth_fcs(void *data, int len)
{
    u32 crc = -1;
    u8 *ptr = (u8 *)data;
    for (int i = 0; i < len; i++)
    {
        crc ^= ptr[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ CRC_POLY;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

static u16 chksum(void *data, int len, u32 sum)
{
    u16 *ptr = (u16 *)data;
    for (; len > 1; len -= 2)
    {
        // 防止溢出
        if (sum & 0x80000000)
            sum = (sum & 0xFFFF) + (sum >> 16);
        sum += *ptr++;
    }

    if (len == 1)
        sum += *(u8 *)ptr;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (u16)sum;
}

u16 ip_chksum(void *data, int len)
{
    u16 sum = chksum(data, len, 0);
    return ~sum;
}

u16 inet_chksum(void *data, u16 len, ip_addr_t dst, ip_addr_t src, u16 proto)
{
    u32 sum = 0;
    u16 *ptr;

    ptr = (u16 *)dst;
    sum += *ptr++;
    sum += *ptr;

    ptr = (u16 *)src;
    sum += *ptr++;
    sum += *ptr;

    sum += htons(proto);
    sum += htons(len);

    sum = chksum(data, len, sum);

    return (u16)(~sum);
}
