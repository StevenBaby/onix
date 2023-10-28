#include <onix/fs.h>
#include <onix/buffer.h>
#include <onix/assert.h>
#include <onix/string.h>
#include <onix/stdlib.h>
#include <onix/arena.h>
#include <onix/task.h>
#include <onix/syscall.h>
#include <onix/device.h>
#include <onix/debug.h>

#include "fat.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SECTOR_SIZE 512 // 扇区大小

#define FREE_CLUSTER 0x00000000
#define BAD_CLUSTER 0x0FFFFFF7
#define END_CLUSTER 0x0FFFFFF8 // >
#define END_FILE_CLUSTER 0x0FFFFFFF

#define END_ENTRY 0x00
#define FREE_ENTRY 0xE5
#define LAST_LONG_ENTRY 0x40

#define GOOD_CLUSTER(cluster) (cluster >= 3 && cluster < END_CLUSTER)
#define VALID_CLUSTER(cluster) (cluster >= 0 && cluster < END_CLUSTER)

static _inline u32 fat32_cluster(fat32_entry_t *entry)
{
    return (u32)entry->first_cluster_high << 16 | entry->first_cluster_low;
}

static _inline void fat32_set_cluster(fat32_entry_t *entry, u32 cluster)
{
    entry->first_cluster_high = cluster >> 16;
    entry->first_cluster_low = cluster & 0xFFFF;
}

inode_t *fat32_make_inode(super_t *super, fat32_entry_t *entry, buffer_t *buf)
{
    task_t *task = running_task();
    inode_t *inode = get_free_inode();

    list_push(&super->inode_list, &inode->node);

    if (buf)
        buf->count++;

    inode->count++;
    inode->desc = entry;
    inode->buf = buf;
    inode->dev = super->dev;
    inode->rdev = 0;
    inode->nr = fat32_cluster(entry);
    inode->super = super;
    inode->op = fs_get_op(super->type);

    inode->mtime = fat_format_time(entry->mdate, entry->mtime);
    inode->ctime = fat_format_time(entry->cdate, entry->ctime);
    inode->atime = fat_format_time(entry->last_accessed_date, 0);

    inode->mode = fat_make_mode((fat_entry_t *)entry);
    inode->uid = task->uid;
    inode->gid = task->gid;
    inode->size = entry->filesize;
    inode->type = super->type;
    return inode;
}

inode_t *fat32_iget(super_t *super, fat32_entry_t *entry, buffer_t *buf)
{
    u32 cluster = fat32_cluster(entry);
    inode_t *inode = find_inode(super->dev, cluster);
    if (inode)
    {
        inode->count++;
        inode->atime = time();
    }
    else
        inode = fat32_make_inode(super, entry, buf);

    return fit_inode(inode);
}

static err_t fat32_read_next_cluster(super_t *super, inode_t *inode, idx_t cluster)
{
    fat_root_entry_t *root = (fat_root_entry_t *)super->iroot->desc;
    fat_super_t *desc = (fat_super_t *)super->desc;

    u32 start = cluster * 4;                                   // 开始字节数
    u32 offset = start % super->block_size;                    // 高速缓冲块中偏移
    idx_t idx = start / super->block_size + root->fat_cluster; // FAT 高速缓冲索引
    u32 limit = root->fat_cluster + root->fat_cluster_size;    // 高速缓冲最大索引
    if (idx >= limit)
        return -ENOSPC;

    buffer_t *buf = bread(super->dev, idx, super->block_size);
    u32 *data = (u32 *)(buf->data + offset);
    u32 next = *data;
    brelse(buf);
    return next;
}

static err_t fat32_write_next_cluster(super_t *super, idx_t cluster, u32 value)
{
    fat_root_entry_t *root = (fat_root_entry_t *)super->iroot->desc;
    fat_super_t *desc = (fat_super_t *)super->desc;
    assert(desc->fat_count == 2);

    u32 start = cluster * 4;                                   // 开始字节数
    u32 offset = start % super->block_size;                    // 高速缓冲块中偏移
    idx_t idx = start / super->block_size + root->fat_cluster; // FAT 高速缓冲索引
    u32 limit = root->fat_cluster + root->fat_cluster_size;    // 高速缓冲最大索引
    u32 first = true;
    if (idx >= limit)
        return -ENOSPC;

again:
    buffer_t *buf = bread(super->dev, idx, super->block_size);

    u32 *data = (u32 *)(buf->data + offset);
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

static _inline buffer_t *fat32_bread(inode_t *inode, int cluster, idx_t *next)
{
    fat_root_entry_t *root = inode->super->iroot->desc;
    idx_t idx = cluster + root->cluster_offset;
    buffer_t *buf = bread(inode->dev, idx, inode->super->block_size);
    *next = fat32_read_next_cluster(inode->super, inode, cluster);
    return buf;
}

static buffer_t *fat32_find_entry(inode_t *dir, char *name, char **next, fat32_entry_t **result)
{
    fat32_super_t *desc = (fat32_super_t *)dir->super->desc;
    fat32_entry_t *entry = (fat32_entry_t *)dir->desc;
    u32 cluster = fat32_cluster(entry);

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
            buf = fat32_bread(dir, cluster, &cluster);
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

    // root direcory
    if (*result && fat32_cluster(*result) == 0)
    {
        fat32_set_cluster(*result, desc->root_directory_cluster);
    }
    return ret;
}

static u32 fat32_find_free_cluster(super_t *super)
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
        u32 *ptr = (u32 *)buf->data + offset;
        while ((u32)ptr < (u32)buf->data + super->block_size)
        {
            u32 entry = *(u32 *)ptr;
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

static buffer_t *fat32_add_entry(inode_t *dir, char *name, fat32_entry_t **result)
{
    int namelen;
    char *ext = NULL;
    if ((namelen = validate_short_name(name, &ext)) < EOK)
        return NULL;

    char *next = NULL;
    buffer_t *buf = fat32_find_entry(dir, name, &next, result);
    if (buf)
        return buf;

    // 需要连续的目录数量
    int total = div_round_up(namelen, 13) + 1;
    int count = 0;
    fat_lentry_t **entries = (fat_lentry_t **)kmalloc(sizeof(void *) * total);

    u16 cdate;
    u16 ctime;
    fat_parse_time(time(), &cdate, &ctime);

    fat32_super_t *desc = (fat32_super_t *)dir->super->desc;
    fat32_entry_t *entry = (fat32_entry_t *)dir->desc;
    u32 cluster = fat32_cluster(entry);
    bool new_cluster = false;

    buffer_t *buf0 = NULL;
    buffer_t *ret = NULL;

    for (size_t i = 0; true; i++, entry++)
    {
        if (cluster >= END_CLUSTER)
        {
            cluster = fat32_find_free_cluster(dir->super);
            assert(GOOD_CLUSTER(cluster));
            assert(fat32_write_next_cluster(dir->super, cluster, END_FILE_CLUSTER) == EOK);
            new_cluster = true;
        }
        if (!buf || (u32)entry >= (u32)buf->data + dir->super->block_size)
        {

            if (!VALID_CLUSTER(cluster))
                break;

            brelse(buf0);
            buf0 = buf;
            brelse(buf);
            buf = fat32_bread(dir, cluster, &cluster);
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

static err_t fat32_remove_entry(inode_t *dir, char *name)
{
    fat32_super_t *desc = (fat32_super_t *)dir->super->desc;
    fat32_entry_t *entry = (fat32_entry_t *)dir->desc;
    u32 cluster = fat32_cluster(entry);

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
            buf = fat32_bread(dir, cluster, &cluster);
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

/// ============================================================================

static err_t fat32_namei(inode_t *dir, char *name, char **next, inode_t **result)
{
    assert(dir->super->type == FS_TYPE_FAT32);
    fat32_entry_t *entry = NULL;

    buffer_t *buf = fat32_find_entry(dir, name, next, &entry);
    if (!buf)
        return -ENOENT;

    inode_t *inode = fat32_iget(dir->super, entry, buf);
    *result = inode;
    brelse(buf);
    return EOK;
}

// 打开 dir 目录的 name 文件
int fat32_open(inode_t *dir, char *name, int flags, int mode, inode_t **result)
{
    if ((!memcmp(name, ".", 1) || !memcmp(name, "..", 2)) && dir == dir->super->iroot)
    {
        dir->count++;
        *result = dir;
        return EOK;
    }

    inode_t *inode = NULL;
    buffer_t *buf = NULL;
    fat32_entry_t *entry = NULL;
    char *next = NULL;
    int ret = EOF;

    if ((flags & O_TRUNC) && ((flags & O_ACCMODE) == O_RDONLY))
        flags |= O_RDWR;

    ret = fat32_namei(dir, name, &next, &inode);
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

    buf = fat32_add_entry(dir, name, &entry);
    if (!buf)
    {
        ret = -EIO;
        goto rollback;
    }

    u16 d, t;
    fat_parse_time(time(), &d, &t);

    buf->dirty = true;
    fat32_set_cluster(entry, END_FILE_CLUSTER);
    entry->filesize = 0;
    entry->attrs = 0;
    entry->mdate = d;
    entry->mtime = t;
    entry->cdate = d;
    entry->ctime = t;
    entry->last_accessed_date = d;

    inode = fat32_iget(dir->super, entry, buf);

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
int fat32_read(inode_t *inode, char *data, int size, off_t offset)
{
    assert(inode->type == FS_TYPE_FAT32);

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
        cluster = fat32_read_next_cluster(inode->super, inode, cluster);
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
        buf = fat32_bread(inode, cluster, &cluster);

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

int fat32_readdir(inode_t *inode, dentry_t *result, size_t count, off_t offset)
{

    assert(ISDIR(inode->mode));
    int ret = EOF;

    assert(inode->type == FS_TYPE_FAT32);

    fat_entry_t *entry = (fat_entry_t *)inode->desc;
    fat_super_t *desc = (fat_super_t *)inode->super->desc;
    size_t block_size = inode->super->block_size;

    // 开始读取的位置
    u32 begin = 0;

    idx_t cluster = entry->cluster;
    while (begin + block_size < offset && VALID_CLUSTER(cluster))
    {
        cluster = fat32_read_next_cluster(inode->super, inode, cluster);
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
            buf = fat32_bread(inode, cluster, &cluster);
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
int fat32_write(inode_t *inode, char *data, int size, off_t offset)
{
    assert(inode->type == FS_TYPE_FAT32);
    assert(ISFILE(inode->mode));

    fat32_entry_t *entry = (fat32_entry_t *)inode->desc;
    assert(entry);
    if (entry->attrs & fat_readonly)
        return -EPERM;

    fat32_super_t *desc = (fat32_super_t *)inode->super->desc;
    size_t block_size = inode->super->block_size;

    // 开始读取的位置
    u32 begin = 0;

    idx_t cluster = fat32_cluster(entry);

    if (!VALID_CLUSTER(cluster))
    {
        cluster = fat32_find_free_cluster(inode->super);
        assert(GOOD_CLUSTER(cluster));
        fat32_set_cluster(entry, cluster);
        inode->buf->dirty = true;
        fat32_write_next_cluster(inode->super, cluster, END_FILE_CLUSTER);
    }

    while (begin + block_size < offset)
    {
        cluster = fat32_read_next_cluster(inode->super, inode, cluster);
        begin += block_size;
        if (VALID_CLUSTER(cluster))
            continue;

        cluster = fat32_find_free_cluster(inode->super);
        assert(GOOD_CLUSTER(cluster));
        fat32_write_next_cluster(inode->super, cluster, END_FILE_CLUSTER);
    }

    assert(VALID_CLUSTER(cluster));

    begin = offset;

    // 剩余字节数
    u32 left = size;
    buffer_t *buf = NULL;

    while (left)
    {
        if (!VALID_CLUSTER(cluster))
        {
            cluster = fat32_find_free_cluster(inode->super);
            assert(GOOD_CLUSTER(cluster));
            fat32_write_next_cluster(inode->super, cluster, END_FILE_CLUSTER);
        }

        // 读取文件块缓冲
        buf = fat32_bread(inode, cluster, &cluster);
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
static int fat32_truncate(inode_t *inode)
{
    if (!ISFILE(inode->mode) && !ISDIR(inode->mode))
        return -EPERM;

    assert(inode != inode->super->iroot);
    fat32_entry_t *entry = (fat32_entry_t *)inode->desc;
    u32 before = fat32_cluster(entry);

    while (GOOD_CLUSTER(before))
    {
        u32 after = fat32_read_next_cluster(inode->super, inode, before);
        assert(fat32_write_next_cluster(inode->super, before, FREE_CLUSTER) == EOK);
        before = after;
    }

    entry->filesize = 0;
    fat32_set_cluster(entry, END_FILE_CLUSTER);
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
int fat32_mkdir(inode_t *dir, char *name, int mode)
{
    assert(dir->type == FS_TYPE_FAT32);
    assert(ISDIR(dir->mode));

    char *next;
    fat32_entry_t *entry;

    fat_super_t *desc = (fat_super_t *)dir->super->desc;
    fat32_entry_t *dir_entry = (fat32_entry_t *)dir->desc;

    buffer_t *ebuf = NULL;
    buffer_t *zbuf = NULL;
    int ret;

    ebuf = fat32_find_entry(dir, name, &next, &entry);
    if (ebuf)
    {
        ret = -EEXIST;
        goto rollback;
    }

    ebuf = fat32_add_entry(dir, name, &entry);
    if (!ebuf)
    {
        ret = -EIO;
        goto rollback;
    }

    ebuf->dirty = true;
    idx_t idx = fat32_find_free_cluster(dir->super);
    if (!GOOD_CLUSTER(idx))
    {
        ret = -EIO;
        goto rollback;
    }

    if ((ret = fat32_write_next_cluster(dir->super, idx, END_CLUSTER)) < EOK)
    {
        ret = -EIO;
        goto rollback;
    }

    entry->attrs = fat_directory;
    fat32_set_cluster(entry, idx);
    entry->filesize = 0;
    task_t *task = running_task();

    fat_root_entry_t *root = dir->super->iroot->desc;
    zbuf = bread(dir->dev, idx + root->cluster_offset, dir->super->block_size);
    zbuf->dirty = true;

    entry = (fat32_entry_t *)zbuf->data;
    memcpy(entry->name, ".       ", 8);
    memcpy(entry->ext, "   ", 3);
    fat32_set_cluster(entry, idx);
    entry->attrs = fat_directory;
    entry->filesize = 0;

    entry++;

    memcpy(entry->name, "..      ", 8);
    memcpy(entry->ext, "   ", 3);
    fat32_set_cluster(entry, fat32_cluster(dir_entry));
    entry->attrs = fat_directory;
    entry->filesize = 0;

    entry++;
    memset(entry, 0, sizeof(fat32_entry_t));

    ret = EOK;

rollback:
    brelse(ebuf);
    brelse(zbuf);
    return ret;
}

static bool fat32_is_empty(inode_t *dir)
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
            buf = fat32_bread(dir, cluster, &cluster);
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
int fat32_rmdir(inode_t *dir, char *name)
{
    assert(ISDIR(dir->mode));

    // 父目录无写权限
    if (!fat_permission(dir, P_WRITE))
        return -EPERM;

    char *next = NULL;
    fat32_entry_t *entry;

    buffer_t *buf = NULL;

    inode_t *inode = NULL;
    int ret = EOF;

    buf = fat32_find_entry(dir, name, &next, &entry);
    if (!buf)
    {
        ret = -ENOENT;
        goto rollback;
    }

    inode = fat32_iget(dir->super, entry, buf);
    if (inode == dir)
    {
        ret = -EPERM;
        goto rollback;
    }

    entry = (fat32_entry_t *)inode->desc;
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

    if (!fat32_is_empty(inode))
    {
        ret = -ENOTEMPTY;
        goto rollback;
    }

    assert(fat32_truncate(inode) == EOK);

    buf->dirty = true;
    assert(fat32_remove_entry(dir, name) == EOK);
    ret = EOK;

rollback:
    iput(inode);
    brelse(buf);
    return ret;
}

// 删除文件
int fat32_unlink(inode_t *dir, char *name)
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

    fat32_entry_t *entry;
    buf = fat32_find_entry(dir, name, &next, &entry);
    if (!buf) // 目录项不存在
    {
        ret = -ENOENT;
        goto rollback;
    }

    inode = fat32_iget(dir->super, entry, buf);
    assert(inode);

    if (ISDIR(inode->mode))
    {
        ret = -EPERM;
        goto rollback;
    }

    fat32_truncate(inode);
    assert(fat32_remove_entry(dir, name) == EOK);
    ret = EOK;

rollback:
    iput(inode);
    brelse(buf);
    return ret;
}

static int fat32_super(dev_t dev, super_t *super)
{
    // 读取超级块
    buffer_t *buf = bread(dev, 0, SECTOR_SIZE * 2);
    int ret = -EFSUNK;
    if (!buf)
        goto rollback;

    fat32_super_t *desc = (fat32_super_t *)buf->data;
    if (memcmp(desc->identifier, "FAT32   ", 8))
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
    super->type = FS_TYPE_FAT32;
    super->sector_size = desc->bytes_per_sector;
    super->block_size = block_size;

    fat_root_entry_t *root = kmalloc(sizeof(fat_root_entry_t));

    root->fat_start_sector = desc->reserved_sector_count;
    root->data_start_sector = root->fat_start_sector + desc->fat_count * desc->sectors_per_fat;

    assert(root->fat_start_sector % desc->sectors_per_cluster == 0);
    assert(root->data_start_sector % desc->sectors_per_cluster == 0);

    u32 sector_count = desc->sector_count;
    root->data_cluster = root->data_start_sector / desc->sectors_per_cluster;
    u32 data_sectors = sector_count - root->data_start_sector;
    root->data_cluster_size = data_sectors / desc->sectors_per_cluster;

    root->fat_cluster = root->fat_start_sector / desc->sectors_per_cluster;
    root->fat_cluster_size = desc->sectors_per_fat / desc->sectors_per_cluster;

    root->cluster_offset = root->data_cluster - 2;

    fat32_entry_t *entry = (fat32_entry_t *)root;

    entry->filesize = 0;
    entry->attrs = fat_directory;
    entry->first_cluster_high = desc->root_directory_cluster >> 16;
    entry->first_cluster_low = desc->root_directory_cluster & 0xFFFF;
    super->iroot = fat32_make_inode(super, entry, NULL);
    super->iroot->desc = root;

    return EOK;

rollback:
    brelse(buf);
    return -EFSUNK;
}

err_t fat32_mkfs(dev_t dev, int cluster)
{
    u32 sectors = device_ioctl(dev, DEV_CMD_SECTOR_COUNT, NULL, 0);
    if (cluster <= 0)
        cluster = sectors / (0x0FFFFFF7 - 2);

    if (cluster > 8)
        return -EINVAL;

    if (cluster > 4)
        cluster = 8;
    else if (cluster > 2)
        cluster = 4;
    else
        cluster = 2;

    u32 clusters = sectors / cluster;
    if (clusters >= (0x0FFFFFF7 - 2))
        return -EINVAL;

    int block_size = cluster * SECTOR_SIZE;
    buffer_t *buf = bread(dev, 0, block_size);

    fat32_super_t *desc = (fat32_super_t *)buf->data;
    memset(desc, 0, sizeof(fat32_super_t));

    desc->bytes_per_sector = SECTOR_SIZE;
    desc->sectors_per_cluster = cluster;
    desc->reserved_sector_count = desc->sectors_per_cluster;
    desc->sector_count = sectors;

    // read https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
    desc->fat_count = 2;
    desc->root_directory_entry_count = 0; // must be 512
    desc->media_descriptor_type = 0xF8;   // fixed media
    desc->sectors_per_fat = 16;           // todo
    desc->sectors_per_track = 32;         // todo
    desc->head_count = 2;                 // todo
    desc->hidden_sector_count = device_ioctl(dev, DEV_CMD_SECTOR_START, NULL, 0);

    u16 d, t;
    fat_parse_time(time(), &d, &t);

    desc->drive_number = 0x80; // from document
    desc->nt_flags = 0;
    desc->signature = 0x29;
    desc->volume_id = (u32)d << 16 | t; // from document
    memcpy(desc->volume_label_string, "ONIX FAT32 ", 11);
    memcpy(desc->identifier, "FAT32   ", 8);

    super_t *super = get_free_super();
    super->count++;
    super->dev = dev;
    super->buf = buf;
    super->desc = desc;
    super->type = FS_TYPE_FAT32;
    super->sector_size = desc->bytes_per_sector;
    super->block_size = block_size;

    fat_root_entry_t *root = kmalloc(sizeof(fat_root_entry_t));

    root->fat_start_sector = desc->reserved_sector_count;
    root->data_start_sector = root->fat_start_sector + desc->fat_count * desc->sectors_per_fat;

    assert(root->fat_start_sector % desc->sectors_per_cluster == 0);
    assert(root->data_start_sector % desc->sectors_per_cluster == 0);

    u32 sector_count = desc->sector_count;
    root->data_cluster = root->data_start_sector / desc->sectors_per_cluster;
    u32 data_sectors = sector_count - root->data_start_sector;
    root->data_cluster_size = data_sectors / desc->sectors_per_cluster;

    root->fat_cluster = root->fat_start_sector / desc->sectors_per_cluster;
    root->fat_cluster_size = desc->sectors_per_fat / desc->sectors_per_cluster;

    root->cluster_offset = root->data_cluster - 2;

    fat32_entry_t *entry = (fat32_entry_t *)root;

    entry->filesize = 0;
    entry->attrs = fat_directory;
    entry->first_cluster_high = desc->root_directory_cluster >> 16;
    entry->first_cluster_low = desc->root_directory_cluster & 0xFFFF;
    super->iroot = fat32_make_inode(super, entry, NULL);
    super->iroot->desc = root;

    for (int i = root->fat_cluster; i < root->data_cluster + 1; i++)
    {
        buf = bread(super->dev, i, super->block_size);
        memset(buf->data, 0, super->block_size);
        buf->dirty = true;
        brelse(buf);
    }

    fat32_write_next_cluster(super, 0, END_FILE_CLUSTER);
    fat32_write_next_cluster(super, 1, END_FILE_CLUSTER);
    fat32_write_next_cluster(super, 2, END_FILE_CLUSTER);

    inode_t *inode;
    assert(fat32_open(super->iroot, "readme.txt", O_CREAT | O_TRUNC, 0, &inode) == EOK);
    char message[] = "fat 32 file system.\n";
    fat32_write(inode, message, sizeof(message), 0);

    iput(inode);
    put_super(super);

    return EOK;
}

static fs_op_t fat32_op = {

    fat32_mkfs, // int (*mkfs)(dev_t dev, int args);

    fat32_super, // int (*super)(dev_t dev, super_t *super);

    fat32_open, // int (*open)(inode_t *dir, char *name, int flags, int mode, inode_t **result);
    fat_close,  // void (*close)(inode_t *inode);

    fs_default_nosys, // int (*ioctl)(inode_t *inode, int cmd, void *args);
    fat32_read,       // int (*read)(inode_t *inode, char *data, int len, off_t offset);
    fat32_write,      // int (*write)(inode_t *inode, char *data, int len, off_t offset);
    fat32_truncate,   // int (*truncate)(inode_t *inode);

    fat_stat,       // int (*stat)(inode_t *inode, stat_t *stat);
    fat_permission, // int (*permission)(inode_t *inode, int mask);

    fat32_namei,      // int (*namei)(inode_t *dir, char *name, char **next, inode_t **result);
    fat32_mkdir,      // int (*mkdir)(inode_t *dir, char *name, int mode);
    fat32_rmdir,      // int (*rmdir)(inode_t *dir, char *name);
    fs_default_nosys, // int (*link)(inode_t *odir, char *oldname, inode_t *ndir, char *newname);
    fat32_unlink,     // int (*unlink)(inode_t *dir, char *name);
    fs_default_nosys, // int (*mknod)(inode_t *dir, char *name, int mode, int dev);
    fat32_readdir,    // int (*readdir)(inode_t *inode, dentry_t *entry, size_t count, off_t offset);
};

void fat32_init()
{
    fs_register_op(FS_TYPE_FAT32, &fat32_op);
}
