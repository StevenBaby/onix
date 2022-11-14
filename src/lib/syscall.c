#include <onix/syscall.h>

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

int32 brk(void *addr)
{
    return _syscall1(SYS_NR_BRK, (u32)addr);
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

time_t time()
{
    return _syscall0(SYS_NR_TIME);
}

mode_t umask(mode_t mask)
{
    return _syscall1(SYS_NR_UMASK, (u32)mask);
}