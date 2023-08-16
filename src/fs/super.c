#include <onix/fs.h>
#include <onix/buffer.h>
#include <onix/device.h>
#include <onix/assert.h>
#include <onix/string.h>
#include <onix/stat.h>
#include <onix/task.h>
#include <onix/syscall.h>
#include <onix/stdlib.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SUPER_NR 16

static super_t super_table[SUPER_NR]; // 超级块表
static super_t *root;                 // 根文件系统超级块

// 从超级块表中查找一个空闲块
super_t *get_free_super()
{
    for (size_t i = 0; i < SUPER_NR; i++)
    {
        super_t *super = &super_table[i];
        if (super->type == FS_TYPE_NONE)
        {
            assert(super->count == 0);
            return super;
        }
    }
    panic("no more super block!!!");
}

// 获得设备 dev 的超级块
super_t *get_super(dev_t dev)
{
    for (size_t i = 0; i < SUPER_NR; i++)
    {
        super_t *super = &super_table[i];
        if (super->type == FS_TYPE_NONE)
            continue;

        if (super->dev == dev)
        {
            return super;
        }
    }
    return NULL;
}

void put_super(super_t *super)
{
    if (!super)
        return;
    assert(super->count > 0);
    super->count--;
    if (super->count)
        return;

    super->type = FS_TYPE_NONE;
    iput(super->imount);
    iput(super->iroot);
    brelse(super->buf);
}

// 读设备 dev 的超级块
super_t *read_super(dev_t dev)
{
    super_t *super = get_super(dev);
    if (super)
    {
        super->count++;
        return super;
    }

    LOGK("Reading super block of device %d\n", dev);

    // 获得空闲超级块
    super = get_free_super();
    super->count++;

    for (size_t i = 1; i < FS_TYPE_NUM; i++)
    {
        fs_op_t *op = fs_get_op(i);
        if (!op)
            continue;
        if (op->super(dev, super) == EOK)
        {
            return super;
        }
    }

    put_super(super);
    return NULL;
}

// 挂载根文件系统
static void mount_root()
{
    LOGK("Mount root file system...\n");
    // 假设主硬盘第一个分区是根文件系统
    device_t *device = device_find(DEV_IDE_PART, 0);
    if (!device)
    {
        device = device_find(DEV_IDE_CD, 0);
    }
    if (!device)
    {
        panic("Cann't find available device.");
    }

    // 读根文件系统超级块
    root = read_super(device->dev);
    assert(root);

    root->imount = root->iroot;
    root->imount->count++;

    root->iroot->mount = device->dev;
}

void super_init()
{
    for (size_t i = 0; i < SUPER_NR; i++)
    {
        super_t *super = &super_table[i];
        super->dev = EOF;
        super->type = FS_TYPE_NONE;
        super->desc = NULL;
        super->buf = NULL;
        super->iroot = NULL;
        super->imount = NULL;
        super->block_size = 0;
        super->sector_size = 0;
        list_init(&super->inode_list);
    }

    mount_root();
}
