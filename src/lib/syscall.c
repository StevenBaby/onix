#include <onix/syscall.h>
#include <onix/signal.h>

static _inline u32 _syscall0(u32 nr)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr));
    return ret;
}

static _inline u32 _syscall1(u32 nr, u32 arg)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg));
    return ret;
}

static _inline u32 _syscall2(u32 nr, u32 arg1, u32 arg2)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2));
    return ret;
}

static _inline u32 _syscall3(u32 nr, u32 arg1, u32 arg2, u32 arg3)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3));
    return ret;
}

static _inline u32 _syscall4(u32 nr, u32 arg1, u32 arg2, u32 arg3, u32 arg4)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4));
    return ret;
}

static _inline u32 _syscall5(u32 nr, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5));
    return ret;
}

static _inline u32 _syscall6(u32 nr, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5, u32 arg6)
{
    u32 ret;
    asm volatile(
        "pushl %%ebp\n"
        "movl %7, %%ebp\n"
        "int $0x80\n"
        "popl %%ebp\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5), "m"(arg6));
    return ret;
}

u32 test()
{
    return _syscall0(SYS_NR_TEST);
}

void exit(int status)
{
    _syscall1(SYS_NR_EXIT, (u32)status);
}

pid_t fork()
{
    return _syscall0(SYS_NR_FORK);
}

pid_t waitpid(pid_t pid, int32 *status)
{
    return _syscall2(SYS_NR_WAITPID, pid, (u32)status);
}

int execve(char *filename, char *argv[], char *envp[])
{
    return _syscall3(SYS_NR_EXECVE, (u32)filename, (u32)argv, (u32)envp);
}

int kill(pid_t pid, int signal)
{
    return _syscall2(SYS_NR_KILL, pid, signal);
}

int alarm(int sec)
{
    return _syscall1(SYS_NR_ALARM, sec);
}

void yield()
{
    _syscall0(SYS_NR_YIELD);
}

void sleep(u32 ms)
{
    _syscall1(SYS_NR_SLEEP, ms);
}

pid_t getpid()
{
    return _syscall0(SYS_NR_GETPID);
}

pid_t getppid()
{
    return _syscall0(SYS_NR_GETPPID);
}

int setpgid(int pid, int pgid)
{
    return _syscall2(SYS_NR_SETPGID, pid, pgid);
}

pid_t setpgrp()
{
    setpgid(0, 0);
}

int getpgrp()
{
    return _syscall0(SYS_NR_GETPGRP);
}

int setsid()
{
    return _syscall0(SYS_NR_SETSID);
}

int stty()
{
    return _syscall0(SYS_NR_STTY);
}

int gtty()
{
    return _syscall0(SYS_NR_GTTY);
}

int ioctl(fd_t fd, int cmd, int args)
{
    return _syscall3(SYS_NR_IOCTL, fd, cmd, args);
}

int32 brk(void *addr)
{
    return _syscall1(SYS_NR_BRK, (u32)addr);
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    return (void *)_syscall6(SYS_NR_MMAP, (u32)addr, length, prot, flags, fd, offset);
}

int munmap(void *addr, size_t length)
{
    return _syscall2(SYS_NR_MUNMAP, (u32)addr, length);
}

fd_t dup(fd_t oldfd)
{
    return _syscall1(SYS_NR_DUP, oldfd);
}

fd_t dup2(fd_t oldfd, fd_t newfd)
{
    return _syscall2(SYS_NR_DUP2, oldfd, newfd);
}

int pipe(fd_t pipefd[2])
{
    return _syscall1(SYS_NR_PIPE, (u32)pipefd);
}

fd_t open(char *filename, int flags, int mode)
{
    return _syscall3(SYS_NR_OPEN, (u32)filename, (u32)flags, (u32)mode);
}

fd_t creat(char *filename, int mode)
{
    return _syscall2(SYS_NR_CREAT, (u32)filename, (u32)mode);
}

void close(fd_t fd)
{
    _syscall1(SYS_NR_CLOSE, (u32)fd);
}

int read(fd_t fd, char *buf, int len)
{
    return _syscall3(SYS_NR_READ, fd, (u32)buf, len);
}

int write(fd_t fd, char *buf, int len)
{
    return _syscall3(SYS_NR_WRITE, fd, (u32)buf, len);
}

int lseek(fd_t fd, off_t offset, int whence)
{
    return _syscall3(SYS_NR_LSEEK, fd, offset, whence);
}

int readdir(fd_t fd, void *dir, int count)
{
    return _syscall3(SYS_NR_READDIR, fd, (u32)dir, (u32)count);
}

char *getcwd(char *buf, size_t size)
{
    return (char *)_syscall2(SYS_NR_GETCWD, (u32)buf, (u32)size);
}

int chdir(char *pathname)
{
    return _syscall1(SYS_NR_CHDIR, (u32)pathname);
}

int chroot(char *pathname)
{
    return _syscall1(SYS_NR_CHROOT, (u32)pathname);
}

int mkdir(char *pathname, int mode)
{
    return _syscall2(SYS_NR_MKDIR, (u32)pathname, (u32)mode);
}

int rmdir(char *pathname)
{
    return _syscall1(SYS_NR_RMDIR, (u32)pathname);
}

int link(char *oldname, char *newname)
{
    return _syscall2(SYS_NR_LINK, (u32)oldname, (u32)newname);
}

int unlink(char *filename)
{
    return _syscall1(SYS_NR_UNLINK, (u32)filename);
}

int mount(char *devname, char *dirname, int flags)
{
    return _syscall3(SYS_NR_MOUNT, (u32)devname, (u32)dirname, (u32)flags);
}

int umount(char *target)
{
    return _syscall1(SYS_NR_UMOUNT, (u32)target);
}

int mknod(char *filename, int mode, int dev)
{
    return _syscall3(SYS_NR_MKNOD, (u32)filename, (u32)mode, (u32)dev);
}

time_t time()
{
    return _syscall0(SYS_NR_TIME);
}

mode_t umask(mode_t mask)
{
    return _syscall1(SYS_NR_UMASK, (u32)mask);
}

int stat(char *filename, stat_t *statbuf)
{
    return _syscall2(SYS_NR_STAT, (u32)filename, (u32)statbuf);
}

int fstat(fd_t fd, stat_t *statbuf)
{
    return _syscall2(SYS_NR_FSTAT, (u32)fd, (u32)statbuf);
}

int mkfs(char *devname, int icount)
{
    return _syscall2(SYS_NR_MKFS, (u32)devname, (u32)icount);
}

int sgetmask()
{
    return _syscall0(SYS_NR_SGETMASK);
}

int ssetmask(int newmask)
{
    return _syscall1(SYS_NR_SSETMASK, (u32)newmask);
}

extern void restorer();

int signal(int sig, int handler)
{
    return _syscall3(SYS_NR_SIGNAL, (u32)sig, (u32)handler, (u32)restorer);
}

int sigaction(int sig, sigaction_t *action, sigaction_t *oldaction)
{
    return _syscall3(SYS_NR_SIGACTION, (u32)sig, (u32)action, (u32)oldaction);
}

int socket(int domain, int type, int protocol)
{
    return _syscall3(SYS_NR_SOCKET, domain, type, protocol);
}

int listen(int fd, int backlog)
{
    return _syscall2(SYS_NR_LISTEN, fd, backlog);
}

int accept(int fd, sockaddr_t *addr, int *addrlen)
{
    return _syscall3(SYS_NR_ACCEPT, fd, (u32)addr, (u32)addrlen);
}

int bind(int fd, const sockaddr_t *name, int namelen)
{
    return _syscall3(SYS_NR_BIND, fd, (u32)name, namelen);
}

int connect(int fd, const sockaddr_t *name, int namelen)
{
    return _syscall3(SYS_NR_CONNECT, fd, (u32)name, namelen);
}

int shutdown(int fd, int how)
{
    return _syscall2(SYS_NR_SHUTDOWN, fd, how);
}

int getpeername(int fd, sockaddr_t *name, int *namelen)
{
    return _syscall3(SYS_NR_GETPEERNAME, fd, (u32)name, (u32)namelen);
}

int getsockname(int fd, sockaddr_t *name, int *namelen)
{
    return _syscall3(SYS_NR_GETSOCKNAME, fd, (u32)name, (u32)namelen);
}

int getsockopt(int fd, int level, int optname, void *optval, int *optlen)
{
    return _syscall5(SYS_NR_GETSOCKOPT, fd, level, optname, (u32)optval, (u32)optlen);
}

int setsockopt(int fd, int level, int optname, const void *optval, int optlen)
{
    return _syscall5(SYS_NR_SETSOCKOPT, fd, level, optname, (u32)optval, (u32)optlen);
}

int recv(int fd, void *data, int size, u32 flags)
{
    return recvfrom(fd, data, size, 0, NULL, 0);
}

int recvfrom(int fd, void *data, int size, u32 flags, sockaddr_t *from, int *fromlen)
{
    return _syscall6(SYS_NR_RECVFROM, fd, (u32)data, size, flags, (u32)from, (u32)fromlen);
}

int recvmsg(int fd, msghdr_t *msg, u32 flags)
{
    return _syscall3(SYS_NR_RECVMSG, fd, (u32)msg, flags);
}

int send(int fd, const void *data, int size, u32 flags)
{
    return sendto(fd, data, size, 0, NULL, 0);
}

int sendto(int fd, const void *data, int size, u32 flags, const sockaddr_t *to, int tolen)
{
    return _syscall6(SYS_NR_SENDTO, fd, (u32)data, size, flags, (u32)to, tolen);
}

int sendmsg(int fd, msghdr_t *msg, u32 flags)
{
    return _syscall3(SYS_NR_SENDMSG, fd, (u32)msg, flags);
}

int resolv(const char *name, ip_addr_t addr)
{
    return _syscall2(SYS_NR_RESOLV, (u32)name, (u32)addr);
}

int uname(void *buf)
{
    return _syscall1(SYS_NR_UNAME, (u32)buf);
}
