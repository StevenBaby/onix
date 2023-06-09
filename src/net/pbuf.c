#include <onix/net.h>
#include <onix/memory.h>
#include <onix/arena.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

static list_t free_buf_list;
static size_t pbuf_count = 0;

// 获取空闲缓冲
pbuf_t *pbuf_get()
{
    pbuf_t *pbuf = NULL;
    if (list_empty(&free_buf_list))
    {
        u32 page = alloc_kpage(1);
        pbuf = (pbuf_t *)page;
        list_push(&free_buf_list, &pbuf->node);

        page += PAGE_SIZE / 2;
        pbuf = (pbuf_t *)page;
        list_push(&free_buf_list, &pbuf->node);

        pbuf_count += 2;
        LOGK("pbuf count %d\n", pbuf_count);
    }
    pbuf = element_entry(pbuf_t, node, list_popback(&free_buf_list));

    // 应该对齐到 2K
    assert(((u32)pbuf & 0x7ff) == 0);

    pbuf->count = 1;
    return pbuf;
}

// 释放缓冲
void pbuf_put(pbuf_t *pbuf)
{
    // 应该对齐到 2K
    assert(((u32)pbuf & 0x7ff) == 0);

    assert(pbuf->count > 0);
    pbuf->count--;
    if (pbuf->count > 0)
    {
        assert(pbuf->node.next && pbuf->node.prev);
        return;
    }

    list_push(&free_buf_list, &pbuf->node);
}

// 初始化数据包缓冲
void pbuf_init()
{
    list_init(&free_buf_list);
}
