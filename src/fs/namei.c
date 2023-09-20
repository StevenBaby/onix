#include <onix/fs.h>
#include <onix/buffer.h>
#include <onix/stat.h>
#include <onix/syscall.h>
#include <onix/string.h>
#include <onix/task.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 判断文件名是否相等
bool match_name(const char *name, const char *entry_name, char **next, int count)
{
    char *lhs = (char *)name;
    char *rhs = (char *)entry_name;

    while (*lhs == *rhs && *lhs != EOS && *rhs != EOS && count--)
    {
        lhs++;
        rhs++;
    }

    if (*rhs && count)
        return false;
    if (*lhs && !IS_SEPARATOR(*lhs))
        return false;
    if (IS_SEPARATOR(*lhs))
        lhs++;
    *next = lhs;
    return true;
}

// 获取 pathname 对应的父目录 inode
inode_t *named(char *pathname, char **next)
{
    inode_t *dir = NULL;
    task_t *task = running_task();
    char *name = pathname;
    if (IS_SEPARATOR(name[0]))
    {
        dir = task->iroot;
        name++;
    }
    else if (name[0])
        dir = task->ipwd;
    else
        return NULL;

    assert(dir->dev > 0);

    dir->count++;
    *next = name;

    if (!*name)
    {
        return dir;
    }

    char *right = strrsep(name);
    if (!right || right < name)
    {
        return dir;
    }

    right++;

    inode_t *inode = NULL;
    while (true)
    {
        // 返回上一级目录且上一级目录是个挂载点
        if (match_name(name, "..", next, 3) && dir == dir->super->iroot)
        {
            super_t *super = dir->super;
            inode = super->imount;
            inode->count++;
            iput(dir);
            dir = inode;
        }

        int ret = dir->op->namei(dir, name, next, &inode);
        if (ret < EOK)
            goto rollback;

        iput(dir);
        dir = inode;

        if (!ISDIR(dir->mode) || !dir->op->permission(dir, P_EXEC))
            goto rollback;

        if (right == *next)
        {
            return dir;
        }
        name = *next;
    }

rollback:
    iput(dir);
    return NULL;
}

// 获取 pathname 对应的 inode
inode_t *namei(char *pathname)
{
    char *next = NULL;
    inode_t *dir = named(pathname, &next);
    if (!dir)
        return NULL;

    if (!(*next))
        return dir;

    char *name = next;
    inode_t *inode = NULL;

    // 返回上一级目录且上一级目录是个挂载点
    if (match_name(name, "..", &next, 3) && dir == dir->super->iroot)
    {
        super_t *super = dir->super;
        inode = super->imount;
        inode->count++;
        iput(dir);
        dir = inode;
    }

    int ret = dir->op->namei(dir, name, &next, &inode);

    iput(dir);
    return inode;
}
