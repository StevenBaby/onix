#include <onix/buffer.h>
#include <onix/memory.h>
#include <onix/arena.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/device.h>
#include <onix/string.h>
#include <onix/task.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define BUFFER_DESC_NR 3 // 描述符数量 1024 2048 4096

static bdesc_t bdescs[BUFFER_DESC_NR];

// 哈希函数
u32 hash(dev_t dev, idx_t block)
{
    return (dev ^ block) % HASH_COUNT;
}

static bdesc_t *desc_get(int size)
{
    for (size_t i = 0; i < BUFFER_DESC_NR; i++)
    {
        bdesc_t *desc = &bdescs[i];
        if (desc->size == size)
        {
            return desc;
        }
    }
    panic("buffer_size %d has no buffer\n", size);
}

// 从哈希表中查找 buffer
static buffer_t *get_from_hash_table(bdesc_t *desc, dev_t dev, idx_t block)
{
    u32 idx = hash(dev, block);
    list_t *list = &desc->hash_table[idx];
    buffer_t *buf = NULL;

    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next)
    {
        buffer_t *ptr = element_entry(buffer_t, hnode, node);
        if (ptr->dev == dev && ptr->block == block)
        {
            buf = ptr;
            break;
        }
    }

    if (!buf)
    {
        return NULL;
    }

    // 如果 buf 在空闲列表中，则移除
    if (list_search(&desc->idle_list, &buf->rnode))
    {
        list_remove(&buf->rnode);
    }

    return buf;
}

// 将 buf 放入哈希表
static void hash_locate(bdesc_t *desc, buffer_t *buf)
{
    u32 idx = hash(buf->dev, buf->block);
    list_t *list = &desc->hash_table[idx];
    assert(!list_search(list, &buf->hnode));
    list_push(list, &buf->hnode);
}

// 将 buf 从哈希表中移除
static void hash_remove(bdesc_t *desc, buffer_t *buf)
{
    u32 idx = hash(buf->dev, buf->block);
    list_t *list = &desc->hash_table[idx];
    if (buf->hnode.next)
    {
        list_remove(&buf->hnode);
    }
}

// 初始化缓冲
static err_t buffer_alloc(bdesc_t *desc)
{
    buffer_t *buf = NULL;
    void *addr = (void *)alloc_kpage(1);
    int left = PAGE_SIZE;

    for (size_t left = PAGE_SIZE; left > 0;
         left -= desc->size, addr += desc->size, desc->count++)
    {
        buf = kmalloc(sizeof(buffer_t));

        buf->desc = desc;
        buf->data = addr;
        buf->dev = EOF;
        buf->block = 0;
        buf->count = 0;
        buf->dirty = false;
        buf->valid = false;
        lock_init(&buf->lock);

        list_push(&desc->free_list, &buf->rnode);
        LOGK("buffer size %d count %d\n", desc->size, desc->count);
    }

    return EOK;
}

// 获得空闲的 buffer
static buffer_t *get_free_buffer(bdesc_t *desc)
{
    if (desc->count < MAX_BUF_COUNT && list_empty(&desc->free_list))
    {
        buffer_alloc(desc);
    }

    if (!list_empty(&desc->free_list))
    {
        // 取最远未访问过的块
        buffer_t *buf = element_entry(buffer_t, rnode, list_popback(&desc->free_list));
        hash_remove(desc, buf);
        buf->valid = false;
        return buf;
    }

    while (list_empty(&desc->idle_list))
    {
        // 等待某个缓冲释放
        task_block(running_task(), &desc->wait_list, TASK_BLOCKED, TIMELESS);
    }

    assert(!list_empty(&desc->idle_list));
    // 取最远未访问过的块
    buffer_t *buf = element_entry(buffer_t, rnode, list_popback(&desc->idle_list));
    hash_remove(desc, buf);
    buf->valid = false;
    return buf;
}

// 获得设备 dev，第 block 对应的缓冲
static buffer_t *getblk(bdesc_t *desc, dev_t dev, idx_t block)
{
    buffer_t *buf = get_from_hash_table(desc, dev, block);
    if (buf)
    {
        buf->count++;
        return buf;
    }

    buf = get_free_buffer(desc);
    assert(buf->count == 0);
    assert(buf->dirty == 0);

    buf->count = 1;
    buf->dev = dev;
    buf->block = block;
    hash_locate(desc, buf);
    return buf;
}

// 读取 dev 的 block 块
buffer_t *bread(dev_t dev, idx_t block, size_t size)
{
    bdesc_t *desc = desc_get(size);

    buffer_t *buf = getblk(desc, dev, block);

    assert(buf != NULL);
    if (buf->valid)
    {
        return buf;
    }

    lock_acquire(&buf->lock);

    if (!buf->valid)
    {
        u32 block_size = desc->size;
        u32 sector_size = device_ioctl(dev, DEV_CMD_SECTOR_SIZE, 0, 0);
        if (sector_size > block_size)
            goto rollback;

        u32 bs = block_size / sector_size;

        int ret = device_request(
            buf->dev,
            buf->data,
            bs,
            buf->block * bs,
            0,
            REQ_READ);
        if (ret < EOK)
            goto rollback;

        bdirty(buf, false);
        buf->valid = true;
    }

    lock_release(&buf->lock);
    return buf;

rollback:
    lock_release(&buf->lock);
    brelse(buf);
    return NULL;
}

// 写缓冲
err_t bwrite(buffer_t *buf)
{
    assert(buf);
    if (!buf->dirty)
        return EOK;

    bdesc_t *desc = buf->desc;

    u32 block_size = desc->size;
    u32 sector_size = device_ioctl(buf->dev, DEV_CMD_SECTOR_SIZE, 0, 0);

    u32 block_sector = block_size / sector_size;
    int ret = device_request(
        buf->dev, buf->data, block_sector,
        buf->block * block_sector, 0, REQ_WRITE);

    if (ret < 0)
        return ret;

    bdirty(buf, false);
    buf->valid = true;
    return ret;
}

// 释放缓冲
err_t brelse(buffer_t *buf)
{
    if (!buf)
        return EOK;

    int ret = bwrite(buf);

    buf->count--;
    assert(buf->count >= 0);
    if (buf->count) // 还有人用，直接返回
        return ret;

    assert(!buf->rnode.next);
    assert(!buf->rnode.prev);

    bdesc_t *desc = buf->desc;
    list_push(&desc->idle_list, &buf->rnode);

    if (!list_empty(&desc->wait_list))
    {
        task_t *task = element_entry(task_t, node, list_popback(&desc->wait_list));
        task_unblock(task, EOK);
    }
    return EOK;
}

err_t bdirty(buffer_t *buf, bool dirty)
{
    buf->dirty = dirty;
}

void buffer_init()
{
    LOGK("buffer_t size is %d\n", sizeof(buffer_t));

    size_t size = 1024;
    for (size_t i = 0; i < BUFFER_DESC_NR; i++)
    {
        bdesc_t *desc = &bdescs[i];
        desc->size = size;
        desc->count = 0;

        size <<= 1;

        // 初始化空闲链表
        list_init(&desc->free_list);
        // 初始化缓冲链表
        list_init(&desc->idle_list);
        // 初始化等待进程链表
        list_init(&desc->wait_list);

        // 初始化哈希表
        for (size_t i = 0; i < HASH_COUNT; i++)
        {
            list_init(&desc->hash_table[i]);
        }
    }
}
