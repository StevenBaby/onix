#include <onix/fs.h>
#include <onix/buffer.h>
#include <onix/stat.h>
#include <onix/syscall.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 判断文件名是否相等
static bool match_name(const char *name, const char *entry_name, char **next)
{
    char *lhs = (char *)name;
    char *rhs = (char *)entry_name;
    while (*lhs == *rhs && *lhs != EOS && *rhs != EOS)
    {
        lhs++;
        rhs++;
    }
    if (*rhs)
        return false;
    if (*lhs && !IS_SEPARATOR(*lhs))
        return false;
    if (IS_SEPARATOR(*lhs))
        lhs++;
    *next = lhs;
    return true;
}

// 获取 dir 目录下的 name 目录 所在的 dentry_t 和 buffer_t
static buffer_t *find_entry(inode_t **dir, const char *name, char **next, dentry_t **result)
{
    // 保证 dir 是目录
    assert(ISDIR((*dir)->desc->mode));

    // 获取目录所在超级块
    // super_block_t *sb = read_super((*dir)->dev);

    // dir 目录最多子目录数量
    u32 entries = (*dir)->desc->size / sizeof(dentry_t);

    idx_t i = 0;
    idx_t block = 0;
    buffer_t *buf = NULL;
    dentry_t *entry = NULL;
    idx_t nr = EOF;

    for (; i < entries; i++, entry++)
    {
        if (!buf || (u32)entry >= (u32)buf->data + BLOCK_SIZE)
        {
            brelse(buf);
            block = bmap((*dir), i / BLOCK_DENTRIES, false);
            assert(block);

            buf = bread((*dir)->dev, block);
            entry = (dentry_t *)buf->data;
        }
        if (match_name(name, entry->name, next))
        {
            *result = entry;
            return buf;
        }
    }

    brelse(buf);
    return NULL;
}

// 在 dir 目录中添加 name 目录项
static buffer_t *add_entry(inode_t *dir, const char *name, dentry_t **result)
{
    char *next = NULL;

    buffer_t *buf = find_entry(&dir, name, &next, result);
    if (buf)
    {
        return buf;
    }

    // name 中不能有分隔符
    for (size_t i = 0; i < NAME_LEN && name[i]; i++)
    {
        assert(!IS_SEPARATOR(name[i]));
    }

    // super_block_t *sb = get_super(dir->dev);
    // assert(sb);

    idx_t i = 0;
    idx_t block = 0;
    dentry_t *entry;

    for (; true; i++, entry++)
    {
        if (!buf || (u32)entry >= (u32)buf->data + BLOCK_SIZE)
        {
            brelse(buf);
            block = bmap(dir, i / BLOCK_DENTRIES, true);
            assert(block);

            buf = bread(dir->dev, block);
            entry = (dentry_t *)buf->data;
        }
        if (i * sizeof(dentry_t) >= dir->desc->size)
        {
            entry->nr = 0;
            dir->desc->size = (i + 1) * sizeof(dentry_t);
            dir->buf->dirty = true;
        }
        if (entry->nr)
            continue;

        strncpy(entry->name, name, NAME_LEN);
        buf->dirty = true;
        dir->desc->mtime = time();
        dir->buf->dirty = true;
        *result = entry;
        return buf;
    };
}

#include <onix/task.h>

void dir_test()
{
    task_t *task = running_task();
    inode_t *inode = task->iroot;
    inode->count++;
    char *next = NULL;
    dentry_t *entry = NULL;
    buffer_t *buf = NULL;

    buf = find_entry(&inode, "hello.txt", &next, &entry);
    idx_t nr = entry->nr;
    brelse(buf);

    buf = add_entry(inode, "world.txt", &entry);
    entry->nr = nr;

    inode_t *hello = iget(inode->dev, nr);
    hello->desc->nlinks++;
    hello->buf->dirty = true;

    iput(inode);
    iput(hello);
    brelse(buf);

    // char pathname[] = "d1/d2/d3/d4";

    // dev_t dev = inode->dev;
    // char *name = pathname;
    // buf = find_entry(&inode, name, &next, &entry);
    // brelse(buf);

    // iput(inode);
    // inode = iget(dev, entry->nr);

    // name = next;
    // buf = find_entry(&inode, name, &next, &entry);
    // brelse(buf);

    // iput(inode);
    // inode = iget(dev, entry->nr);

    // name = next;
    // buf = find_entry(&inode, name, &next, &entry);
    // brelse(buf);

    // iput(inode);
    // inode = iget(dev, entry->nr);

    // name = next;
    // buf = find_entry(&inode, name, &next, &entry);
    // brelse(buf);
    // iput(inode);
}