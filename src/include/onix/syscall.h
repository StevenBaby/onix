#ifndef ONIX_SYSCALL_H
#define ONIX_SYSCALL_H

#include <onix/types.h>

#if 0
#include <asm/unistd_32.h>
#endif

typedef enum syscall_t
{
    SYS_NR_TEST,
    SYS_NR_EXIT = 1,
    SYS_NR_FORK = 2,
    SYS_NR_READ = 3,
    SYS_NR_WRITE = 4,
    SYS_NR_OPEN = 5,
    SYS_NR_CLOSE = 6,
    SYS_NR_WAITPID = 7,
    SYS_NR_CREAT = 8,
    SYS_NR_LINK = 9,
    SYS_NR_UNLINK = 10,
    SYS_NR_TIME = 13,
    SYS_NR_LSEEK = 19,
    SYS_NR_GETPID = 20,
    SYS_NR_MKDIR = 39,
    SYS_NR_RMDIR = 40,
    SYS_NR_BRK = 45,
    SYS_NR_UMASK = 60,
    SYS_NR_GETPPID = 64,
    SYS_NR_YIELD = 158,
    SYS_NR_SLEEP = 162,
} syscall_t;

u32 test();

pid_t fork();
void exit(int status);
pid_t waitpid(pid_t pid, int32 *status);

void yield();
void sleep(u32 ms);

pid_t getpid();
pid_t getppid();

int32 brk(void *addr);

// 打开文件
fd_t open(char *filename, int flags, int mode);
// 创建普通文件
fd_t creat(char *filename, int mode);
// 关闭文件
void close(fd_t fd);

// 读文件
int read(fd_t fd, char *buf, int len);
// 写文件
int write(fd_t fd, char *buf, int len);
// 设置文件偏移量
int lseek(fd_t fd, off_t offset, int whence);

// 创建目录
int mkdir(char *pathname, int mode);
// 删除目录
int rmdir(char *pathname);

// 创建硬链接
int link(char *oldname, char *newname);
// 删除硬链接（删除文件）
int unlink(char *filename);

time_t time();

mode_t umask(mode_t mask);

#endif