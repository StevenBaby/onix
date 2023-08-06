#include <onix/net/tcp.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

enum
{
    OPT_END = 0,   // 选项列表结束
    OPT_NOP = 1,   // 无操作，用于四字节对齐
    OPT_MSS = 2,   // Maximum Segment Size 最大分段大小
    OPT_WS = 3,    // Window Scale 窗口扩大
    OPT_SACKP = 4, // Selective Acknowledgements Permited
    OPT_SACK = 5,  // Selective Acknowledgements
    OPT_ECHO = 6,  // Echo
    OPT_ER = 7,    // Echo Reply
    OPT_TS = 8,    // Timestamps 时间戳
};

typedef struct tcp_opt_t
{
    u8 type;
    u8 size;
    u8 value[0];
} _packed tcp_opt_t;

err_t tcp_parse_option(tcp_pcb_t *pcb, tcp_t *tcp)
{
    // reference
    // https://datatracker.ietf.org/doc/html/rfc9293#name-specific-option-definitions

    u8 *ptr = tcp->options;
    u8 *payload = (u8 *)tcp + (tcp->len * 4);

    while (ptr < payload)
    {
        tcp_opt_t *opt = (tcp_opt_t *)ptr;
        switch (opt->type)
        {
        case OPT_END:
            return EOK;
        case OPT_NOP:
            ptr++;
            continue;
        case OPT_MSS:
            if (opt->size != 4)
                return -EOPTION;
            pcb->snd_mss = ntohs(*(u16 *)&opt->value);
            LOGK("TCP parse option mss %d\n", pcb->snd_mss);
            break;
        case OPT_WS:
            break;
        case OPT_SACKP:
            break;
        case OPT_SACK:
            break;
        case OPT_TS:
            break;
        default:
            LOGK("TCP option unknown %d\n", opt->type);
            break;
        }
        ptr += opt->size;
    }
    return EOK;
}

err_t tcp_write_option(tcp_pcb_t *pcb, tcp_t *tcp)
{
    u8 *ptr = tcp->options;

    tcp_opt_t *opt;
    u32 size = 0;
    bool has_option = false;

    if (tcp->syn)
    {
        opt = (tcp_opt_t *)ptr;
        opt->type = OPT_MSS;
        opt->size = 4;
        *(u16 *)&opt->value = htons(pcb->rcv_mss);
        ptr += 4;
        size += 4;
        has_option = true;
    }

    if (has_option)
    {
        opt = (tcp_opt_t *)ptr;
        memset(ptr, 0, 4);
        opt->type = OPT_END;
        size += 4;
    }

    tcp->len = (sizeof(tcp_t) + size) / 4;
    return size + sizeof(tcp_t);
}
