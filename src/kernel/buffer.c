#include <onix/buffer.h>
#include <onix/memory.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/device.h>
#include <onix/string.h>
#include <onix/task.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define HASH_COUNT 31 // 应该是个素数

static buffer_t *buffer_start = (buffer_t *)KERNEL_BUFFER_MEM;
static u32 buffer_count = 0;

// 记录当前 buffer_t 结构体位置
static buffer_t *buffer_ptr = (buffer_t *)KERNEL_BUFFER_MEM;

// 记录当前数据缓冲区位置
static void *buffer_data = (void *)(KERNEL_BUFFER_MEM + KERNEL_BUFFER_SIZE - BLOCK_SIZE);

static list_t free_list;              // 缓存链表，被释放的块
static list_t wait_list;              // 等待进程链表
static list_t hash_table[HASH_COUNT]; // 缓存哈希表

// 哈希函数
u32 hash(dev_t dev, idx_t block)
{
    return (dev ^ block) % HASH_COUNT;
}

// 从哈希表中查找 buffer
static buffer_t *get_from_hash_table(dev_t dev, idx_t block)
{
    u32 idx = hash(dev, block);
    list_t *list = &hash_table[idx];
    buffer_t *bf = NULL;

    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next)
    {
        buffer_t *ptr = element_entry(buffer_t, hnode, node);
        if (ptr->dev == dev && ptr->block == block)
        {
            bf = ptr;
            break;
        }
    }

    if (!bf)
    {
        return NULL;
    }

    // 如果 bf 在空闲列表中，则移除
    if (list_search(&free_list, &bf->rnode))
    {
        list_remove(&bf->rnode);
    }

    return bf;
}

// 将 bf 放入哈希表
static void hash_locate(buffer_t *bf)
{
    u32 idx = hash(bf->dev, bf->block);
    list_t *list = &hash_table[idx];
    assert(!list_search(list, &bf->hnode));
    list_push(list, &bf->hnode);
}

// 将 bf 从哈希表中移除
static void hash_remove(buffer_t *bf)
{
    u32 idx = hash(bf->dev, bf->block);
    list_t *list = &hash_table[idx];
    assert(list_search(list, &bf->hnode));
    list_remove(&bf->hnode);
}

// 直接初始化过慢，按需取用
static buffer_t *get_new_buffer()
{
    buffer_t *bf = NULL;

    if ((u32)buffer_ptr + sizeof(buffer_t) < (u32)buffer_data)
    {
        bf = buffer_ptr;
        bf->data = buffer_data;
        bf->dev = EOF;
        bf->block = 0;
        bf->count = 0;
        bf->dirty = false;
        bf->valid = false;
        lock_init(&bf->lock);
        buffer_count++;
        buffer_ptr++;
        buffer_data -= BLOCK_SIZE;
        LOGK("buffer count %d\n", buffer_count);
    }

    return bf;
}

// 获得空闲的 buffer
static buffer_t *get_free_buffer()
{
    buffer_t *bf = NULL;
    while (true)
    {
        // 如果内存够，直接获得缓存
        bf = get_new_buffer();
        if (bf)
        {
            return bf;
        }
        // 否则，从空闲列表中取得
        if (!list_empty(&free_list))
        {
            // 取最远未访问过的块
            bf = element_entry(buffer_t, rnode, list_popback(&free_list));
            hash_remove(bf);
            bf->valid = false;
            return bf;
        }
        // 等待某个缓冲释放
        task_block(running_task(), &wait_list, TASK_BLOCKED, TIMELESS);
    }
}

// 获得设备 dev，第 block 对应的缓冲
buffer_t *getblk(dev_t dev, idx_t block)
{
    buffer_t *bf = get_from_hash_table(dev, block);
    if (bf)
    {
        bf->count++;
        return bf;
    }

    bf = get_free_buffer();
    assert(bf->count == 0);
    assert(bf->dirty == 0);

    bf->count = 1;
    bf->dev = dev;
    bf->block = block;
    hash_locate(bf);
    return bf;
}

// 读取 dev 的 block 块
buffer_t *bread(dev_t dev, idx_t block)
{
    buffer_t *bf = getblk(dev, block);
    assert(bf != NULL);
    if (bf->valid)
    {
        return bf;
    }

    lock_acquire(&bf->lock);

    if (!bf->valid)
    {
        device_request(bf->dev, bf->data, BLOCK_SECS, bf->block * BLOCK_SECS, 0, REQ_READ);
        bf->dirty = false;
        bf->valid = true;
    }

    lock_release(&bf->lock);
    return bf;
}

// 写缓冲
void bwrite(buffer_t *bf)
{
    assert(bf);
    if (!bf->dirty)
        return;
    device_request(bf->dev, bf->data, BLOCK_SECS, bf->block * BLOCK_SECS, 0, REQ_WRITE);
    bf->dirty = false;
    bf->valid = true;
}

// 释放缓冲
void brelse(buffer_t *bf)
{
    if (!bf)
        return;
    if (bf->dirty)
    {
        bwrite(bf); // todo need write?
    }

    bf->count--;
    assert(bf->count >= 0);
    if (bf->count) // 还有人用，直接返回
        return;

    // if (bf->rnode.next)
    // {
    //     list_remove(&bf->rnode);
    // }
    assert(!bf->rnode.next);
    assert(!bf->rnode.prev);
    list_push(&free_list, &bf->rnode);
    if (!list_empty(&wait_list))
    {
        task_t *task = element_entry(task_t, node, list_popback(&wait_list));
        task_unblock(task, EOK);
    }
}

void buffer_init()
{
    LOGK("buffer_t size is %d\n", sizeof(buffer_t));

    // 初始化空闲链表
    list_init(&free_list);
    // 初始化等待进程链表
    list_init(&wait_list);

    // 初始化哈希表
    for (size_t i = 0; i < HASH_COUNT; i++)
    {
        list_init(&hash_table[i]);
    }
}
