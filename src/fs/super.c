#include <onix/fs.h>
#include <onix/buffer.h>
#include <onix/device.h>
#include <onix/assert.h>
#include <onix/string.h>
#include <onix/stat.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SUPER_NR 16

static super_block_t super_table[SUPER_NR]; // 超级块表
static super_block_t *root;                 // 根文件系统超级块

// 从超级块表中查找一个空闲块
static super_block_t *get_free_super()
{
    for (size_t i = 0; i < SUPER_NR; i++)
    {
        super_block_t *sb = &super_table[i];
        if (sb->dev == EOF)
        {
            return sb;
        }
    }
    panic("no more super block!!!");
}

// 获得设备 dev 的超级块
super_block_t *get_super(dev_t dev)
{
    for (size_t i = 0; i < SUPER_NR; i++)
    {
        super_block_t *sb = &super_table[i];
        if (sb->dev == dev)
        {
            return sb;
        }
    }
    return NULL;
}

void put_super(super_block_t *sb)
{
    if (!sb)
        return;
    assert(sb->count > 0);
    sb->count--;
    if (sb->count)
        return;

    sb->dev = EOF;
    iput(sb->imount);
    iput(sb->iroot);

    for (int i = 0; i < sb->desc->imap_blocks; i++)
        brelse(sb->imaps[i]);
    for (int i = 0; i < sb->desc->zmap_blocks; i++)
        brelse(sb->zmaps[i]);

    brelse(sb->buf);
}

// 读设备 dev 的超级块
super_block_t *read_super(dev_t dev)
{
    super_block_t *sb = get_super(dev);
    if (sb)
    {
        sb->count++;
        return sb;
    }

    LOGK("Reading super block of device %d\n", dev);

    // 获得空闲超级块
    sb = get_free_super();

    // 读取超级块
    buffer_t *buf = bread(dev, 1);

    sb->buf = buf;
    sb->desc = (super_desc_t *)buf->data;
    sb->dev = dev;
    sb->count = 1;

    assert(sb->desc->magic == MINIX1_MAGIC);

    memset(sb->imaps, 0, sizeof(sb->imaps));
    memset(sb->zmaps, 0, sizeof(sb->zmaps));

    // 读取 inode 位图
    int idx = 2; // 块位图从第 2 块开始，第 0 块 引导块，第 1 块 超级块

    for (int i = 0; i < sb->desc->imap_blocks; i++)
    {
        assert(i < IMAP_NR);
        if ((sb->imaps[i] = bread(dev, idx)))
            idx++;
        else
            break;
    }

    // 读取块位图
    for (int i = 0; i < sb->desc->zmap_blocks; i++)
    {
        assert(i < ZMAP_NR);
        if ((sb->zmaps[i] = bread(dev, idx)))
            idx++;
        else
            break;
    }

    return sb;
}

// 挂载根文件系统
static void mount_root()
{
    LOGK("Mount root file system...\n");
    // 假设主硬盘第一个分区是根文件系统
    device_t *device = device_find(DEV_IDE_PART, 0);
    assert(device);

    // 读根文件系统超级块
    root = read_super(device->dev);

    // 初始化根目录 inode
    root->iroot = iget(device->dev, 1);  // 获得根目录 inode
    root->imount = iget(device->dev, 1); // 根目录挂载 inode

    root->iroot->mount = device->dev;
}

void super_init()
{
    for (size_t i = 0; i < SUPER_NR; i++)
    {
        super_block_t *sb = &super_table[i];
        sb->dev = EOF;
        sb->desc = NULL;
        sb->buf = NULL;
        sb->iroot = NULL;
        sb->imount = NULL;
        list_init(&sb->inode_list);
    }

    mount_root();
}

int sys_mount(char *devname, char *dirname, int flags)
{
    LOGK("mount %s to %s\n", devname, dirname);

    inode_t *devinode = NULL;
    inode_t *dirinode = NULL;
    super_block_t *sb = NULL;
    devinode = namei(devname);
    if (!devinode)
        goto rollback;
    if (!ISBLK(devinode->desc->mode))
        goto rollback;

    dev_t dev = devinode->desc->zone[0];

    dirinode = namei(dirname);
    if (!dirinode)
        goto rollback;

    if (!ISDIR(dirinode->desc->mode))
        goto rollback;

    if (dirinode->count != 1 || dirinode->mount)
        goto rollback;

    sb = read_super(dev);
    if (sb->imount)
        goto rollback;

    sb->iroot = iget(dev, 1);
    sb->imount = dirinode;
    dirinode->mount = dev;
    iput(devinode);
    return 0;

rollback:
    put_super(sb);
    iput(devinode);
    iput(dirinode);
    return EOF;
}

int sys_umount(char *target)
{
    LOGK("umount %s\n", target);
    inode_t *inode = NULL;
    super_block_t *sb = NULL;
    int ret = EOF;

    inode = namei(target);
    if (!inode)
        goto rollback;

    if (!ISBLK(inode->desc->mode) && inode->nr != 1)
        goto rollback;

    if (inode == root->imount)
        goto rollback;

    dev_t dev = inode->dev;
    if (ISBLK(inode->desc->mode))
    {
        dev = inode->desc->zone[0];
    }

    sb = get_super(dev);
    if (!sb->imount)
        goto rollback;

    if (!sb->imount->mount)
    {
        LOGK("warning super block mount = 0\n");
    }

    if (list_size(&sb->inode_list) > 1)
        goto rollback;

    iput(sb->iroot);
    sb->iroot = NULL;

    sb->imount->mount = 0;
    iput(sb->imount);
    sb->imount = NULL;
    ret = 0;

rollback:
    put_super(sb);
    iput(inode);
    return ret;
}
