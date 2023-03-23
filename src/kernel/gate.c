#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/syscall.h>
#include <onix/task.h>
#include <onix/console.h>
#include <onix/memory.h>
#include <onix/device.h>
#include <onix/string.h>
#include <onix/buffer.h>
#include <onix/fs.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SYSCALL_SIZE 256

handler_t syscall_table[SYSCALL_SIZE];

void syscall_check(u32 nr)
{
    if (nr >= SYSCALL_SIZE)
    {
        panic("syscall nr error!!!");
    }
}

static void sys_default()
{
    panic("syscall not implemented!!!");
}

static u32 sys_test()
{
    char ch;
    device_t *device;

    device_t *serial = device_find(DEV_SERIAL, 0);
    assert(serial);

    device_t *keyboard = device_find(DEV_KEYBOARD, 0);
    assert(keyboard);

    device_t *console = device_find(DEV_CONSOLE, 0);
    assert(console);

    device_read(serial->dev, &ch, 1, 0, 0);
    // device_read(keyboard->dev, &ch, 1, 0, 0);

    device_write(serial->dev, &ch, 1, 0, 0);
    device_write(console->dev, &ch, 1, 0, 0);

    return 255;
}

extern void sys_execve();
extern int sys_kill();

extern fd_t sys_dup();
extern fd_t sys_dup2();

extern int sys_pipe();

extern int sys_read();
extern int sys_write();
extern int sys_lseek();
extern int sys_readdir();

extern fd_t sys_open();
extern fd_t sys_creat();
extern void sys_close();

extern int sys_chdir();
extern int sys_chroot();
extern char *sys_getcwd();

extern int sys_mkdir();
extern int sys_rmdir();

extern int sys_link();
extern int sys_unlink();

extern time_t sys_time();
extern mode_t sys_umask();

extern int sys_stat();
extern int sys_fstat();

extern int sys_mknod();

extern int sys_mount();
extern int sys_umount();

extern int sys_brk();
extern int sys_mmap();
extern int sys_munmap();

extern int sys_setpgid();
extern int sys_setsid();
extern int sys_getpgrp();

extern int sys_stty();
extern int sys_gtty();
extern int sys_ioctl();

extern int sys_signal();
extern int sys_sgetmask();
extern int sys_ssetmask();
extern int sys_sigaction();

extern int sys_mkfs();

void syscall_init()
{
    for (size_t i = 0; i < SYSCALL_SIZE; i++)
    {
        syscall_table[i] = sys_default;
    }

    syscall_table[SYS_NR_TEST] = sys_test;

    syscall_table[SYS_NR_EXIT] = task_exit;
    syscall_table[SYS_NR_FORK] = task_fork;
    syscall_table[SYS_NR_WAITPID] = task_waitpid;
    syscall_table[SYS_NR_KILL] = sys_kill;

    syscall_table[SYS_NR_EXECVE] = sys_execve;

    syscall_table[SYS_NR_SLEEP] = task_sleep;
    syscall_table[SYS_NR_YIELD] = task_yield;

    syscall_table[SYS_NR_GETPID] = sys_getpid;
    syscall_table[SYS_NR_GETPPID] = sys_getppid;
    syscall_table[SYS_NR_SETPGID] = sys_setpgid;
    syscall_table[SYS_NR_GETPGRP] = sys_getpgrp;
    syscall_table[SYS_NR_SETSID] = sys_setsid;

    syscall_table[SYS_NR_STTY] = sys_stty;
    syscall_table[SYS_NR_GTTY] = sys_gtty;
    syscall_table[SYS_NR_IOCTL] = sys_ioctl;

    syscall_table[SYS_NR_BRK] = sys_brk;
    syscall_table[SYS_NR_MMAP] = sys_mmap;
    syscall_table[SYS_NR_MUNMAP] = sys_munmap;

    syscall_table[SYS_NR_DUP] = sys_dup;
    syscall_table[SYS_NR_DUP2] = sys_dup2;

    syscall_table[SYS_NR_PIPE] = sys_pipe;

    syscall_table[SYS_NR_READ] = sys_read;
    syscall_table[SYS_NR_WRITE] = sys_write;
    syscall_table[SYS_NR_LSEEK] = sys_lseek;
    syscall_table[SYS_NR_READDIR] = sys_readdir;

    syscall_table[SYS_NR_MKDIR] = sys_mkdir;
    syscall_table[SYS_NR_RMDIR] = sys_rmdir;

    syscall_table[SYS_NR_OPEN] = sys_open;
    syscall_table[SYS_NR_CREAT] = sys_creat;
    syscall_table[SYS_NR_CLOSE] = sys_close;

    syscall_table[SYS_NR_LINK] = sys_link;
    syscall_table[SYS_NR_UNLINK] = sys_unlink;

    syscall_table[SYS_NR_TIME] = sys_time;

    syscall_table[SYS_NR_UMASK] = sys_umask;

    syscall_table[SYS_NR_CHDIR] = sys_chdir;
    syscall_table[SYS_NR_CHROOT] = sys_chroot;
    syscall_table[SYS_NR_GETCWD] = sys_getcwd;

    syscall_table[SYS_NR_STAT] = sys_stat;
    syscall_table[SYS_NR_FSTAT] = sys_fstat;

    syscall_table[SYS_NR_MKNOD] = sys_mknod;

    syscall_table[SYS_NR_MOUNT] = sys_mount;
    syscall_table[SYS_NR_UMOUNT] = sys_umount;

    syscall_table[SYS_NR_MKFS] = sys_mkfs;

    syscall_table[SYS_NR_SIGNAL] = sys_signal;
    syscall_table[SYS_NR_SGETMASK] = sys_sgetmask;
    syscall_table[SYS_NR_SSETMASK] = sys_ssetmask;
    syscall_table[SYS_NR_SIGACTION] = sys_sigaction;
}
