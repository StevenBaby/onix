#include <onix/fs.h>
#include <onix/stat.h>
#include <onix/syscall.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/buffer.h>
#include <onix/arena.h>
#include <onix/bitmap.h>
#include <onix/string.h>
#include <onix/stdlib.h>
#include <onix/memory.h>
#include <onix/stat.h>
#include <onix/task.h>
#include <onix/device.h>

#include "minix.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 分配一个文件块
idx_t minix_balloc(super_t *super)
{
    buffer_t *buf = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    minix_super_t *desc = (minix_super_t *)super->desc;
    idx_t bidx = 2 + desc->imap_blocks;

    for (size_t i = 0; i < desc->zmap_blocks; i++)
    {
        buf = bread(super->dev, bidx + i, BLOCK_SIZE);
        assert(buf);

        // 将整个缓冲区作为位图
        bitmap_make(&map, buf->data, BLOCK_SIZE, i * BLOCK_BITS + desc->firstdatazone - 1);

        // 从位图中扫描一位
        bit = bitmap_scan(&map, 1);
        if (bit != EOF)
        {
            // 如果扫描成功，则 标记缓冲区脏，中止查找
            assert(bit < desc->zones);
            buf->dirty = true;
            break;
        }
    }
    brelse(buf); // todo 调试期间强同步
    return bit;
}

// 释放一个文件块
void minix_bfree(super_t *super, idx_t idx)
{
    minix_super_t *desc = (minix_super_t *)super->desc;
    assert(idx < desc->zones);

    buffer_t *buf;
    bitmap_t map;

    idx_t bidx = 2 + desc->imap_blocks;

    for (size_t i = 0; i < desc->zmap_blocks; i++)
    {
        // 跳过开始的块
        if (idx > BLOCK_BITS * (i + 1))
        {
            continue;
        }

        buf = bread(super->dev, bidx + i, BLOCK_SIZE);
        assert(buf);

        // 将整个缓冲区作为位图
        bitmap_make(&map, buf->data, BLOCK_SIZE, BLOCK_BITS * i + desc->firstdatazone - 1);

        // 将 idx 对应的位图置位 0
        assert(bitmap_test(&map, idx));
        bitmap_set(&map, idx, 0);

        // 标记缓冲区脏
        buf->dirty = true;
        break;
    }
    brelse(buf); // todo 调试期间强同步
}

// 分配一个文件系统 inode
idx_t minix_ialloc(super_t *super)
{
    buffer_t *buf = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    minix_super_t *desc = (minix_super_t *)super->desc;
    idx_t bidx = 2;
    for (size_t i = 0; i < desc->imap_blocks; i++)
    {
        buf = bread(super->dev, bidx + i, BLOCK_SIZE);
        assert(buf);

        bitmap_make(&map, buf->data, BLOCK_BITS, i * BLOCK_BITS);
        bit = bitmap_scan(&map, 1);
        if (bit != EOF)
        {
            assert(bit < desc->inodes);
            buf->dirty = true;
            break;
        }
    }
    brelse(buf); // todo 调试期间强同步
    return bit;
}

// 释放一个文件系统 inode
void minix_ifree(super_t *super, idx_t idx)
{
    minix_super_t *desc = (minix_super_t *)super->desc;
    assert(idx < desc->inodes);

    buffer_t *buf;
    bitmap_t map;

    idx_t bidx = 2;
    for (size_t i = 0; i < desc->imap_blocks; i++)
    {
        if (idx > BLOCK_BITS * (i + 1))
        {
            continue;
        }

        buf = bread(super->dev, bidx + i, BLOCK_SIZE);
        assert(buf);

        bitmap_make(&map, buf->data, BLOCK_BITS, i * BLOCK_BITS);
        assert(bitmap_test(&map, idx));
        bitmap_set(&map, idx, 0);
        buf->dirty = true;
        break;
    }
    brelse(buf); // todo 调试期间强同步
}

// 获取 inode 第 block 块的索引值
// 如果不存在 且 create 为 true，则创建
idx_t minix_bmap(inode_t *inode, idx_t block, bool create)
{
    // 确保 block 合法
    assert(block >= 0 && block < TOTAL_BLOCK);

    // 数组索引
    u16 index = block;

    minix_inode_t *minode = (minix_inode_t *)inode->desc;

    // 数组
    u16 *array = minode->zone;

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
            array[index] = minix_balloc(inode->super);
            buf->dirty = true;
        }

        brelse(buf);

        // 如果 level == 0 或者 索引不存在，直接返回
        if (level == 0 || !array[index])
        {
            return array[index];
        }

        // level 不为 0，处理下一级索引
        buf = bread(inode->dev, array[index], BLOCK_SIZE);
        index = block / divider;
        block = block % divider;
        divider /= BLOCK_INDEXES;
        array = (u16 *)buf->data;
    }
}

// 计算 inode nr 对应的块号
static inline idx_t inode_block(minix_super_t *desc, idx_t nr)
{
    // inode 编号 从 1 开始
    return 2 + desc->imap_blocks + desc->zmap_blocks + (nr - 1) / BLOCK_INODES;
}

// 获得设备 dev 的 nr inode
static inode_t *iget(dev_t dev, idx_t nr)
{
    inode_t *inode = find_inode(dev, nr);
    if (inode)
    {
        inode->count++;
        inode->atime = time();
        return fit_inode(inode);
    }

    super_t *super = get_super(dev);
    assert(super);

    minix_super_t *desc = (minix_super_t *)super->desc;

    assert(nr <= desc->inodes);

    inode = get_free_inode();
    inode->dev = dev;
    inode->nr = nr;
    inode->count++;

    // 加入超级块 inode 链表
    list_push(&super->inode_list, &inode->node);

    idx_t block = inode_block(desc, inode->nr);
    buffer_t *buf = bread(inode->dev, block, BLOCK_SIZE);

    inode->buf = buf;

    // 将缓冲视为一个 inode 描述符数组，获取对应的指针；
    inode->desc = &((minix_inode_t *)buf->data)[(inode->nr - 1) % BLOCK_INODES];
    minix_inode_t *minode = (minix_inode_t *)inode->desc;

    inode->rdev = minode->zone[0];

    inode->atime = time();
    inode->mtime = minode->mtime;
    inode->ctime = minode->mtime;

    inode->mode = minode->mode;
    inode->size = minode->size;
    inode->super = super;

    inode->uid = minode->uid;
    inode->gid = minode->gid;

    inode->type = FS_TYPE_MINIX;
    inode->op = fs_get_op(FS_TYPE_MINIX);

    return fit_inode(inode);
}

static inode_t *new_inode(dev_t dev, idx_t nr)
{
    task_t *task = running_task();
    inode_t *inode = iget(dev, nr);

    // assert(inode->desc->nlinks == 0);

    inode->buf->dirty = true;

    minix_inode_t *minode = (minix_inode_t *)inode->desc;
    memset(minode, 0, sizeof(minix_inode_t));

    inode->mode = minode->mode = 0777 & (~task->umask);
    inode->uid = minode->uid = task->uid;
    inode->gid = minode->gid = task->gid;

    inode->size = minode->size = 0;
    minode->mtime = inode->atime = inode->mtime = inode->ctime = time();
    minode->nlinks = 1;

    return inode;
}

// 关闭 inode
static void minix_close(inode_t *inode)
{
    assert(inode->type == FS_TYPE_MINIX);

    // TODO: need write... ?
    if (inode->buf->dirty)
    {
        bwrite(inode->buf);
    }

    inode->count--;

    if (inode->count)
    {
        return;
    }

    // 释放 inode 对应的缓冲
    brelse(inode->buf);

    // 从超级块链表中移除
    list_remove(&inode->node);

    // 释放 inode 内存
    put_free_inode(inode);
}

// 从 inode 的 offset 处，读 len 个字节到 buf
static int minix_read(inode_t *inode, char *data, int len, off_t offset)
{
    minix_inode_t *minode = (minix_inode_t *)inode->desc;
    if (ISCHR(minode->mode))
    {
        assert(minode->zone[0]);
        return device_read(minode->zone[0], data, len, 0, 0);
    }
    else if (ISBLK(minode->mode))
    {
        assert(minode->zone[0]);
        device_t *device = device_get(minode->zone[0]);
        assert(len % BLOCK_SIZE == 0);
        assert(device_read(minode->zone[0], data, len / BLOCK_SIZE, offset / BLOCK_SIZE, 0) == EOK);
        return len;
    }

    assert(ISFILE(minode->mode) || ISDIR(minode->mode));

    // 如果偏移量超过文件大小，返回 EOF
    if (offset >= minode->size)
    {
        return EOF;
    }

    // 开始读取的位置
    u32 begin = offset;

    // 剩余字节数
    u32 left = MIN(len, minode->size - offset);
    while (left)
    {
        // 找到对应的文件便宜，所在文件块
        idx_t nr = minix_bmap(inode, offset / BLOCK_SIZE, false);
        assert(nr);

        // 读取文件块缓冲
        buffer_t *buf = bread(inode->dev, nr, BLOCK_SIZE);

        // 文件块中的偏移量
        u32 start = offset % BLOCK_SIZE;

        // 本次需要读取的字节数
        u32 chars = MIN(BLOCK_SIZE - start, left);

        // 更新 偏移量 和 剩余字节数
        offset += chars;
        left -= chars;

        // 文件块中的指针
        char *ptr = buf->data + start;

        // 拷贝内容
        memcpy(data, ptr, chars);

        // 更新缓存位置
        data += chars;

        // 释放文件块缓冲
        brelse(buf);
    }

    // 更新访问时间
    inode->atime = time();

    // 返回读取数量
    return offset - begin;
}

// 从 inode 的 offset 处，将 data 的 len 个字节写入磁盘
static int minix_write(inode_t *inode, char *data, int len, off_t offset)
{
    minix_inode_t *minode = (minix_inode_t *)inode->desc;
    if (ISCHR(minode->mode))
    {
        assert(minode->zone[0]);
        device_t *device = device_get(minode->zone[0]);
        return device_write(minode->zone[0], data, len, 0, 0);
    }
    else if (ISBLK(minode->mode))
    {
        assert(minode->zone[0]);
        device_t *device = device_get(minode->zone[0]);
        assert(len % BLOCK_SIZE == 0);
        assert(device_write(minode->zone[0], data, len / BLOCK_SIZE, offset / BLOCK_SIZE, 0) == EOK);
        return len;
    }

    // 不允许目录写入目录文件，修改目录有其他的专用方法
    assert(ISFILE(minode->mode));

    // 开始的位置
    u32 begin = offset;

    // 剩余数量
    u32 left = len;

    while (left)
    {
        // 找到文件块，若不存在则创建
        idx_t nr = minix_bmap(inode, offset / BLOCK_SIZE, true);
        assert(nr);

        // 将读入文件块
        buffer_t *buf = bread(inode->dev, nr, BLOCK_SIZE);
        buf->dirty = true;

        // 块中的偏移量
        u32 start = offset % BLOCK_SIZE;
        // 文件块中的指针
        char *ptr = buf->data + start;

        // 读取的数量
        u32 chars = MIN(BLOCK_SIZE - start, left);

        // 更新偏移量
        offset += chars;

        // 更新剩余字节数
        left -= chars;

        // 如果偏移量大于文件大小，则更新
        if (offset > minode->size)
        {
            inode->size = minode->size = offset;
            inode->buf->dirty = true;
        }

        // 拷贝内容
        memcpy(ptr, data, chars);

        // 更新缓存偏移
        data += chars;

        // 释放文件块
        brelse(buf);
    }

    // 更新修改时间
    minode->mtime = inode->atime = time();

    // TODO: 写入磁盘 ？
    bwrite(inode->buf);

    // 返回写入大小
    return offset - begin;
}

static void inode_bfree(inode_t *inode, u16 *array, int index, int level)
{
    if (!array[index])
    {
        return;
    }

    if (!level)
    {
        minix_bfree(inode->super, array[index]);
        return;
    }

    buffer_t *buf = bread(inode->dev, array[index], BLOCK_SIZE);
    for (size_t i = 0; i < BLOCK_INDEXES; i++)
    {
        inode_bfree(inode, (u16 *)buf->data, i, level - 1);
    }
    brelse(buf);
    minix_bfree(inode->super, array[index]);
}

// 读取文件路径
int minix_readdir(inode_t *inode, dentry_t *entry, size_t count, off_t offset)
{
    minix_dentry_t mentry;
    int ret = EOF;
    if ((ret = minix_read(inode, (char *)&mentry, sizeof(mentry), offset)) < 0)
        return ret;

    entry->length = sizeof(mentry);
    entry->namelen = strnlen(mentry.name, MINIX1_NAME_LEN);
    memcpy(entry->name, mentry.name, entry->namelen + 1);
    entry->name[entry->namelen] = 0;
    entry->nr = mentry.nr;
    return ret;
}

// 释放 inode 所有文件块
static int minix_truncate(inode_t *inode)
{
    minix_inode_t *minode = (minix_inode_t *)inode->desc;
    if (!ISFILE(minode->mode) && !ISDIR(minode->mode))
    {
        return -EPERM;
    }

    // 释放直接块
    for (size_t i = 0; i < DIRECT_BLOCK; i++)
    {
        inode_bfree(inode, minode->zone, i, 0);
        minode->zone[i] = 0;
    }

    // 释放一级间接块
    inode_bfree(inode, minode->zone, DIRECT_BLOCK, 1);
    minode->zone[DIRECT_BLOCK] = 0;

    // 释放二级间接块
    inode_bfree(inode, minode->zone, DIRECT_BLOCK + 1, 2);
    minode->zone[DIRECT_BLOCK + 1] = 0;

    inode->size = minode->size = 0;
    inode->buf->dirty = true;
    minode->mtime = time();
    bwrite(inode->buf);
    return EOK;
}

static int minix_stat(inode_t *inode, stat_t *statbuf)
{
    minix_inode_t *minode = (minix_inode_t *)inode->desc;
    statbuf->dev = inode->dev;        // 文件所在的设备号
    statbuf->nr = inode->nr;          // 文件 i 节点号
    statbuf->mode = minode->mode;     // 文件属性
    statbuf->nlinks = minode->nlinks; // 文件的连接数
    statbuf->uid = minode->uid;       // 文件的用户 id
    statbuf->gid = minode->gid;       // 文件的组 id
    statbuf->rdev = minode->zone[0];  // 设备号(如果文件是特殊的字符文件或块文件)
    statbuf->size = minode->size;     // 文件大小（字节数）（如果文件是常规文件）
    statbuf->atime = inode->atime;    // 最后访问时间
    statbuf->mtime = minode->mtime;   // 最后修改时间
    statbuf->ctime = inode->ctime;    // 最后节点修改时间
    return EOK;
}

// 判断权限
static int minix_permission(inode_t *inode, int mask)
{
    minix_inode_t *minode = (minix_inode_t *)inode->desc;
    u16 mode = minode->mode;

    if (!minode->nlinks)
        return false;

    task_t *task = running_task();
    if (task->uid == KERNEL_USER)
        return true;

    if (task->uid == minode->uid)
        mode >>= 6;
    else if (task->gid == minode->gid)
        mode >>= 3;

    if ((mode & mask & 0b111) == mask)
        return true;
    return false;
}

// 获取 dir 目录下的 name 目录 所在的 minix_dentry_t 和 buffer_t
static buffer_t *find_entry(inode_t *dir, const char *name, char **next, minix_dentry_t **result)
{
    // 保证 dir 是目录

    minix_inode_t *minode = (minix_inode_t *)dir->desc;
    assert(ISDIR(minode->mode));

    // dir 目录最多子目录数量
    u32 entries = minode->size / sizeof(minix_dentry_t);

    idx_t i = 0;
    idx_t block = 0;
    buffer_t *buf = NULL;
    minix_dentry_t *entry = NULL;
    idx_t nr = EOF;

    for (; i < entries; i++, entry++)
    {
        if (!buf || (u32)entry >= (u32)buf->data + BLOCK_SIZE)
        {
            brelse(buf);
            block = minix_bmap(dir, i / BLOCK_DENTRIES, false);
            assert(block);

            buf = bread(dir->dev, block, BLOCK_SIZE);
            entry = (minix_dentry_t *)buf->data;
        }
        if (match_name(name, entry->name, next, MINIX1_NAME_LEN) && entry->nr)
        {
            *result = entry;
            return buf;
        }
    }

    brelse(buf);
    return NULL;
}

// 在 dir 目录中添加 name 目录项
static buffer_t *add_entry(inode_t *dir, const char *name, minix_dentry_t **result)
{
    char *next = NULL;

    buffer_t *buf = find_entry(dir, name, &next, result);
    if (buf)
    {
        return buf;
    }

    // name 中不能有分隔符
    for (size_t i = 0; i < MINIX1_NAME_LEN && name[i]; i++)
    {
        assert(!IS_SEPARATOR(name[i]));
    }

    idx_t i = 0;
    idx_t block = 0;
    minix_dentry_t *entry;

    minix_inode_t *minode = (minix_inode_t *)dir->desc;
    for (; true; i++, entry++)
    {
        if (!buf || (u32)entry >= (u32)buf->data + BLOCK_SIZE)
        {
            brelse(buf);
            block = minix_bmap(dir, i / BLOCK_DENTRIES, true);
            assert(block);

            buf = bread(dir->dev, block, BLOCK_SIZE);
            entry = (minix_dentry_t *)buf->data;
        }
        if (i * sizeof(minix_dentry_t) >= minode->size)
        {
            entry->nr = 0;
            dir->size = minode->size = (i + 1) * sizeof(minix_dentry_t);
            dir->buf->dirty = true;
        }
        if (entry->nr)
            continue;

        strncpy(entry->name, name, MINIX1_NAME_LEN);

        buf->dirty = true;
        dir->mtime = minode->mtime = time();
        dir->buf->dirty = true;
        *result = entry;
        return buf;
    };
}

static int minix_open(inode_t *dir, char *name, int flags, int mode, inode_t **result)
{

    inode_t *inode = NULL;
    buffer_t *buf = NULL;
    minix_dentry_t *entry = NULL;
    char *next = NULL;
    int ret = EOF;

    if ((flags & O_TRUNC) && ((flags & O_ACCMODE) == O_RDONLY))
        flags |= O_RDWR;

    buf = find_entry(dir, name, &next, &entry);
    if (buf)
    {
        inode = iget(dir->dev, entry->nr);
        assert(inode);
        goto makeup;
    }

    if (!(flags & O_CREAT))
    {
        ret = -EEXIST;
        goto rollback;
    }

    if (!dir->op->permission(dir, P_WRITE))
    {
        ret = -EPERM;
        goto rollback;
    }

    buf = add_entry(dir, name, &entry);
    if (!buf)
    {
        ret = -EIO;
        goto rollback;
    }

    entry->nr = minix_ialloc(dir->super);
    inode = new_inode(dir->dev, entry->nr);
    assert(inode);

    task_t *task = running_task();

    mode &= (0777 & ~task->umask);
    mode |= IFREG;

    minix_inode_t *minode = (minix_inode_t *)inode->desc;

    minode->mode = inode->mode = mode;

makeup:
    if (!dir->op->permission(inode, ACC_MODE(flags & O_ACCMODE)))
        goto rollback;

    if (ISDIR(inode->mode) && ((flags & O_ACCMODE) != O_RDONLY))
        goto rollback;

    inode->atime = time();

    if (flags & O_TRUNC)
        inode->op->truncate(inode);

    brelse(buf);
    *result = inode;
    return EOK;

rollback:
    brelse(buf);
    iput(inode);
    return ret;
}

// 获取 dir 目录下 name 对应的 inode
static err_t minix_namei(inode_t *dir, char *name, char **next, inode_t **result)
{
    minix_dentry_t *entry = NULL;
    buffer_t *buf = find_entry(dir, name, next, &entry);
    if (!buf)
        return -ENOENT;

    inode_t *inode = iget(dir->dev, entry->nr);
    brelse(buf);

    *result = inode;
    return EOK;
}

// 创建目录
int minix_mkdir(inode_t *dir, char *name, int mode)
{
    minix_inode_t *dminode = (minix_inode_t *)dir->desc;
    assert(ISDIR(dminode->mode));

    // 父目录无写权限
    if (!minix_permission(dir, P_WRITE))
        return -EPERM;

    char *next;
    minix_dentry_t *entry;

    buffer_t *ebuf = NULL;
    buffer_t *zbuf = NULL;
    int ret;

    ebuf = find_entry(dir, name, &next, &entry);
    if (ebuf)
    {
        ret = -EEXIST;
        goto rollback;
    }

    ebuf = add_entry(dir, name, &entry);
    if (!ebuf)
    {
        ret = -EIO;
        goto rollback;
    }

    ebuf->dirty = true;
    idx_t idx = minix_ialloc(dir->super);
    entry->nr = idx;

    task_t *task = running_task();
    inode_t *inode = new_inode(dir->dev, entry->nr);
    assert(inode);

    minix_inode_t *iminode = (minix_inode_t *)inode->desc;
    inode->mode = iminode->mode = (mode & 0777 & ~task->umask) | IFDIR;
    inode->size = iminode->size = sizeof(minix_dentry_t) * 2; // 当前目录和父目录两个目录项
    iminode->nlinks = 2;                                      // 一个是 '.' 一个是 name

    // 父目录链接数加 1
    dir->buf->dirty = true;
    dminode->nlinks++; // ..

    // 写入 inode 目录中的默认目录项
    idx = minix_bmap(inode, 0, true);
    assert(idx);

    zbuf = bread(inode->dev, idx, BLOCK_SIZE);
    assert(zbuf);

    zbuf->dirty = true;

    entry = (minix_dentry_t *)zbuf->data;

    strcpy(entry->name, ".");
    entry->nr = inode->nr;

    entry++;
    strcpy(entry->name, "..");
    entry->nr = dir->nr;

    iput(inode);
    bwrite(dir->buf);

    ret = EOK;

rollback:
    brelse(ebuf);
    brelse(zbuf);
    return ret;
}

// 判断目录是否为空
static bool is_empty(inode_t *inode)
{
    minix_inode_t *minode = (minix_inode_t *)inode->desc;
    assert(ISDIR(minode->mode));

    int entries = minode->size / sizeof(minix_dentry_t);
    if (entries < 2 || !minode->zone[0])
    {
        LOGK("bad directory on dev %d\n", inode->super->dev);
        return false;
    }

    idx_t i = 0;
    idx_t block = 0;
    buffer_t *buf = NULL;
    minix_dentry_t *entry;
    int count = 0;

    for (; i < entries; i++, entry++)
    {
        if (!buf || (u32)entry >= (u32)buf->data + BLOCK_SIZE)
        {
            brelse(buf);
            block = minix_bmap(inode, i / BLOCK_DENTRIES, false);
            assert(block);

            buf = bread(inode->dev, block, BLOCK_SIZE);
            assert(buf);
            entry = (minix_dentry_t *)buf->data;
        }
        if (entry->nr)
            count++;
    };

    brelse(buf);

    if (count < 2)
    {
        LOGK("bad directory on dev %d\n", inode->dev);
        return false;
    }

    return count == 2;
}

// 删除目录
int minix_rmdir(inode_t *dir, char *name)
{
    minix_inode_t *dminode = (minix_inode_t *)dir->desc;
    assert(ISDIR(dminode->mode));

    // 父目录无写权限
    if (!minix_permission(dir, P_WRITE))
        return -EPERM;

    char *next = NULL;
    minix_dentry_t *entry;

    buffer_t *zbuf = NULL;
    buffer_t *ebuf = NULL;

    inode_t *inode = NULL;
    int ret = EOF;

    ebuf = find_entry(dir, name, &next, &entry);
    if (!ebuf)
    {
        ret = -ENOENT;
        goto rollback;
    }

    inode = iget(dir->dev, entry->nr);
    if (inode == dir)
    {
        ret = -EPERM;
        goto rollback;
    }

    minix_inode_t *iminode = (minix_inode_t *)inode->desc;
    if (!ISDIR(iminode->mode))
    {
        ret = -ENOTDIR;
        goto rollback;
    }

    task_t *task = running_task();
    if ((dminode->mode & ISVTX) && task->uid != iminode->uid)
    {
        ret = -EPERM;
        goto rollback;
    }

    if (dir->dev != inode->dev || inode->count > 1)
    {
        ret = -EPERM;
        goto rollback;
    }

    if (!is_empty(inode))
    {
        ret = -ENOTEMPTY;
        goto rollback;
    }

    assert(iminode->nlinks == 2);

    assert(minix_truncate(inode) == EOK);
    minix_ifree(inode->super, inode->nr);

    iminode->nlinks = 0;
    inode->buf->dirty = true;
    inode->nr = 0;

    dminode->nlinks--;
    dir->ctime = dir->atime = dminode->mtime = time();
    dir->buf->dirty = true;
    assert(dminode->nlinks > 0);

    entry->nr = 0;
    ebuf->dirty = true;
    ret = 0;

rollback:
    iput(inode);
    brelse(ebuf);
    return ret;
}

// 创建硬链接
int minix_link(inode_t *odir, char *oldname, inode_t *ndir, char *newname)
{
    int ret = EOF;
    buffer_t *buf = NULL;
    inode_t *dir = NULL;
    char *next = NULL;
    inode_t *inode = NULL;

    ret = minix_namei(odir, oldname, &next, &inode);
    if (ret < EOK)
        goto rollback;

    minix_inode_t *minode = (minix_inode_t *)inode->desc;
    if (ISDIR(minode->mode))
    {
        ret = -EPERM;
        goto rollback;
    }

    minix_inode_t *nminode = (minix_inode_t *)ndir->desc;
    if (!ISDIR(nminode->mode))
    {
        ret = -EPERM;
        goto rollback;
    }

    if (ndir->dev != inode->dev)
    {
        ret = -EXDEV;
        goto rollback;
    }

    if (!minix_permission(ndir, P_WRITE))
    {
        ret = -EACCES;
        goto rollback;
    }

    minix_dentry_t *entry;
    buf = find_entry(ndir, newname, &next, &entry);
    if (buf) // 目录项存在
    {
        ret = -EEXIST;
        goto rollback;
    }

    buf = add_entry(ndir, newname, &entry);
    if (!buf)
    {
        ret = -EIO;
        goto rollback;
        /* code */
    }

    entry->nr = inode->nr;
    buf->dirty = true;

    minode->nlinks++;
    inode->ctime = time();
    inode->buf->dirty = true;
    ret = EOK;

rollback:
    brelse(buf);
    iput(inode);
    return ret;
}

// 删除文件
int minix_unlink(inode_t *dir, char *name)
{
    int ret = EOF;
    char *next = NULL;
    inode_t *inode = NULL;
    buffer_t *buf = NULL;

    if (!minix_permission(dir, P_WRITE))
    {
        ret = -EPERM;
        goto rollback;
    }

    minix_dentry_t *entry;
    buf = find_entry(dir, name, &next, &entry);
    if (!buf) // 目录项不存在
    {
        ret = -ENOENT;
        goto rollback;
    }

    inode = iget(dir->dev, entry->nr);
    assert(inode);

    minix_inode_t *minode = inode->desc;

    if (ISDIR(minode->mode))
    {
        ret = -EPERM;
        goto rollback;
    }

    task_t *task = running_task();
    if ((minode->mode & ISVTX) && task->uid != minode->uid)
    {
        ret = -EPERM;
        goto rollback;
    }

    if (!minode->nlinks)
    {
        LOGK("deleting non exists file (%04x:%d)\n",
             inode->super->count, inode->nr);
    }

    entry->nr = 0;
    buf->dirty = true;

    minode->nlinks--;
    inode->buf->dirty = true;

    if (minode->nlinks == 0)
    {
        minix_truncate(inode);
        minix_ifree(inode->super, inode->nr);
    }
    ret = 0;

rollback:
    brelse(buf);
    iput(inode);
    return ret;
}

int minix_mknod(inode_t *dir, char *name, int mode, int dev)
{
    char *next = NULL;
    buffer_t *buf = NULL;
    inode_t *inode = NULL;
    int ret = -ERROR;

    if (!minix_permission(dir, P_WRITE))
    {
        ret = -EPERM;
        goto rollback;
    }

    minix_dentry_t *entry;
    buf = find_entry(dir, name, &next, &entry);
    if (buf)
    {
        ret = -EEXIST;
        goto rollback;
    }

    buf = add_entry(dir, name, &entry);
    if (!buf)
    {
        ret = -EIO;
        goto rollback;
    }

    buf->dirty = true;
    idx_t idx = minix_ialloc(dir->super);
    entry->nr = idx;

    inode = new_inode(dir->dev, entry->nr);
    assert(inode);

    minix_inode_t *minode = inode->desc;

    minode->mode = mode;
    if (ISBLK(mode) || ISCHR(mode) || ISSOCK(mode))
        minode->zone[0] = dev;

    ret = 0;

rollback:
    brelse(buf);
    iput(inode);
    return ret;
}

static int minix_super(dev_t dev, super_t *super)
{
    // 读取超级块
    buffer_t *buf = bread(dev, 1, BLOCK_SIZE);
    if (!buf)
        return -EFSUNK;

    assert(buf);
    minix_super_t *desc = (minix_super_t *)buf->data;

    if (desc->magic != MINIX1_MAGIC)
    {
        brelse(buf);
        return -EFSUNK;
    }

    super->buf = buf;
    super->desc = desc;
    super->dev = dev;
    super->type = FS_TYPE_MINIX;
    super->block_size = BLOCK_SIZE;
    super->sector_size = SECTOR_SIZE;
    super->iroot = iget(dev, 1);

    return EOK;
}

int minix_mkfs(dev_t dev, int icount)
{
    super_t *super = NULL;
    buffer_t *buf = NULL;
    int ret = EOF;

    int total_block = device_ioctl(dev, DEV_CMD_SECTOR_COUNT, NULL, 0) / BLOCK_SECS;
    assert(total_block);
    assert(icount < total_block);
    if (!icount)
    {
        icount = total_block / 3;
    }

    super = get_free_super();
    super->type = FS_TYPE_MINIX;
    super->dev = dev;
    super->count++;

    buf = bread(dev, 1, BLOCK_SIZE);
    super->buf = buf;
    buf->dirty = true;

    // 初始化超级块
    minix_super_t *desc = (minix_super_t *)buf->data;
    super->desc = desc;

    int inode_blocks = div_round_up(icount * sizeof(minix_inode_t), BLOCK_SIZE);
    desc->inodes = icount;
    desc->zones = total_block;
    desc->imap_blocks = div_round_up(icount, BLOCK_BITS);

    int zcount = total_block - desc->imap_blocks - inode_blocks - 2;
    desc->zmap_blocks = div_round_up(zcount, BLOCK_BITS);

    desc->firstdatazone = 2 + desc->imap_blocks + desc->zmap_blocks + inode_blocks;
    desc->log_zone_size = 0;
    desc->max_size = BLOCK_SIZE * TOTAL_BLOCK;
    desc->magic = MINIX1_MAGIC;

    int idx = 2;
    for (int i = 0; i < (desc->imap_blocks + desc->zmap_blocks); i++, idx++)
    {
        buf = bread(dev, idx, BLOCK_SIZE);
        assert(buf);
        buf->dirty = true;
        memset(buf->data, 0, BLOCK_SIZE);
        brelse(buf);
    }

    // 初始化位图
    idx = minix_balloc(super);

    idx = minix_ialloc(super);
    idx = minix_ialloc(super);

    // 位图尾部置位 TODO:

    // 创建根目录
    task_t *task = running_task();

    inode_t *iroot = new_inode(dev, 1);
    super->iroot = iroot;

    minix_inode_t *minode = (minix_inode_t *)iroot->desc;

    minode->mode = (0777 & ~task->umask) | IFDIR;
    minode->size = sizeof(minix_dentry_t) * 2; // 当前目录和父目录两个目录项
    minode->nlinks = 2;                        // 一个是 '.' 一个是 name

    buf = bread(dev, minix_bmap(iroot, 0, true), BLOCK_SIZE);
    buf->dirty = true;

    minix_dentry_t *entry = (minix_dentry_t *)buf->data;
    memset(entry, 0, BLOCK_SIZE);

    strcpy(entry->name, ".");
    entry->nr = iroot->nr;

    entry++;
    strcpy(entry->name, "..");
    entry->nr = iroot->nr;

    brelse(buf);

    ret = 0;
rollback:
    put_super(super);
    return ret;
}

int minix_ioctl(inode_t *inode, int cmd, void *args)
{
    // 文件必须是某种设备
    int mode = inode->mode;
    if (!ISCHR(mode) && !ISBLK(mode) && !ISSOCK(mode))
        return -EINVAL;

    // 得到设备号
    dev_t dev = inode->rdev;
    if (dev >= DEVICE_NR)
        return -ENOTTY;

    // 进行设备控制
    return device_ioctl(dev, cmd, args, 0);
}

static fs_op_t minix_op = {
    minix_mkfs,
    minix_super,

    minix_open,
    minix_close,

    minix_ioctl,
    minix_read,
    minix_write,
    minix_truncate,

    minix_stat,
    minix_permission,

    minix_namei,
    minix_mkdir,
    minix_rmdir,
    minix_link,
    minix_unlink,
    minix_mknod,
    minix_readdir,
};

void minix_init()
{
    fs_register_op(FS_TYPE_MINIX, &minix_op);
}
