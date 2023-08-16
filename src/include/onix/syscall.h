#ifndef ONIX_SYSCALL_H
#define ONIX_SYSCALL_H

#include <onix/types.h>
#include <onix/stat.h>
#include <onix/net/socket.h>

#define SYSCALL_SIZE 512

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
    SYS_NR_EXECVE = 11,
    SYS_NR_CHDIR = 12,
    SYS_NR_TIME = 13,
    SYS_NR_MKNOD = 14,
    SYS_NR_STAT = 18,
    SYS_NR_LSEEK = 19,
    SYS_NR_GETPID = 20,
    SYS_NR_MOUNT = 21,
    SYS_NR_UMOUNT = 22,
    SYS_NR_ALARM = 27,
    SYS_NR_FSTAT = 28,
    SYS_NR_STTY = 31,
    SYS_NR_GTTY = 32,
    SYS_NR_KILL = 37,
    SYS_NR_MKDIR = 39,
    SYS_NR_RMDIR = 40,
    SYS_NR_DUP = 41,
    SYS_NR_PIPE = 42,
    SYS_NR_BRK = 45,
    SYS_NR_SIGNAL = 48,
    SYS_NR_IOCTL = 54,
    SYS_NR_SETPGID = 57,
    SYS_NR_UNAME = 59,
    SYS_NR_UMASK = 60,
    SYS_NR_CHROOT = 61,
    SYS_NR_DUP2 = 63,
    SYS_NR_GETPPID = 64,
    SYS_NR_GETPGRP = 65,
    SYS_NR_SETSID = 66,
    SYS_NR_SIGACTION = 67,
    SYS_NR_SGETMASK = 68,
    SYS_NR_SSETMASK = 69,
    SYS_NR_READDIR = 89,
    SYS_NR_MMAP = 90,
    SYS_NR_MUNMAP = 91,
    SYS_NR_YIELD = 158,
    SYS_NR_SLEEP = 162,
    SYS_NR_GETCWD = 183,

    SYS_NR_SOCKET = 359,
    SYS_NR_BIND = 361,
    SYS_NR_CONNECT,
    SYS_NR_LISTEN,
    SYS_NR_ACCEPT,
    SYS_NR_GETSOCKOPT,
    SYS_NR_SETSOCKOPT,
    SYS_NR_GETSOCKNAME,
    SYS_NR_GETPEERNAME,
    SYS_NR_SENDTO,
    SYS_NR_SENDMSG,
    SYS_NR_RECVFROM,
    SYS_NR_RECVMSG,
    SYS_NR_SHUTDOWN,
    SYS_NR_RESOLV,

    SYS_NR_MKFS = SYSCALL_SIZE - 1,
} syscall_t;

#if 0
#include <sys/mman.h>
#endif

enum mmap_type_t
{
    PROT_NONE = 0,
    PROT_READ = 1,
    PROT_WRITE = 2,
    PROT_EXEC = 4,

    MAP_SHARED = 1,
    MAP_PRIVATE = 2,
    MAP_FIXED = 0x10,
};

u32 test();

pid_t fork();
void exit(int status);
pid_t waitpid(pid_t pid, int32 *status);

// 执行程序
int execve(char *filename, char *argv[], char *envp[]);

void yield();
void sleep(u32 ms);

pid_t getpid();
pid_t getppid();

// 设置进程组
pid_t setpgrp();
int setpgid(int pid, int pgid);
// 获得进程组
pid_t getpgrp();
// 设置会话
pid_t setsid();

// 终端设置
int stty();
// 获取终端设置信息
int gtty();

// 操作 IO 设备
int ioctl(fd_t fd, int cmd, int args);

int brk(void *addr);
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);

// 打开文件
fd_t open(char *filename, int flags, int mode);
// 创建普通文件
fd_t creat(char *filename, int mode);
// 关闭文件
void close(fd_t fd);

// 复制文件描述符
fd_t dup(fd_t oldfd);
fd_t dup2(fd_t oldfd, fd_t newfd);

// 创建管道
int pipe(fd_t pipefd[2]);

// 读文件
int read(fd_t fd, char *buf, int len);
// 写文件
int write(fd_t fd, char *buf, int len);
// 设置文件偏移量
int lseek(fd_t fd, off_t offset, int whence);
// 读取目录
int readdir(fd_t fd, void *dir, int count);

// 获取当前路径
char *getcwd(char *buf, size_t size);
// 切换当前目录
int chdir(char *pathname);
// 切换根目录
int chroot(char *pathname);

// 创建目录
int mkdir(char *pathname, int mode);
// 删除目录
int rmdir(char *pathname);

// 创建硬链接
int link(char *oldname, char *newname);
// 删除硬链接（删除文件）
int unlink(char *filename);

// 挂载设备
int mount(char *devname, char *dirname, int flags);
// 卸载设备
int umount(char *target);

// 创建设备文件
int mknod(char *filename, int mode, int dev);

time_t time();

mode_t umask(mode_t mask);

// 获取文件状态
int stat(char *filename, stat_t *statbuf);
int fstat(fd_t fd, stat_t *statbuf);

// 格式化文件系统
int mkfs(char *devname, int icount);

int stty();
int gtty();

// 发送信号
int kill(pid_t pid, int signal);

// 设置闹钟
int alarm(int sec);

// 套接字
int socket(int domain, int type, int protocol);
int listen(int fd, int backlog);
int accept(int fd, sockaddr_t *addr, int *addrlen);
int bind(int fd, const sockaddr_t *name, int namelen);
int connect(int fd, const sockaddr_t *name, int namelen);
int shutdown(int fd, int how);

int getpeername(int fd, sockaddr_t *name, int *namelen);
int getsockname(int fd, sockaddr_t *name, int *namelen);
int getsockopt(int fd, int level, int optname, void *optval, int *optlen);
int setsockopt(int fd, int level, int optname, const void *optval, int optlen);

int recv(int fd, void *data, int size, u32 flags);
int recvfrom(int fd, void *data, int size, u32 flags, sockaddr_t *from, int *fromlen);
int recvmsg(int fd, msghdr_t *msg, u32 flags);

int send(int fd, const void *data, int size, u32 flags);
int sendto(int fd, const void *data, int size, u32 flags, const sockaddr_t *to, int tolen);
int sendmsg(int fd, msghdr_t *msg, u32 flags);

int resolv(const char *name, ip_addr_t addr);

int uname(void *buf);

#endif
