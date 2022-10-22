#include <onix/fs.h>
#include <onix/debug.h>
#include <onix/bitmap.h>
#include <onix/assert.h>
#include <onix/string.h>
#include <onix/buffer.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 分配一个文件块
idx_t balloc(dev_t dev)
{
    super_block_t *sb = get_super(dev);
    assert(sb);

    buffer_t *buf = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    for (size_t i = 0; i < ZMAP_NR; i++)
    {
        buf = sb->zmaps[i];
        assert(buf);

        // 将整个缓冲区作为位图
        bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS + sb->desc->firstdatazone - 1);

        // 从位图中扫描一位
        bit = bitmap_scan(&map, 1);
        if (bit != EOF)
        {
            // 如果扫描成功，则 标记缓冲区脏，中止查找
            assert(bit < sb->desc->zones);
            buf->dirty = true;
            break;
        }
    }
    bwrite(buf); // todo 调试期间强同步
    return bit;
}

// 释放一个文件块
void bfree(dev_t dev, idx_t idx)
{
    super_block_t *sb = get_super(dev);
    assert(sb != NULL);
    assert(idx < sb->desc->zones);

    buffer_t *buf;
    bitmap_t map;
    for (size_t i = 0; i < ZMAP_NR; i++)
    {
        // 跳过开始的块
        if (idx > BLOCK_BITS * (i + 1))
        {
            continue;
        }

        buf = sb->zmaps[i];
        assert(buf);

        // 将整个缓冲区作为位图
        bitmap_make(&map, buf->data, BLOCK_SIZE, BLOCK_BITS * i + sb->desc->firstdatazone - 1);

        // 将 idx 对应的位图置位 0
        assert(bitmap_test(&map, idx));
        bitmap_set(&map, idx, 0);

        // 标记缓冲区脏
        buf->dirty = true;
        break;
    }
    bwrite(buf); // todo 调试期间强同步
}

// 分配一个文件系统 inode
idx_t ialloc(dev_t dev)
{
    super_block_t *sb = get_super(dev);
    assert(sb);

    buffer_t *buf = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    for (size_t i = 0; i < IMAP_NR; i++)
    {
        buf = sb->imaps[i];
        assert(buf);

        bitmap_make(&map, buf->data, BLOCK_BITS, i * BLOCK_BITS);
        bit = bitmap_scan(&map, 1);
        if (bit != EOF)
        {
            assert(bit < sb->desc->inodes);
            buf->dirty = true;
            break;
        }
    }
    bwrite(buf); // todo 调试期间强同步
    return bit;
}

// 释放一个文件系统 inode
void ifree(dev_t dev, idx_t idx)
{
    super_block_t *sb = get_super(dev);
    assert(sb != NULL);
    assert(idx < sb->desc->inodes);

    buffer_t *buf;
    bitmap_t map;
    for (size_t i = 0; i < IMAP_NR; i++)
    {
        if (idx > BLOCK_BITS * (i + 1))
        {
            continue;
        }

        buf = sb->imaps[i];
        assert(buf);

        bitmap_make(&map, buf->data, BLOCK_BITS, i * BLOCK_BITS);
        assert(bitmap_test(&map, idx));
        bitmap_set(&map, idx, 0);
        buf->dirty = true;
        break;
    }
    bwrite(buf); // todo 调试期间强同步
}

// 获取 inode 第 block 块的索引值
// 如果不存在 且 create 为 true，则创建
idx_t bmap(inode_t *inode, idx_t block, bool create)
{
    // 确保 block 合法
    assert(block >= 0 && block < TOTAL_BLOCK);

    // 数组索引
    u16 index = block;

    // 数组
    u16 *array = inode->desc->zone;

    // 缓冲区
    buffer_t *buf = inode->buf;

    // 用于下面的 brelse，传入参数 inode 的 buf 不应该释放
    buf->count += 1;

    // 当前处理级别
    int level = 0;

    // 当前子级别块数量
    int divider = 1;

    // 直接块
    if (block < DIRECT_BLOCK)
    {
        goto reckon;
    }

    block -= DIRECT_BLOCK;

    if (block < INDIRECT1_BLOCK)
    {
        index = DIRECT_BLOCK;
        level = 1;
        divider = 1;
        goto reckon;
    }

    block -= INDIRECT1_BLOCK;
    assert(block < INDIRECT2_BLOCK);
    index = DIRECT_BLOCK + 1;
    level = 2;
    divider = BLOCK_INDEXES;

reckon:
    for (; level >= 0; level--)
    {
        // 如果不存在 且 create 则申请一块文件块
        if (!array[index] && create)
        {
            array[index] = balloc(inode->dev);
            buf->dirty = true;
        }

        brelse(buf);

        // 如果 level == 0 或者 索引不存在，直接返回
        if (level == 0 || !array[index])
        {
            return array[index];
        }

        // level 不为 0，处理下一级索引
        buf = bread(inode->dev, array[index]);
        index = block / divider;
        block = block % divider;
        divider /= BLOCK_INDEXES;
        array = (u16 *)buf->data;
    }
}
