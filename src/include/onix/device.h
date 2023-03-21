#ifndef ONIX_DEVICE_H
#define ONIX_DEVICE_H

#include <onix/types.h>
#include <onix/list.h>

#define DEVICE_NR 64 // 设备数量
#define NAMELEN 16

// 设备类型
enum device_type_t
{
    DEV_NULL,  // 空设备
    DEV_CHAR,  // 字符设备
    DEV_BLOCK, // 块设备
};

// 设备子类型
enum device_subtype_t
{
    DEV_CONSOLE = 1, // 控制台
    DEV_KEYBOARD,    // 键盘
    DEV_SERIAL,      // 串口
    DEV_TTY,         // TTY 设备
    DEV_IDE_DISK,    // IDE 磁盘
    DEV_IDE_PART,    // IDE 磁盘分区
    DEV_RAMDISK,     // 虚拟磁盘
};

// 设备控制命令
enum device_cmd_t
{
    DEV_CMD_SECTOR_START = 1, // 获得设备扇区开始位置 lba
    DEV_CMD_SECTOR_COUNT,     // 获得设备扇区数量
};

#define REQ_READ 0  // 块设备读
#define REQ_WRITE 1 // 块设备写

#define DIRECT_UP 0   // 上楼
#define DIRECT_DOWN 1 // 下楼

// 块设备请求
typedef struct request_t
{
    dev_t dev;           // 设备号
    u32 type;            // 请求类型
    u32 idx;             // 扇区位置
    u32 count;           // 扇区数量
    int flags;           // 特殊标志
    u8 *buf;             // 缓冲区
    struct task_t *task; // 请求进程
    list_node_t node;    // 列表节点
} request_t;

typedef struct device_t
{
    char name[NAMELEN];  // 设备名
    int type;            // 设备类型
    int subtype;         // 设备子类型
    dev_t dev;           // 设备号
    dev_t parent;        // 父设备号
    void *ptr;           // 设备指针
    list_t request_list; // 块设备请求链表
    bool direct;         // 磁盘寻道方向

    // 设备控制
    int (*ioctl)(void *dev, int cmd, void *args, int flags);
    // 读设备
    int (*read)(void *dev, void *buf, size_t count, idx_t idx, int flags);
    // 写设备
    int (*write)(void *dev, void *buf, size_t count, idx_t idx, int flags);
} device_t;

// 安装设备
dev_t device_install(
    int type, int subtype,
    void *ptr, char *name, dev_t parent,
    void *ioctl, void *read, void *write);

// 根据子类型查找设备
device_t *device_find(int type, idx_t idx);

// 根据设备号查找设备
device_t *device_get(dev_t dev);

// 控制设备
int device_ioctl(dev_t dev, int cmd, void *args, int flags);

// 读设备
int device_read(dev_t dev, void *buf, size_t count, idx_t idx, int flags);

// 写设备
int device_write(dev_t dev, void *buf, size_t count, idx_t idx, int flags);

// 块设备请求
void device_request(dev_t dev, void *buf, u8 count, idx_t idx, int flags, u32 type);

#endif