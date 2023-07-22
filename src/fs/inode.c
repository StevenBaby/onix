#include <onix/fs.h>
#include <onix/stat.h>
#include <onix/syscall.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/buffer.h>
#include <onix/arena.h>
#include <onix/string.h>
#include <onix/stdlib.h>
#include <onix/memory.h>
#include <onix/stat.h>
#include <onix/task.h>
#include <onix/fifo.h>
#include <onix/device.h>

#include "minix/minix.h"

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define INODE_NR 64

static inode_t inode_table[INODE_NR];

// 申请一个 inode
inode_t *get_free_inode()
{
    for (size_t i = 0; i < INODE_NR; i++)
    {
        inode_t *inode = &inode_table[i];
        if (inode->type == FS_TYPE_NONE)
        {
            return inode;
        }
    }
    panic("no more inode!!!");
}

// 释放一个 inode
void put_free_inode(inode_t *inode)
{
    assert(inode != inode_table);
    assert(inode->count == 0);
    inode->type = FS_TYPE_NONE;
}

// 获取根 inode
inode_t *get_root_inode()
{
    return inode_table;
}

// 从已有 inode 中查找编号为 nr 的 inode
inode_t *find_inode(dev_t dev, idx_t nr)
{
    super_t *super = get_super(dev);
    assert(super);
    list_t *list = &super->inode_list;

    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next)
    {
        inode_t *inode = element_entry(inode_t, node, node);
        if (inode->nr == nr)
        {
            return inode;
        }
    }
    return NULL;
}

inode_t *fit_inode(inode_t *inode)
{
    if (!inode->mount)
        return inode;

    super_t *super = get_super(inode->mount);
    assert(super);
    iput(inode);
    inode = super->iroot;
    inode->count++;
    return inode;
}

// 释放 inode
void iput(inode_t *inode)
{
    if (!inode)
        return;
    inode->op->close(inode);
}

void inode_init()
{
    for (size_t i = 0; i < INODE_NR; i++)
    {
        inode_t *inode = &inode_table[i];
        inode->dev = EOF;
        inode->type = FS_TYPE_NONE;
        inode->rxwaiter = NULL;
        inode->txwaiter = NULL;
    }
}
