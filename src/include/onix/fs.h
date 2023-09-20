#ifndef ONIX_FS_H
#define ONIX_FS_H

#include <onix/types.h>
#include <onix/list.h>
#include <onix/stat.h>

#define MAXNAMELEN 64

#define SEPARATOR1 '/'                                       // 目录分隔符 1
#define SEPARATOR2 '\\'                                      // 目录分隔符 2
#define IS_SEPARATOR(c) (c == SEPARATOR1 || c == SEPARATOR2) // 字符是否位目录分隔符

enum file_flag
{
    O_RDONLY = 00,      // 只读方式
    O_WRONLY = 01,      // 只写方式
    O_RDWR = 02,        // 读写方式
    O_ACCMODE = 03,     // 文件访问模式屏蔽码
    O_CREAT = 00100,    // 如果文件不存在就创建
    O_EXCL = 00200,     // 独占使用文件标志
    O_NOCTTY = 00400,   // 不分配控制终端
    O_TRUNC = 01000,    // 若文件已存在且是写操作，则长度截为 0
    O_APPEND = 02000,   // 以添加方式打开，文件指针置为文件尾
    O_NONBLOCK = 04000, // 非阻塞方式打开和操作文件
};

enum
{
    FS_TYPE_NONE = 0,
    FS_TYPE_PIPE,
    FS_TYPE_SOCKET,
    FS_TYPE_MINIX,
    FS_TYPE_ISO9660,
    FS_TYPE_NUM,
};

typedef struct inode_t
{
    list_node_t node; // 链表结点

    void *desc; // inode 描述符

    union
    {
        struct buffer_t *buf; // inode 描述符对应 buffer
        void *addr;           // pipe 缓冲地址
    };

    dev_t dev;  // 设备号
    dev_t rdev; // 虚拟设备号

    idx_t nr;     // i 节点号
    size_t count; // 引用计数

    time_t atime; // 访问时间
    time_t mtime; // 修改时间
    time_t ctime; // 创建时间

    dev_t mount; // 安装设备

    mode_t mode; // 文件模式
    size_t size; // 文件大小
    int type;    // 文件系统类型

    int uid; // 用户 id
    int gid; // 组 id

    struct super_t *super;   // 超级块
    struct fs_op_t *op;      // 文件系统操作
    struct task_t *rxwaiter; // 读等待进程
    struct task_t *txwaiter; // 写等待进程
} inode_t;

typedef struct super_t
{
    void *desc;           // 超级块描述符
    struct buffer_t *buf; // 超级块描述符 buffer
    dev_t dev;            // 设备号
    u32 count;            // 引用计数
    int type;             // 文件系统类型
    size_t sector_size;   // 扇区大小
    size_t block_size;    // 块大小
    list_t inode_list;    // 使用中 inode 链表
    inode_t *iroot;       // 根目录 inode
    inode_t *imount;      // 安装到的 inode
} super_t;

// 目录结构
typedef struct dentry_t
{
    idx_t nr;    // inode
    u32 length;  // 目录长度
    u32 namelen; // 文件名长度
    char name[MAXNAMELEN];
} dentry_t;

typedef struct file_t
{
    inode_t *inode; // 文件 inode
    u32 count;      // 引用计数
    off_t offset;   // 文件偏移
    int flags;      // 文件标记
} file_t;

typedef dentry_t dirent_t;

typedef enum whence_t
{
    SEEK_SET = 1, // 直接设置偏移
    SEEK_CUR,     // 当前位置偏移
    SEEK_END      // 结束位置偏移
} whence_t;

typedef struct fs_op_t
{
    int (*mkfs)(dev_t dev, int args);

    int (*super)(dev_t dev, super_t *super);

    int (*open)(inode_t *dir, char *name, int flags, int mode, inode_t **result);
    void (*close)(inode_t *inode);

    int (*ioctl)(inode_t *inode, int cmd, void *args);
    int (*read)(inode_t *inode, char *data, int len, off_t offset);
    int (*write)(inode_t *inode, char *data, int len, off_t offset);
    int (*truncate)(inode_t *inode);

    int (*stat)(inode_t *inode, stat_t *stat);
    int (*permission)(inode_t *inode, int mask);

    int (*namei)(inode_t *dir, char *name, char **next, inode_t **result);
    int (*mkdir)(inode_t *dir, char *name, int mode);
    int (*rmdir)(inode_t *dir, char *name);
    int (*link)(inode_t *odir, char *oldname, inode_t *ndir, char *newname);
    int (*unlink)(inode_t *dir, char *name);
    int (*mknod)(inode_t *dir, char *name, int mode, int dev);
    int (*readdir)(inode_t *inode, dentry_t *entry, size_t count, off_t offset);
} fs_op_t;

err_t fd_check(fd_t fd, file_t **file);
fd_t fd_get(file_t **file);
err_t fd_put(fd_t fd);

int fs_default_nosys();  // 未实现的系统调用
void *fs_default_null(); // 返回空指针

super_t *get_super(dev_t dev);  // 获得 dev 对应的超级块
super_t *read_super(dev_t dev); // 读取 dev 对应的超级块
void put_super(super_t *sb);

inode_t *get_root_inode(); // 获取根目录 inode
void iput(inode_t *inode); // 释放 inode
inode_t *find_inode(dev_t dev, idx_t nr);
inode_t *fit_inode(inode_t *inode);

super_t *get_free_super();

inode_t *named(char *pathname, char **next); // 获取 pathname 对应的父目录 inode
inode_t *namei(char *pathname);              // 获取 pathname 对应的 inode

fs_op_t *fs_get_op(int type);
void fs_register_op(int type, fs_op_t *op);

inode_t *get_free_inode();
void put_free_inode(inode_t *inode);

file_t *get_file();
void put_file(file_t *file);

#define P_EXEC IXOTH
#define P_READ IROTH
#define P_WRITE IWOTH

bool match_name(const char *name, const char *entry_name, char **next, int count);

#endif
