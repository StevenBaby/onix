#include <onix/fs.h>
#include <onix/buffer.h>
#include <onix/assert.h>
#include <onix/string.h>
#include <onix/arena.h>
#include <onix/stdlib.h>
#include <onix/task.h>
#include <onix/syscall.h>
#include <onix/device.h>
#include <onix/debug.h>

#include "fat.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SECTOR_SIZE 512 // 扇区大小

#define FREE_CLUSTER 0x0000
#define BAD_CLUSTER 0xFFF7
#define END_CLUSTER 0xFFF8 // > 0xFFF8
#define END_FILE_CLUSTER 0xFFFF

#define END_ENTRY 0x00
#define FREE_ENTRY 0xE5
#define LAST_LONG_ENTRY 0x40

#define GOOD_CLUSTER(cluster) (cluster >= 3 && cluster < END_CLUSTER)
#define VALID_CLUSTER(cluster) (cluster >= 0 && cluster < END_CLUSTER)

static err_t fat16_read_next_cluster(super_t *super, inode_t *inode, idx_t cluster)
{
    fat_root_entry_t *root = (fat_root_entry_t *)super->iroot->desc;

    if (inode == super->iroot)
    {
        cluster += 1;
        if (cluster == root->data_cluster)
            return END_FILE_CLUSTER;
        return cluster;
    }

    fat_super_t *desc = (fat_super_t *)super->desc;

    u32 start = cluster * 2;                                   // 开始字节数
    u32 offset = start % super->block_size;                    // 高速缓冲块中偏移
    idx_t idx = start / super->block_size + root->fat_cluster; // FAT 高速缓冲索引
    u32 limit = root->fat_cluster + root->fat_cluster_size;    // 高速缓冲最大索引
    if (idx >= limit)
        return -ENOSPC;

    buffer_t *buf = bread(super->dev, idx, super->block_size);
    u16 *data = (u16 *)(buf->data + offset);
    u16 next = *data;
    brelse(buf);
    return next;
}

static err_t fat16_write_next_cluster(super_t *super, idx_t cluster, u16 value)
{
    fat_root_entry_t *root = (fat_root_entry_t *)super->iroot->desc;
    fat_super_t *desc = (fat_super_t *)super->desc;
    assert(desc->fat_count == 2);

    u32 start = cluster * 2;                                   // 开始字节数
    u32 offset = start % super->block_size;                    // 高速缓冲块中偏移
    idx_t idx = start / super->block_size + root->fat_cluster; // FAT 高速缓冲索引
    u32 limit = root->fat_cluster + root->fat_cluster_size;    // 高速缓冲最大索引
    u32 first = true;
    if (idx >= limit)
        return -ENOSPC;

again:
    buffer_t *buf = bread(super->dev, idx, super->block_size);

    u16 *data = (u16 *)(buf->data + offset);
    *data = value;

    buf->dirty = true;
    brelse(buf);

    if (!first)
        return EOK;

    first = false;
    idx += root->fat_cluster_size;   // FAT 高速缓冲索引
    limit += root->fat_cluster_size; // 高速缓冲最大索引
    goto again;
}

static _inline buffer_t *fat16_bread(inode_t *inode, int cluster, idx_t *next)
{
    fat_root_entry_t *root = inode->super->iroot->desc;
    idx_t idx;
    if (inode == inode->super->iroot)
        idx = cluster + root->root_cluster;
    else
        idx = cluster + root->cluster_offset;

    buffer_t *buf = bread(inode->dev, idx, inode->super->block_size);
    *next = fat16_read_next_cluster(inode->super, inode, cluster);
    return buf;
}

static buffer_t *fat16_find_entry(inode_t *dir, char *name, char **next, fat_entry_t **result)
{
    fat_super_t *desc = (fat_super_t *)dir->super->desc;
    fat_entry_t *entry = (fat_entry_t *)dir->desc;
    u32 cluster = entry->cluster;

    buffer_t *buf0 = NULL;
    buffer_t *buf = NULL;
    buffer_t *ret = NULL;

    char entry_name[MAXNAMELEN];

    fat_lentry_t **entries = (fat_lentry_t **)kmalloc(sizeof(void *) * 20);
    int index = 0;
    int length = 0;

    for (size_t i = 0; true; i++, entry++)
    {
        if (!buf || (u32)entry >= (u32)buf->data + dir->super->block_size)
        {
            if (!VALID_CLUSTER(cluster))
                break;

            brelse(buf0);
            buf0 = buf;
            buf = fat16_bread(dir, cluster, &cluster);
            entry = (fat_entry_t *)buf->data;
        }
        if (entry->name[0] == FREE_ENTRY)
            continue;
        if (entry->name[0] == END_ENTRY)
            break;
        if (entry->attrs == fat_long_filename)
        {
            if (entry->name[0] & LAST_LONG_ENTRY)
            {
                index = 0;
            }
            entries[index++] = entry;
            continue;
        }
        u8 chksum = fat_chksum(entry->name);
        length = fat_format_long_name(entry_name, entries, index - 1, chksum);
        if (length > 0 && match_name(name, entry_name, next, MAXNAMELEN - 1))
        {
            *result = entry;
            ret = buf;
            buf = NULL;
            break;
        }
        fat_format_short_name(entry_name, entry);
        if (fat_match_short_name(name, entry_name, next, 12))
        {
            *result = entry;
            ret = buf;
            buf = NULL;
            break;
        }
        index = 0;
    }
    kfree(entries);
    brelse(buf0);
    brelse(buf);
    return ret;
}

static u32 fat16_find_free_cluster(super_t *super)
{
    fat_root_entry_t *root = super->iroot->desc;
    idx_t idx = root->fat_cluster;
    idx_t cluster = 3;
    idx_t offset = 3;
    u32 clusters = idx + root->fat_cluster_size;

    buffer_t *buf = NULL;
    while (idx < clusters)
    {
        brelse(buf);
        buf = bread(super->dev, idx++, super->block_size);
        u16 *ptr = (u16 *)buf->data + offset;
        while ((u32)ptr < (u32)buf->data + super->block_size)
        {
            u16 entry = *(u16 *)ptr;
            if (entry == FREE_CLUSTER)
                return cluster;
            ptr++;
            cluster++;
        }
        offset = 0;
    }
    brelse(buf);
    return 0;
}

static buffer_t *fat16_add_entry(inode_t *dir, char *name, fat_entry_t **result)
{
    int namelen;
    char *ext = NULL;
    if ((namelen = validate_short_name(name, &ext)) < EOK)
        return NULL;

    char *next = NULL;
    buffer_t *buf = fat16_find_entry(dir, name, &next, result);
    if (buf)
        return buf;

    // 需要连续的目录数量
    int total = div_round_up(namelen, 13) + 1;
    int count = 0;
    fat_lentry_t **entries = (fat_lentry_t **)kmalloc(sizeof(void *) * total);

    u16 cdate;
    u16 ctime;
    fat_parse_time(time(), &cdate, &ctime);

    fat_super_t *desc = (fat_super_t *)dir->super->desc;
    fat_entry_t *entry = (fat_entry_t *)dir->desc;
    u32 cluster = entry->cluster;
    bool new_cluster = false;

    buffer_t *buf0 = NULL;
    buffer_t *ret = NULL;

    for (size_t i = 0; true; i++, entry++)
    {
        if (cluster >= END_CLUSTER)
        {
            cluster = fat16_find_free_cluster(dir->super);
            assert(GOOD_CLUSTER(cluster));
            assert(fat16_write_next_cluster(dir->super, cluster, END_FILE_CLUSTER) == EOK);
            new_cluster = true;
        }
        if (!buf || (u32)entry >= (u32)buf->data + dir->super->block_size)
        {

            if (!VALID_CLUSTER(cluster))
                break;

            brelse(buf0);
            buf0 = buf;
            brelse(buf);
            buf = fat16_bread(dir, cluster, &cluster);
            if (new_cluster)
            {
                memset(buf->data, 0, dir->super->block_size);
                new_cluster = false;
            }
            entry = (fat_entry_t *)buf->data;
        }
        if (entry->name[0] != END_ENTRY && entry->name[0] != FREE_ENTRY)
        {
            count = 0;
            continue;
        }
        entries[count++] = entry;
        if (count < total)
            continue;

        assert(fat_create_short_name(dir, entry, name, ext, namelen) == EOK);
        u32 chksum = fat_chksum(entry->name);
        fat_parse_long_name(name, entries, count - 2, chksum);

        buf->dirty = true;
        dir->mtime = time();
        entry->cdate = cdate;
        entry->mdate = cdate;
        entry->last_accessed_date = cdate;
        entry->mtime = ctime;
        entry->ctime = ctime;
        *result = entry;
        ret = buf;
        buf = NULL;
        break;
    }
    if (buf0)
        buf0->dirty = true;

    kfree(entries);
    brelse(buf);
    brelse(buf0);
    return ret;
}

static err_t fat16_remove_entry(inode_t *dir, char *name)
{
    fat_super_t *desc = (fat_super_t *)dir->super->desc;
    fat_entry_t *entry = (fat_entry_t *)dir->desc;
    u32 cluster = entry->cluster;

    buffer_t *buf0 = NULL;
    buffer_t *buf = NULL;
    err_t ret = EOF;

    char entry_name[MAXNAMELEN];

    fat_lentry_t **entries = (fat_lentry_t **)kmalloc(sizeof(void *) * 20);
    int index = 0;
    int length = 0;
    char *next;

    for (size_t i = 0; true; i++, entry++)
    {
        if (!buf || (u32)entry >= (u32)buf->data + dir->super->block_size)
        {
            if (!VALID_CLUSTER(cluster))
                break;

            brelse(buf0);
            buf0 = buf;
            buf = fat16_bread(dir, cluster, &cluster);
            entry = (fat_entry_t *)buf->data;
        }
        if (entry->name[0] == FREE_ENTRY)
            continue;
        if (entry->name[0] == END_ENTRY)
            goto rollback;

        if (entry->attrs == fat_long_filename)
        {
            if (entry->name[0] & LAST_LONG_ENTRY)
            {
                index = 0;
            }
            entries[index++] = entry;
            continue;
        }
        u8 chksum = fat_chksum(entry->name);
        length = fat_format_long_name(entry_name, entries, index - 1, chksum);
        if (length > 0 && match_name(name, entry_name, &next, MAXNAMELEN - 1))
            goto setfree;
        fat_format_short_name(entry_name, entry);
        if (fat_match_short_name(name, entry_name, next, 12))
            goto setfree;
        index = 0;
    }
setfree:
    for (int i = index - 1; i >= 0; i--)
    {
        memset(entries[i], 0, sizeof(fat_entry_t));
        entries[i]->entry_index = 0;
    }
    if (buf0)
        buf0->dirty = true;
    buf->dirty = true;
    memset(entry, 0, sizeof(fat_entry_t));
    entry->name[0] = FREE_ENTRY;
    ret = EOK;
rollback:
    kfree(entries);
    brelse(buf0);
    brelse(buf);
    return ret;
}

// ========================================================================

static err_t fat16_namei(inode_t *dir, char *name, char **next, inode_t **result)
{
    assert(dir->super->type == FS_TYPE_FAT16);
    fat_entry_t *entry = NULL;

    buffer_t *buf = fat16_find_entry(dir, name, next, &entry);
    if (!buf)
        return -ENOENT;

    inode_t *inode = fat_iget(dir->super, entry, buf);
    *result = inode;
    brelse(buf);
    return EOK;
}

// 打开 dir 目录的 name 文件
int fat16_open(inode_t *dir, char *name, int flags, int mode, inode_t **result)
{
    if ((!memcmp(name, ".", 1) || !memcmp(name, "..", 2)) && dir == dir->super->iroot)
    {
        dir->count++;
        *result = dir;
        return EOK;
    }

    inode_t *inode = NULL;
    buffer_t *buf = NULL;
    fat_entry_t *entry = NULL;
    char *next = NULL;
    int ret = EOF;

    if ((flags & O_TRUNC) && ((flags & O_ACCMODE) == O_RDONLY))
        flags |= O_RDWR;

    ret = fat16_namei(dir, name, &next, &inode);
    if (ret == EOK)
        goto makeup;

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

    buf = fat16_add_entry(dir, name, &entry);
    if (!buf)
    {
        ret = -EIO;
        goto rollback;
    }

    u16 d, t;
    fat_parse_time(time(), &d, &t);

    buf->dirty = true;
    entry->cluster = END_FILE_CLUSTER;
    entry->filesize = 0;
    entry->attrs = 0;
    entry->mdate = d;
    entry->mtime = t;
    entry->cdate = d;
    entry->ctime = t;
    entry->last_accessed_date = d;

    inode = fat_iget(dir->super, entry, buf);

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

// 从 offset 处，读 size 个字节到 data
int fat16_read(inode_t *inode, char *data, int size, off_t offset)
{
    assert(inode->type == FS_TYPE_FAT16);

    fat_entry_t *entry = (fat_entry_t *)inode->desc;

    // 如果偏移量超过文件大小，返回 EOF
    if (!(entry->attrs & fat_directory) && offset >= entry->filesize)
        return EOF;

    fat_super_t *desc = (fat_super_t *)inode->super->desc;
    size_t block_size = inode->super->block_size;

    // 开始读取的位置
    u32 begin = 0;

    idx_t cluster = entry->cluster;
    while (begin + block_size < offset && VALID_CLUSTER(cluster))
    {
        cluster = fat16_read_next_cluster(inode->super, inode, cluster);
        begin += block_size;
    }

    if (!VALID_CLUSTER(cluster))
        return EOF;

    begin = offset;

    u32 filesize = (entry->attrs & fat_directory) ? 0xFFFFFFFF : entry->filesize;

    // 剩余字节数
    u32 left = MIN(size, filesize - offset);

    int ret = EOF;
    buffer_t *buf = NULL;

    while (left && VALID_CLUSTER(cluster))
    {
        // 读取文件块缓冲
        buf = fat16_bread(inode, cluster, &cluster);

        // 文件块中的偏移量
        u32 start = offset % block_size;

        // 本次需要读取的字节数
        u32 chars = MIN(block_size - start, left);

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
        assert(brelse(buf) == EOK);
    }

    // 更新访问时间
    inode->atime = time();

    // 返回读取数量
    return offset - begin;

rollback:
    assert(brelse(buf) == EOK);
    return ret;
}

int fat16_readdir(inode_t *inode, dentry_t *result, size_t count, off_t offset)
{

    assert(ISDIR(inode->mode));
    int ret = EOF;

    assert(inode->type == FS_TYPE_FAT16);

    fat_entry_t *entry = (fat_entry_t *)inode->desc;
    fat_super_t *desc = (fat_super_t *)inode->super->desc;
    size_t block_size = inode->super->block_size;

    // 开始读取的位置
    u32 begin = 0;

    idx_t cluster = entry->cluster;
    while (begin + block_size < offset && VALID_CLUSTER(cluster))
    {
        cluster = fat16_read_next_cluster(inode->super, inode, cluster);
        begin += block_size;
    }

    if (!VALID_CLUSTER(cluster))
        return EOF;

    begin = offset;

    // 剩余字节数
    u32 left = 0xFFFFFFFF;

    buffer_t *buf0 = NULL;
    buffer_t *buf = NULL;

    fat_lentry_t **entries = (fat_lentry_t **)kmalloc(sizeof(void *) * 20);

    int index = 0;
    int length = 0;

    for (size_t i = 0; true; i++, entry++)
    {
        if (!buf || (u32)entry >= (u32)buf->data + inode->super->block_size)
        {
            if (!VALID_CLUSTER(cluster))
                break;

            brelse(buf0);
            buf0 = buf;
            buf = fat16_bread(inode, cluster, &cluster);
            // 文件块中的偏移量
            u32 start = offset % block_size;
            entry = (fat_entry_t *)(buf->data + start);
        }
        if (entry->name[0] == END_ENTRY)
        {
            result->name[0] = 0;
            result->nr = 0;
            result->length = 0;
            result->namelen = 0;
            ret = EOF;
            goto rollback;
        }
        offset += sizeof(fat_lentry_t);
        if (entry->name[0] == FREE_ENTRY)
            continue;

        if (entry->attrs == fat_long_filename)
        {
            if (entry->name[0] & LAST_LONG_ENTRY)
            {
                index = 0;
            }
            entries[index++] = entry;
            continue;
        }
        result->length = sizeof(fat_lentry_t);
        result->nr = entry->cluster;

        u8 chksum = fat_chksum(entry->name);
        result->length = fat_format_long_name(result->name, entries, index - 1, chksum);
        if (result->length == 0)
            result->namelen = fat_format_short_name(result->name, entry);
        break;
    }
    ret = (offset - begin);
rollback:
    kfree(entries);
    brelse(buf0);
    brelse(buf);
    return ret;
}

// 从 inode 的 offset 处，将 data 的 size 个字节写入磁盘
int fat16_write(inode_t *inode, char *data, int size, off_t offset)
{
    assert(inode->type == FS_TYPE_FAT16);
    assert(ISFILE(inode->mode));

    fat_entry_t *entry = (fat_entry_t *)inode->desc;
    assert(entry);
    if (entry->attrs & fat_readonly)
        return -EPERM;

    fat_super_t *desc = (fat_super_t *)inode->super->desc;
    size_t block_size = inode->super->block_size;

    // 开始读取的位置
    u32 begin = 0;

    idx_t cluster = entry->cluster;

    if (!VALID_CLUSTER(cluster))
    {
        cluster = fat16_find_free_cluster(inode->super);
        assert(GOOD_CLUSTER(cluster));
        entry->cluster = cluster;
        inode->buf->dirty = true;
        fat16_write_next_cluster(inode->super, cluster, END_FILE_CLUSTER);
    }

    while (begin + block_size < offset)
    {
        cluster = fat16_read_next_cluster(inode->super, inode, cluster);
        begin += block_size;
        if (GOOD_CLUSTER(cluster))
            continue;

        cluster = fat16_find_free_cluster(inode->super);
        assert(GOOD_CLUSTER(cluster));
        fat16_write_next_cluster(inode->super, cluster, END_FILE_CLUSTER);
    }

    assert(GOOD_CLUSTER(cluster));

    begin = offset;

    // 剩余字节数
    u32 left = size;
    buffer_t *buf = NULL;

    while (left)
    {
        if (!VALID_CLUSTER(cluster))
        {
            cluster = fat16_find_free_cluster(inode->super);
            assert(GOOD_CLUSTER(cluster));
            fat16_write_next_cluster(inode->super, cluster, END_FILE_CLUSTER);
        }

        // 读取文件块缓冲
        buf = fat16_bread(inode, cluster, &cluster);
        buf->dirty = true;

        // 文件块中的偏移量
        u32 start = offset % block_size;

        // 文件块中的指针
        char *ptr = buf->data + start;

        // 本次需要写入的字节数
        u32 chars = MIN(block_size - start, left);

        // 更新 偏移量 和 剩余字节数
        offset += chars;
        left -= chars;

        if (offset > entry->filesize)
        {
            inode->size = entry->filesize = offset;
            inode->buf->dirty = true;
        }

        // 拷贝内容
        memcpy(ptr, data, chars);

        // 更新缓存位置
        data += chars;

        // 释放文件块缓冲
        assert(brelse(buf) == EOK);
    }

    // 更新修改时间
    inode->mtime = time();

    u16 t;
    u16 d;
    fat_parse_time(inode->mtime, &d, &t);

    entry->last_accessed_date = d;
    entry->mdate = d;
    entry->mtime = t;

    bwrite(inode->buf);

    // 返回写入的大小
    return offset - begin;
}

// 释放 inode 所有文件块
static int fat16_truncate(inode_t *inode)
{
    if (!ISFILE(inode->mode) && !ISDIR(inode->mode))
        return -EPERM;

    assert(inode != inode->super->iroot);
    fat_entry_t *entry = (fat_entry_t *)inode->desc;
    u16 before = entry->cluster;

    while (GOOD_CLUSTER(before))
    {
        u16 after = fat16_read_next_cluster(inode->super, inode, before);
        assert(fat16_write_next_cluster(inode->super, before, FREE_CLUSTER) == EOK);
        before = after;
    }

    entry->filesize = 0;
    entry->cluster = END_FILE_CLUSTER;
    inode->buf->dirty = true;

    u16 d, t;
    fat_parse_time(time(), &d, &t);
    entry->mdate = d;
    entry->mtime = t;
    entry->last_accessed_date = d;

    bwrite(inode->buf);
    return EOK;
}

// 创建目录
int fat16_mkdir(inode_t *dir, char *name, int mode)
{
    assert(dir->type == FS_TYPE_FAT16);
    assert(ISDIR(dir->mode));

    char *next;
    fat_entry_t *entry;

    fat_super_t *desc = (fat_super_t *)dir->super->desc;
    fat_entry_t *dir_entry = (fat_entry_t *)dir->desc;

    buffer_t *ebuf = NULL;
    buffer_t *zbuf = NULL;
    int ret;

    ebuf = fat16_find_entry(dir, name, &next, &entry);
    if (ebuf)
    {
        ret = -EEXIST;
        goto rollback;
    }

    ebuf = fat16_add_entry(dir, name, &entry);
    if (!ebuf)
    {
        ret = -EIO;
        goto rollback;
    }

    ebuf->dirty = true;
    idx_t idx = fat16_find_free_cluster(dir->super);
    if (!GOOD_CLUSTER(idx))
    {
        ret = -EIO;
        goto rollback;
    }

    if ((ret = fat16_write_next_cluster(dir->super, idx, END_CLUSTER)) < EOK)
    {
        ret = -EIO;
        goto rollback;
    }

    entry->attrs = fat_directory;
    entry->cluster = idx;
    entry->filesize = 0;
    task_t *task = running_task();

    fat_root_entry_t *root = dir->super->iroot->desc;
    zbuf = bread(dir->dev, idx + root->cluster_offset, dir->super->block_size);
    zbuf->dirty = true;

    entry = (fat_entry_t *)zbuf->data;
    memcpy(entry->name, ".       ", 8);
    memcpy(entry->ext, "   ", 3);
    entry->cluster = idx;
    entry->attrs = fat_directory;
    entry->filesize = 0;

    entry++;

    memcpy(entry->name, "..      ", 8);
    memcpy(entry->ext, "   ", 3);
    entry->cluster = dir_entry->cluster;
    entry->attrs = fat_directory;
    entry->filesize = 0;

    entry++;
    memset(entry, 0, sizeof(fat_entry_t));

    ret = EOK;

rollback:
    brelse(ebuf);
    brelse(zbuf);
    return ret;
}

static bool fat16_is_empty(inode_t *dir)
{
    assert(ISDIR(dir->mode));
    assert(dir != dir->super->iroot);

    fat_super_t *desc = (fat_super_t *)dir->super->desc;
    fat_entry_t *entry = (fat_entry_t *)dir->desc;
    u32 cluster = entry->cluster;

    buffer_t *buf = NULL;
    int count = 0;
    for (size_t i = 0; true; i++, entry++)
    {
        if (!buf || (u32)entry >= (u32)buf->data + dir->super->block_size)
        {
            if (!VALID_CLUSTER(cluster))
                break;
            brelse(buf);
            buf = fat16_bread(dir, cluster, &cluster);
            entry = (fat_entry_t *)buf->data;
        }
        if (entry->attrs == fat_long_filename)
            continue;
        if (entry->name[0] == FREE_ENTRY)
            continue;
        if (entry->name[0] == END_ENTRY)
            break;
        if (!memcmp(entry->name, ".", 1) || !memcmp(entry->name, "..", 2))
        {
            count++;
            continue;
        }
        return false;
    }
    if (count != 2)
    {
        LOGK("bad directory on dev %d\n", dir->dev);
    }
    return true;
}

// 删除目录
int fat16_rmdir(inode_t *dir, char *name)
{
    assert(ISDIR(dir->mode));

    // 父目录无写权限
    if (!fat_permission(dir, P_WRITE))
        return -EPERM;

    char *next = NULL;
    fat_entry_t *entry;

    buffer_t *buf = NULL;

    inode_t *inode = NULL;
    int ret = EOF;

    buf = fat16_find_entry(dir, name, &next, &entry);
    if (!buf)
    {
        ret = -ENOENT;
        goto rollback;
    }

    inode = fat_iget(dir->super, entry, buf);
    if (inode == dir)
    {
        ret = -EPERM;
        goto rollback;
    }

    entry = (fat_entry_t *)inode->desc;
    if (!ISDIR(inode->mode))
    {
        ret = -ENOTDIR;
        goto rollback;
    }

    if (dir->dev != inode->dev || inode->count > 1)
    {
        ret = -EPERM;
        goto rollback;
    }

    if (!fat16_is_empty(inode))
    {
        ret = -ENOTEMPTY;
        goto rollback;
    }

    assert(fat16_truncate(inode) == EOK);
    assert(fat16_remove_entry(dir, name) == EOK);
    ret = EOK;

rollback:
    iput(inode);
    brelse(buf);
    return ret;
}

// 删除文件
int fat16_unlink(inode_t *dir, char *name)
{
    int ret = EOF;
    char *next = NULL;
    inode_t *inode = NULL;
    buffer_t *buf = NULL;

    if (!fat_permission(dir, P_WRITE))
    {
        ret = -EPERM;
        goto rollback;
    }

    fat_entry_t *entry;
    buf = fat16_find_entry(dir, name, &next, &entry);
    if (!buf) // 目录项不存在
    {
        ret = -ENOENT;
        goto rollback;
    }

    inode = fat_iget(dir->super, entry, buf);
    assert(inode);

    if (ISDIR(inode->mode))
    {
        ret = -EPERM;
        goto rollback;
    }

    fat16_truncate(inode);
    assert(fat16_remove_entry(dir, name) == EOK);
    ret = EOK;

rollback:
    iput(inode);
    brelse(buf);
    return ret;
}

static int fat16_super(dev_t dev, super_t *super)
{
    // 读取超级块
    buffer_t *buf = bread(dev, 0, SECTOR_SIZE * 2);
    int ret = -EFSUNK;
    if (!buf)
        goto rollback;

    fat_super_t *desc = (fat_super_t *)buf->data;
    if (memcmp(desc->identifier, "FAT16   ", 8))
        goto rollback;

    u32 block_size = desc->bytes_per_sector * desc->sectors_per_cluster;
    if (!bvalid(block_size))
    {
        ret = -ESUPPORT;
        goto rollback;
    }

    LOGK("fat oem %s\n", desc->oem);
    LOGK("fat bytes_per_sector %d\n", desc->bytes_per_sector);
    LOGK("fat sectors_per_cluster %d\n", desc->sectors_per_cluster);
    LOGK("fat reserved_sector_count %d\n", desc->reserved_sector_count);
    LOGK("fat fat_count %d\n", desc->fat_count);
    LOGK("fat root_directory_entry_count %d\n", desc->root_directory_entry_count);
    LOGK("fat sector_count16 %d\n", desc->sector_count16);
    LOGK("fat media_descriptor_type %d\n", desc->media_descriptor_type);
    LOGK("fat sectors_per_fat %d\n", desc->sectors_per_fat);
    LOGK("fat sectors_per_track %d\n", desc->sectors_per_track);
    LOGK("fat head_count %d\n", desc->head_count);
    LOGK("fat hidden_sector_count %d\n", desc->hidden_sector_count);
    LOGK("fat sector_count %d\n", desc->sector_count);

    LOGK("fat drive_number %d\n", desc->drive_number);
    LOGK("fat nt_flags %d\n", desc->nt_flags);
    LOGK("fat signature %#x\n", desc->signature);
    LOGK("fat volume_id %#x\n", desc->volume_id);

    super->dev = dev;
    super->buf = buf;
    super->desc = desc;
    super->type = FS_TYPE_FAT16;
    super->sector_size = desc->bytes_per_sector;
    super->block_size = block_size;

    fat_root_entry_t *root = kmalloc(sizeof(fat_root_entry_t));

    root->fat_start_sector = desc->reserved_sector_count;
    root->root_start_sector = root->fat_start_sector + desc->fat_count * desc->sectors_per_fat;
    root->data_start_sector = root->root_start_sector;
    root->data_start_sector += desc->root_directory_entry_count * sizeof(fat_entry_t) / desc->bytes_per_sector;

    assert(root->fat_start_sector % desc->sectors_per_cluster == 0);
    assert(root->root_start_sector % desc->sectors_per_cluster == 0);
    assert(root->data_start_sector % desc->sectors_per_cluster == 0);

    u32 sector_count = MAX(desc->sector_count, desc->sector_count16);
    root->data_cluster = root->data_start_sector / desc->sectors_per_cluster;
    u32 data_sectors = sector_count - root->data_start_sector;
    root->data_cluster_size = data_sectors / desc->sectors_per_cluster;

    root->root_cluster = root->root_start_sector / desc->sectors_per_cluster;
    root->root_cluster_size = root->data_cluster - root->root_cluster;

    root->fat_cluster = root->fat_start_sector / desc->sectors_per_cluster;
    root->fat_cluster_size = desc->sectors_per_fat / desc->sectors_per_cluster;

    root->cluster_offset = root->data_cluster - 2;

    fat_entry_t *entry = (fat_entry_t *)root;

    entry->filesize = 0;
    entry->attrs = fat_directory;
    entry->cluster = 0;
    super->iroot = fat_make_inode(super, entry, NULL);
    super->iroot->desc = root;

    return EOK;

rollback:
    brelse(buf);
    return -EFSUNK;
}

err_t fat16_mkfs(dev_t dev, int cluster)
{
    u32 sectors = device_ioctl(dev, DEV_CMD_SECTOR_COUNT, NULL, 0);
    if (cluster <= 0)
        cluster = sectors / (0xFFF7 - 2);

    if (cluster > 8)
        return -EINVAL;

    if (cluster > 4)
        cluster = 8;
    else if (cluster > 2)
        cluster = 4;
    else
        cluster = 2;

    u32 clusters = sectors / cluster;
    if (clusters >= (0xFFF7 - 2))
        return -EINVAL;

    int block_size = cluster * SECTOR_SIZE;
    buffer_t *buf = bread(dev, 0, block_size);

    fat_super_t *desc = (fat_super_t *)buf->data;
    desc->bytes_per_sector = SECTOR_SIZE;
    desc->sectors_per_cluster = cluster;
    desc->reserved_sector_count = desc->sectors_per_cluster;

    if (sectors > 65535)
    {
        desc->sector_count = sectors;
        desc->sector_count16 = 0;
    }
    else
    {
        desc->sector_count = 0;
        desc->sector_count16 = sectors;
    }

    // read https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
    desc->fat_count = 2;
    desc->root_directory_entry_count = 512; // must be 512
    desc->media_descriptor_type = 0xF8;     // fixed media
    desc->sectors_per_fat = 16;             // todo
    desc->sectors_per_track = 32;           // todo
    desc->head_count = 2;                   // todo
    desc->hidden_sector_count = device_ioctl(dev, DEV_CMD_SECTOR_START, NULL, 0);

    u16 d, t;
    fat_parse_time(time(), &d, &t);

    desc->drive_number = 0x80; // from document
    desc->nt_flags = 0;
    desc->signature = 0x29;
    desc->volume_id = (u32)d << 16 | t; // from document
    memcpy(desc->volume_label_string, "ONIX FAT16 ", 11);
    memcpy(desc->identifier, "FAT16   ", 8);

    super_t *super = get_free_super();
    super->type = FS_TYPE_FAT16;
    super->dev = dev;
    super->count++;
    super->sector_size = SECTOR_SIZE;
    super->block_size = desc->bytes_per_sector * desc->sectors_per_cluster;
    super->desc = desc;
    super->buf = buf;
    buf->dirty = true;

    fat_root_entry_t *root = kmalloc(sizeof(fat_root_entry_t));

    root->fat_start_sector = desc->reserved_sector_count;
    root->root_start_sector = root->fat_start_sector + desc->fat_count * desc->sectors_per_fat;
    root->data_start_sector = root->root_start_sector;
    root->data_start_sector += desc->root_directory_entry_count * sizeof(fat_entry_t) / desc->bytes_per_sector;

    assert(root->fat_start_sector % desc->sectors_per_cluster == 0);
    assert(root->root_start_sector % desc->sectors_per_cluster == 0);
    assert(root->data_start_sector % desc->sectors_per_cluster == 0);

    u32 sector_count = MAX(desc->sector_count, desc->sector_count16);
    root->data_cluster = root->data_start_sector / desc->sectors_per_cluster;
    u32 data_sectors = sector_count - root->data_start_sector;
    root->data_cluster_size = data_sectors / desc->sectors_per_cluster;

    root->root_cluster = root->root_start_sector / desc->sectors_per_cluster;
    root->root_cluster_size = root->data_cluster - root->root_cluster;

    root->fat_cluster = root->fat_start_sector / desc->sectors_per_cluster;
    root->fat_cluster_size = desc->sectors_per_fat / desc->sectors_per_cluster;

    root->cluster_offset = root->data_cluster - 2;

    fat_entry_t *entry = (fat_entry_t *)root;

    entry->filesize = 0;
    entry->attrs = fat_directory;
    entry->cluster = 0;
    super->iroot = fat_make_inode(super, entry, NULL);
    super->iroot->desc = root;

    for (int i = root->fat_cluster; i < root->data_cluster; i++)
    {
        buf = bread(super->dev, i, super->block_size);
        memset(buf->data, 0, super->block_size);
        buf->dirty = true;
        brelse(buf);
    }

    fat16_write_next_cluster(super, 0, END_FILE_CLUSTER);
    fat16_write_next_cluster(super, 1, END_FILE_CLUSTER);
    fat16_write_next_cluster(super, 2, END_FILE_CLUSTER);

    inode_t *inode;
    assert(fat16_open(super->iroot, "readme.txt", O_CREAT | O_TRUNC, 0, &inode) == EOK);
    char message[] = "fat 16 file system.\n";
    fat16_write(inode, message, sizeof(message), 0);

    iput(inode);
    put_super(super);

    return EOK;
}

static fs_op_t fat16_op = {

    fat16_mkfs, // int (*mkfs)(dev_t dev, int args);

    fat16_super, // int (*super)(dev_t dev, super_t *super);

    fat16_open, // int (*open)(inode_t *dir, char *name, int flags, int mode, inode_t **result);
    fat_close,  // void (*close)(inode_t *inode);

    fs_default_nosys, // int (*ioctl)(inode_t *inode, int cmd, void *args);
    fat16_read,       // int (*read)(inode_t *inode, char *data, int len, off_t offset);
    fat16_write,      // int (*write)(inode_t *inode, char *data, int len, off_t offset);
    fat16_truncate,   // int (*truncate)(inode_t *inode);

    fat_stat,       // int (*stat)(inode_t *inode, stat_t *stat);
    fat_permission, // int (*permission)(inode_t *inode, int mask);

    fat16_namei,      // int (*namei)(inode_t *dir, char *name, char **next, inode_t **result);
    fat16_mkdir,      // int (*mkdir)(inode_t *dir, char *name, int mode);
    fat16_rmdir,      // int (*rmdir)(inode_t *dir, char *name);
    fs_default_nosys, // int (*link)(inode_t *odir, char *oldname, inode_t *ndir, char *newname);
    fat16_unlink,     // int (*unlink)(inode_t *dir, char *name);
    fs_default_nosys, // int (*mknod)(inode_t *dir, char *name, int mode, int dev);
    fat16_readdir,    // int (*readdir)(inode_t *inode, dentry_t *entry, size_t count, off_t offset);
};

void fat16_init()
{
    fs_register_op(FS_TYPE_FAT16, &fat16_op);
}
