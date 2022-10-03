#include <onix/fs.h>
#include <onix/buffer.h>
#include <onix/device.h>
#include <onix/assert.h>
#include <onix/string.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

void super_init()
{
    device_t *device = device_find(DEV_IDE_PART, 0);
    assert(device);

    buffer_t *boot = bread(device->dev, 0);
    buffer_t *super = bread(device->dev, 1);

    super_desc_t *sb = (super_desc_t *)super->data;
    assert(sb->magic == MINIX1_MAGIC);

    // inode 位图
    buffer_t *imap = bread(device->dev, 2);

    // 块位图
    buffer_t *zmap = bread(device->dev, 2 + sb->imap_blocks);

    // 读取第一个 inode 块
    buffer_t *buf1 = bread(device->dev, 2 + sb->imap_blocks + sb->zmap_blocks);
    inode_desc_t *inode = (inode_desc_t *)buf1->data;

    buffer_t *buf2 = bread(device->dev, inode->zone[0]);

    dentry_t *dir = (dentry_t *)buf2->data;
    inode_desc_t *helloi = NULL;
    while (dir->nr)
    {
        LOGK("inode %04d, name %s\n", dir->nr, dir->name);
        if (!strcmp(dir->name, "hello.txt"))
        {
            helloi = &((inode_desc_t *) buf1->data)[dir->nr - 1];
            strcpy(dir->name, "world.txt");
            buf2->dirty = true;
            bwrite(buf2);
        }
        dir++;
    }

    buffer_t *buf3 = bread(device->dev, helloi->zone[0]);
    LOGK("content %s", buf3->data);

    strcpy(buf3->data, "This is modified content!!!\n");
    buf3->dirty = true;
    bwrite(buf3);

    helloi->size = strlen(buf3->data);
    buf1->dirty = true;
    bwrite(buf1);
}