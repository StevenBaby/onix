#include <onix/types.h>
#include <onix/device.h>
#include <onix/buffer.h>
#include <onix/time.h>
#include <onix/memory.h>
#include <onix/task.h>
#include <onix/arena.h>
#include <onix/fs.h>
#include <onix/string.h>
#include <onix/stdlib.h>
#include <onix/syscall.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/errno.h>
#include <onix/stat.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define BLOCK_SIZE 2048

#define AREA(from, to) (to - from + 1)

// VD Volume Descriptor 卷描述符
#define ISO_VD_BOOT 0         // 引导记录
#define ISO_VD_PRIMARY 1      // 主卷描述符
#define ISO_VD_SUPPLEMENTAL 2 // 补充卷描述符
#define ISO_VD_PARTITION 3    // 卷分区描述符
#define ISO_VD_END 255        // 卷描述符集结束符

#define ISO_FLAG_HIDDEN 0x01 // 隐藏标志
#define ISO_FLAG_SUBDIR 0x02 // 子目录
#define ISO_FLAG_REF 0x04    // 关联文件
#define ISO_FLAG_EXTFMT 0x08 // 扩展文件格式信息
#define ISO_FLAG_EXTUSR 0x10 // 扩展所有者和组权限
#define ISO_FLAG_LONG 0x80   // 大文件

typedef struct iso_dentry_t
{
    u8 length;             // 目录记录的长度
    u8 ext_attr_length;    // 扩展属性记录长度
    u32 lba;               // 两端格式的区段位置
    u32 lba_msb;           // 两端格式的区段位置-大端
    u32 size;              // 两端格式的数据长度
    u32 size_msb;          // 两端格式的数据长度-大端
    u8 date[AREA(19, 25)]; // 记录日期和时间
    u8 flags;              // 文件标志
    u8 file_unit_size;     // 文件单位大小
    u8 interleave;         // 交错间隙大小
    u16 volume_number;     // 卷序列号
    u16 volume_number_msb; // 卷序列号-大端
    u8 namelen;            // 文件标识符长度
    u8 name[0];            // 文件标识符
} _packed iso_dentry_t;

typedef struct iso_desc_t
{
    u8 type;                        // 类型 711
    u8 id[AREA(2, 6)];              // 标识符
    u8 version;                     // 版本
    u8 RESERVED;                    // 保留
    u8 system_id[AREA(9, 40)];      // 系统标识符 A字符
    u8 volume_id[AREA(41, 72)];     // 卷标识符 D字符
    u8 RESERVED[AREA(73, 80)];      // 保留
    u32 volume_space_size;          // 卷空间大小-小端
    u32 volume_space_size_msb;      // 卷空间大小-大段（x86不用）
    u8 RESERVED[AREA(89, 120)];     // 未使用
    u16 volume_set_size;            // 卷集大小
    u16 volume_set_size_msb;        // 卷集大小-大端
    u16 volume_sequence_number;     // 卷序列号
    u16 volume_sequence_number_msb; // 卷序列号-大端
    u16 logical_block_size;         // 逻辑块大小
    u16 logical_block_size_msb;     // 逻辑块大小-大端
    u32 path_table_size;            // 路径表大小
    u32 path_table_size_msb;        // 路径表大小-大端
    u32 l_path_table;               // L型路径表的位置
    u32 opt_l_path_table;           // 可选L型路径表的位置
    u32 m_path_table;               // M型路径表的位置
    u32 opt_m_path_table;           // 可选M型路径表的位置

    union
    {
        u8 root_dentry[AREA(157, 190)]; // 根目录的目录入口
        iso_dentry_t root;
    };

    u8 volume_set_id[AREA(191, 318)];         // 卷集标识符
    u8 publisher_id[AREA(319, 446)];          // 出版商标识符
    u8 preparer_id[AREA(447, 574)];           // 数据准备标识符
    u8 application_id[AREA(575, 702)];        // 应用标识符
    u8 copyright_file_id[AREA(703, 739)];     // 版权文件标识符
    u8 abstract_file_id[AREA(740, 776)];      // 摘要文件标识符
    u8 bibliographic_file_id[AREA(777, 813)]; // 书目文件标识符
    u8 create_date[AREA(814, 830)];           // 卷创建日期和时间
    u8 modify_date[AREA(831, 847)];           // 卷修改日期和时间
    u8 expire_date[AREA(848, 864)];           // 卷过期日期和时间
    u8 effect_date[AREA(865, 881)];           // 卷生效日期和时间
    u8 file_struct_version;                   // 文件结构版本
    u8 RESERVED;                              // 保留
    u8 application_data[AREA(884, 1395)];     // 应用程序使用
    u8 RESERVED[AREA(1396, 2048)];            // 保留
} _packed iso_desc_t;

static time_t iso_make_datetime(u8 *data)
{
    tm t;
    t.tm_year = data[0];
    t.tm_mon = data[1];
    t.tm_mday = data[2];
    t.tm_hour = data[3];
    t.tm_min = data[4];
    t.tm_sec = data[5];
    t.tm_min += ((char)data[6] - 48) * 15;
    return mktime(&t);
}

static int iso_namelen(char *name, int len)
{
    if (len > 1 && name[len - 2] == ';')
        len -= 2;
    if (len > 0 && name[len - 1] == '.')
        len--;
    return len;
}

static bool iso_match_name(char *name1, int len1, char *name2, int len2)
{
    char *next;
    if (len2 == 1 && !memcmp("\0", name2, 1) && match_name(name1, ".", &next))
    {
        return true;
    }
    if (len2 == 1 && !memcmp("\x01", name2, 1) && match_name(name1, "..", &next))
    {
        return true;
    }

    len2 = iso_namelen(name2, len2);
    if (len1 != len2)
        return false;

    while (len1--)
    {
        if (toupper(*name1) != *name2)
            return false;
        name1++;
        name2++;
    }
    return true;
}

static int namelen(char *name, char **next)
{
    char *ptr = name;
    while (*ptr != EOS && !IS_SEPARATOR(*ptr))
    {
        ptr++;
    }
    if (IS_SEPARATOR(*ptr))
        *next = ptr + 1;
    else
        *next = ptr;
    return ptr - name;
}

static buffer_t *iso_find_entry(inode_t *dir, char *name, char **next, iso_dentry_t **result)
{
    iso_dentry_t *desc = dir->desc;
    iso_dentry_t *entry = NULL;
    buffer_t *buf = NULL;
    int ret = -ERROR;
    int len1 = namelen(name, next);
    int lba = desc->lba;
    char *ptr = NULL;
    int left = desc->size;

    while (left > 0)
    {
        if (!buf || ptr >= buf->data + BLOCK_SIZE)
        {
            brelse(buf);

            buf = bread(dir->dev, lba++, BLOCK_SIZE);
            assert(buf);

            ptr = buf->data;
        }
        entry = (iso_dentry_t *)ptr;
        if (!entry->length)
        {
            left -= BLOCK_SIZE;
            ptr = buf->data + BLOCK_SIZE;
            continue;
        }

        int length = entry->length;
        int len2 = entry->namelen;
        if (iso_match_name(name, len1, entry->name, len2))
        {
            *result = entry;
            return buf;
        }
        // LOGK("find name %s\n", entry->name);
        ptr += length;
    }

rollback:
    brelse(buf);
    return NULL;
}

static u16 iso_make_mode(iso_dentry_t *entry)
{
    u16 mode = IXOTH | IROTH;
    mode |= (IXOTH | IROTH) << 3;
    mode |= (IXOTH | IROTH) << 6;
    if (entry->flags & ISO_FLAG_SUBDIR)
    {
        mode |= IFDIR;
    }
    if (entry->flags == 0)
    {
        mode |= IFREG;
    }
    return mode;
}

static inode_t *iso_make_inode(super_t *super, iso_dentry_t *entry)
{
    task_t *task = running_task();
    inode_t *inode = get_free_inode();

    inode->count++;
    inode->dev = super->dev;
    inode->nr = entry->lba;
    inode->super = super;
    inode->op = fs_get_op(FS_TYPE_ISO9660);
    inode->mtime = inode->ctime = iso_make_datetime(entry->date);
    inode->atime = time();
    inode->mode = iso_make_mode(entry);
    inode->uid = task->uid;
    inode->gid = task->gid;
    inode->size = entry->size;
    inode->type = FS_TYPE_ISO9660;

    list_push(&super->inode_list, &inode->node);
    return inode;
}

static int iso_super(dev_t dev, super_t *super)
{
    buffer_t *buf = NULL;
    int ret = -ERROR;

    buf = bread(dev, 0x10, BLOCK_SIZE);
    if (!buf)
        return -EFSUNK;

    iso_desc_t *desc = (iso_desc_t *)buf->data;
    if (desc->type != ISO_VD_PRIMARY)
    {
        ret = -EFSUNK;
        goto rollback;
    }

    assert(!memcmp(desc->id, "CD001", 5));

    super->desc = desc;
    super->buf = buf;
    super->sector_size = BLOCK_SIZE;
    super->block_size = BLOCK_SIZE;
    super->type = FS_TYPE_ISO9660;
    super->dev = dev;

    inode_t *inode = iso_make_inode(super, &desc->root);
    inode->buf = buf;
    inode->buf->count++;
    inode->desc = &desc->root;
    super->iroot = inode;

    assert(desc->logical_block_size == BLOCK_SIZE);

    // LOGK("PVD struct size %d\n", sizeof(iso_desc_t));
    // LOGK("PVD System ID %s\n", desc->system_id);
    // LOGK("PVD Volume ID %s\n", desc->volume_id);
    // LOGK("PVD Volume Set 0x%x\n", desc->volume_set_size);
    // LOGK("PVD Preparer %s\n", desc->preparer_id);
    // LOGK("PVD Create Date %s\n", desc->create_date);
    // LOGK("PVD Modify Date %s\n", desc->modify_date);
    // LOGK("PVD Expire Date %s\n", desc->expire_date);
    // LOGK("PVD Effect Date %s\n", desc->effect_date);
    return EOK;

rollback:
    brelse(buf);
    return ret;
}

int iso_permission(inode_t *inode, int mask)
{
    u16 mode = 05; // 可读可执行
    if ((mode & mask & 0b111) == mask)
        return true;
    return false;
}

static err_t iso_namei(inode_t *dir, char *name, char **next, inode_t **result)
{
    iso_dentry_t *entry = NULL;

    buffer_t *buf = iso_find_entry(dir, name, next, &entry);
    if (!buf)
    {
        return -ENOENT;
    }

    inode_t *inode = find_inode(buf->dev, entry->lba);
    if (inode)
    {
        inode->count++;
        inode->atime = time();
        *result = fit_inode(inode);
        return EOK;
    }

    inode = iso_make_inode(dir->super, entry);
    inode->desc = entry;
    inode->buf = buf;
    *result = fit_inode(inode);
    return EOK;
}

// 打开 dir 目录的 name 文件
int iso_open(inode_t *dir, char *name, int flags, int mode, inode_t **result)
{
    if (flags != O_RDONLY)
        return -EROFS;

    inode_t *inode = NULL;
    char *next;
    int ret = iso_namei(dir, name, &next, &inode);
    if (ret < 0)
        return ret;

    inode->atime = time();
    *result = inode;
    return EOK;
}

static void iso_close(inode_t *inode)
{
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

// 从 offset 处，读 size 个字节到 data
int iso_read(inode_t *inode, char *data, int size, off_t offset)
{
    assert(inode->type == FS_TYPE_ISO9660);

    iso_dentry_t *entry = (iso_dentry_t *)inode->desc;

    // 如果偏移量超过文件大小，返回 EOF
    if (offset >= entry->size)
    {
        return EOF;
    }

    // 开始读取的位置
    u32 begin = offset;

    // 剩余字节数
    u32 left = MIN(size, entry->size - offset);

    idx_t lba = entry->lba;
    size_t block_size = inode->super->block_size;

    int ret = EOF;
    buffer_t *buf = NULL;

    while (left)
    {
        // 找到对应的文件便宜，所在文件块
        idx_t nr = offset / block_size;

        // 读取文件块缓冲
        buf = bread(inode->dev, lba + nr, block_size);

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

int iso_readdir(inode_t *inode, dentry_t *entry, size_t count, off_t offset)
{
    void *buf = kmalloc(256);
    int ret = EOF;
    iso_dentry_t *desc = inode->desc;
    if (offset >= desc->size)
        goto rollback;

    if ((ret = iso_read(inode, (char *)buf, 256, offset)) < 0)
        goto rollback;

    iso_dentry_t *mentry = buf;
    if (mentry->length == 0)
    {
        ret = EOF;
        goto rollback;
    }

    assert(mentry->length < 256);
    assert(mentry->namelen < MAXNAMELEN);

    entry->length = mentry->length;
    entry->namelen = iso_namelen(mentry->name, mentry->namelen);
    if (entry->namelen == 1 && !memcmp(mentry->name, "\0", 1))
    {
        strcpy(entry->name, ".");
    }
    else if (entry->namelen == 1 && !memcmp(mentry->name, "\x01", 1))
    {
        entry->namelen = 2;
        strcpy(entry->name, "..");
    }
    else
    {
        memcpy(entry->name, mentry->name, entry->namelen);
        entry->name[entry->namelen] = 0;
    }

    entry->nr = mentry->lba;
    ret = entry->length;

rollback:
    kfree(buf);
    return ret;
}

int iso_stat(inode_t *inode, stat_t *stat)
{
    iso_dentry_t *entry = inode->desc;
    task_t *task = running_task();

    stat->dev = inode->dev;     // 文件所在的设备号
    stat->nr = inode->nr;       // 文件 i 节点号
    stat->mode = inode->mode;   // 文件属性
    stat->nlinks = 1;           // 文件的连接数
    stat->uid = task->uid;      // 文件的用户 id
    stat->gid = task->gid;      // 文件的组 id
    stat->rdev = 0;             // 设备号(如果文件是特殊的字符文件或块文件)
    stat->size = entry->size;   // 文件大小（字节数）（如果文件是常规文件）
    stat->atime = inode->atime; // 最后访问时间
    stat->mtime = inode->mtime; // 最后修改时间
    stat->ctime = inode->ctime; // 最后节点修改时间
    return 0;
}

static fs_op_t iso_op = {
    fs_default_nosys,

    iso_super,

    iso_open,
    iso_close,

    fs_default_nosys,
    iso_read,
    fs_default_nosys,
    fs_default_nosys,

    iso_stat,
    iso_permission,

    iso_namei,
    fs_default_nosys,
    fs_default_nosys,
    fs_default_nosys,
    fs_default_nosys,
    fs_default_nosys,
    iso_readdir,
};

void iso_init()
{
    fs_register_op(FS_TYPE_ISO9660, &iso_op);
}
