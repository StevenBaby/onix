#include <onix/signal.h>
#include <onix/task.h>
#include <onix/memory.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

typedef struct signal_frame_t
{
    u32 restorer; // 恢复函数
    u32 sig;      // 信号
    u32 blocked;  // 屏蔽位图
    // 保存调用时的寄存器，用于恢复执行信号之前的代码
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 eflags;
    u32 eip;
} signal_frame_t;

// 获取信号屏蔽位图
int sys_sgetmask()
{
    task_t *task = running_task();
    return task->blocked;
}

// 设置信号屏蔽位图
int sys_ssetmask(int newmask)
{
    if (newmask == EOF)
    {
        return -EPERM;
    }

    task_t *task = running_task();
    int old = task->blocked;
    task->blocked = newmask & ~SIGMASK(SIGKILL);
    return old;
}

// 注册信号处理函数
int sys_signal(int sig, int handler, int restorer)
{
    if (sig < MINSIG || sig > MAXSIG || sig == SIGKILL)
        return EOF;
    task_t *task = running_task();
    sigaction_t *ptr = &task->actions[sig - 1];
    ptr->mask = 0;
    ptr->handler = (void (*)(int))handler;
    ptr->flags = SIG_ONESHOT | SIG_NOMASK;
    ptr->restorer = (void (*)())restorer;
    return handler;
}

// 注册信号处理函数，更高级的一种方式
int sys_sigaction(int sig, sigaction_t *action, sigaction_t *oldaction)
{
    if (sig < MINSIG || sig > MAXSIG || sig == SIGKILL)
        return EOF;
    task_t *task = running_task();
    sigaction_t *ptr = &task->actions[sig - 1];
    if (oldaction)
        *oldaction = *ptr;

    *ptr = *action;
    if (ptr->flags & SIG_NOMASK)
        ptr->mask = 0;
    else
        ptr->mask |= SIGMASK(sig);
    return 0;
}

// 发送信号
int sys_kill(pid_t pid, int sig)
{
    if (sig < MINSIG || sig > MAXSIG)
        return EOF;
    task_t *task = get_task(pid);
    if (!task)
        return EOF;
    if (task->uid == KERNEL_USER)
        return EOF;
    if (task->pid == 1)
        return EOF;

    LOGK("kill task %s pid %d signal %d\n", task->name, pid, sig);
    task->signal |= SIGMASK(sig);
    if (task->state == TASK_WAITING || task->state == TASK_SLEEPING)
    {
        task_unblock(task, -EINTR);
    }
    return 0;
}

// 内核信号处理函数
void task_signal()
{
    task_t *task = running_task();
    // 获得任务可用信号位图
    u32 map = task->signal & (~task->blocked);
    if (!map)
        return;

    assert(task->uid);
    int sig = 1;
    for (; sig <= MAXSIG; sig++)
    {
        if (map & SIGMASK(sig))
        {
            // 将此信号置空，表示已执行
            task->signal &= (~SIGMASK(sig));
            break;
        }
    }

    // 得到对应的信号处理结构
    sigaction_t *action = &task->actions[sig - 1];
    // 忽略信号
    if (action->handler == SIG_IGN)
        return;
    // 子进程终止的默认处理方式
    if (action->handler == SIG_DFL && sig == SIGCHLD)
        return;
    // 默认信号处理方式，为退出
    if (action->handler == SIG_DFL)
        task_exit(SIGMASK(sig));

    // 处理用户态栈，使得程序去执行信号处理函数，处理结束之后，调用 restorer 恢复执行之前的代码
    intr_frame_t *iframe = (intr_frame_t *)((u32)task + PAGE_SIZE - sizeof(intr_frame_t));

    signal_frame_t *frame = (signal_frame_t *)(iframe->esp - sizeof(signal_frame_t));

    // 保存执行前的寄存器
    frame->eip = iframe->eip;
    frame->eflags = iframe->eflags;
    frame->edx = iframe->edx;
    frame->ecx = iframe->ecx;
    frame->eax = iframe->eax;

    // 屏蔽所有信号
    frame->blocked = EOF;

    // 不屏蔽在信号处理过程中，再次收到该信号
    if (action->flags & SIG_NOMASK)
        frame->blocked = task->blocked;

    // 信号
    frame->sig = sig;
    // 信号处理结束的恢复函数
    frame->restorer = (u32)action->restorer;

    LOGK("old esp 0x%p\n", iframe->esp);
    iframe->esp = (u32)frame;
    LOGK("new esp 0x%p\n", iframe->esp);
    iframe->eip = (u32)action->handler;

    // 如果设置了 ONESHOT，表示该信号只执行一次
    if (action->flags & SIG_ONESHOT)
        action->handler = SIG_DFL;

    // 进程屏蔽码添加此信号
    task->blocked |= action->mask;
}
