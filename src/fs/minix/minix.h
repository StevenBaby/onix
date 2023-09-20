#ifndef ONIX_MINIX_H
#define ONIX_MINIX_H

#include <onix/fs.h>
#include <onix/buffer.h>

#define BLOCK_SIZE 1024                       // 块大小
#define SECTOR_SIZE 512                       // 扇区大小
#define BLOCK_SECS (BLOCK_SIZE / SECTOR_SIZE) // 一块占 2 个扇区

#define MINIX1_MAGIC 0x137F // 文件系统魔数
#define MINIX1_NAME_LEN 14  // 文件名长度

#define BLOCK_BITS (BLOCK_SIZE * 8)                          // 块位图大小
#define BLOCK_INODES (BLOCK_SIZE / sizeof(minix_inode_t))    // 块 inode 数量
#define BLOCK_DENTRIES (BLOCK_SIZE / sizeof(minix_dentry_t)) // 块 dentry 数量
#define BLOCK_INDEXES (BLOCK_SIZE / sizeof(u16))             // 块索引数量

#define DIRECT_BLOCK (7)                                               // 直接块数量
#define INDIRECT1_BLOCK BLOCK_INDEXES                                  // 一级间接块数量
#define INDIRECT2_BLOCK (INDIRECT1_BLOCK * INDIRECT1_BLOCK)            // 二级间接块数量
#define TOTAL_BLOCK (DIRECT_BLOCK + INDIRECT1_BLOCK + INDIRECT2_BLOCK) // 全部块数量

#define P_EXEC IXOTH
#define P_READ IROTH
#define P_WRITE IWOTH

#define ACC_MODE(x) ("\004\002\006\377"[(x) & O_ACCMODE])

typedef struct minix_super_t
{
    u16 inodes;        // 节点数
    u16 zones;         // 逻辑块数
    u16 imap_blocks;   // i 节点位图所占用的数据块数
    u16 zmap_blocks;   // 逻辑块位图所占用的数据块数
    u16 firstdatazone; // 第一个数据逻辑块号
    u16 log_zone_size; // log2(每逻辑块数据块数)
    u32 max_size;      // 文件最大长度
    u16 magic;         // 文件系统魔数
} minix_super_t;

typedef struct minix_inode_t
{
    u16 mode;    // 文件类型和属性(rwx 位)
    u16 uid;     // 用户id（文件拥有者标识符）
    u32 size;    // 文件大小（字节数）
    u32 mtime;   // 修改时间戳 这个时间戳应该用 UTC 时间，不然有瑕疵
    u8 gid;      // 组id(文件拥有者所在的组)
    u8 nlinks;   // 链接数（多少个文件目录项指向该i 节点）
    u16 zone[9]; // 直接 (0-6)、间接(7)或双重间接 (8) 逻辑块号
} minix_inode_t;

// 文件目录项结构
typedef struct minix_dentry_t
{
    u16 nr;                    // i 节点
    char name[MINIX1_NAME_LEN]; // 文件名
} minix_dentry_t;

#endif