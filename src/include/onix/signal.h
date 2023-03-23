#ifndef ONIX_SIGNAL_H
#define ONIX_SIGNAL_H

#include <onix/types.h>

enum SIGNAL
{
    SIGHUP = 1,   // 挂断控制终端或进程
    SIGINT,       // 来自键盘的中断
    SIGQUIT,      // 来自键盘的退出
    SIGILL,       // 非法指令
    SIGTRAP,      // 跟踪断点
    SIGABRT,      // 异常结束
    SIGIOT = 6,   // 异常结束
    SIGUNUSED,    // 没有使用
    SIGFPE,       // 协处理器出错
    SIGKILL = 9,  // 强迫进程终止
    SIGUSR1,      // 用户信号 1，进程可使用
    SIGSEGV,      // 无效内存引用
    SIGUSR2,      // 用户信号 2，进程可使用
    SIGPIPE,      // 管道写出错，无读者
    SIGALRM,      // 实时定时器报警
    SIGTERM = 15, // 进程终止
    SIGSTKFLT,    // 栈出错（协处理器）
    SIGCHLD,      // 子进程停止或被终止
    SIGCONT,      // 恢复进程继续执行
    SIGSTOP,      // 停止进程的执行
    SIGTSTP,      // tty 发出停止进程，可忽略
    SIGTTIN,      // 后台进程请求输入
    SIGTTOU = 22, // 后台进程请求输出
};

#define MINSIG 1
#define MAXSIG 31

#define SIGMASK(sig) (1 << (sig - 1))

// 不阻止在指定的信号处理程序中再收到该信号
#define SIG_NOMASK 0x40000000

// 信号句柄一旦被调用过就恢复到默认处理句柄
#define SIG_ONESHOT 0x80000000

#define SIG_DFL ((void (*)(int))0) // 默认的信号处理程序（信号句柄）
#define SIG_IGN ((void (*)(int))1) // 忽略信号的处理程序

typedef u32 sigset_t;

// 信号处理结构
typedef struct sigaction_t
{
    void (*handler)(int); // 信号处理函数
    sigset_t mask;        // 信号屏蔽码
    u32 flags;
    void (*restorer)(void); // 恢复函数指针
} sigaction_t;

// 获取信号屏蔽码
int sgetmask(); 
// 设置信号屏蔽码
int ssetmask(int newmask);
// 注册信号处理函数
int signal(int sig, int handler);
// 注册信号处理函数，并且返回旧信号处理函数
int sigaction(int sig, sigaction_t *action, sigaction_t *oldaction);

#endif