#include <onix/task.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/memory.h>
#include <onix/assert.h>
#include <onix/interrupt.h>
#include <onix/string.h>
#include <onix/bitmap.h>
#include <onix/syscall.h>
#include <onix/list.h>
#include <onix/global.h>
#include <onix/arena.h>
#include <onix/fs.h>
#include <onix/errno.h>
#include <onix/timer.h>
#include <onix/device.h>
#include <onix/tty.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern u32 volatile jiffies;
extern u32 jiffy;
extern bitmap_t kernel_map;
extern tss_t tss;
extern file_t file_table[];

extern void task_switch(task_t *next);

task_t *task_table[TASK_NR]; // 任务表
static list_t block_list;    // 任务默认阻塞链表
static list_t sleep_list;    // 任务睡眠链表

static task_t *idle_task;

// 从 task_table 里获得一个空闲的任务
static task_t *get_free_task()
{
    for (size_t i = 0; i < TASK_NR; i++)
    {
        if (task_table[i] == NULL)
        {
            task_t *task = (task_t *)alloc_kpage(1);
            memset(task, 0, PAGE_SIZE);
            task->pid = i;
            task_table[i] = task;
            return task;
        }
    }
    panic("No more tasks");
}

// 获得 pid 对应的 task
task_t *get_task(pid_t pid)
{
    for (size_t i = 0; i < TASK_NR; i++)
    {
        if (!task_table[i])
            continue;
        if (task_table[i]->pid == pid)
            return task_table[i];
    }
    return NULL;
}

// 获取进程 id
pid_t sys_getpid()
{
    task_t *task = running_task();
    return task->pid;
}

// 获取父进程 id
pid_t sys_getppid()
{
    task_t *task = running_task();
    return task->ppid;
}

fd_t task_get_fd(task_t *task)
{
    fd_t i;
    for (i = 3; i < TASK_FILE_NR; i++)
    {
        if (!task->files[i])
            break;
    }
    if (i == TASK_FILE_NR)
    {
        panic("Exceed task max open files.");
    }
    return i;
}

void task_put_fd(task_t *task, fd_t fd)
{
    assert(fd < TASK_FILE_NR);
    task->files[fd] = NULL;
}

// 从任务数组中查找某种状态的任务，自己除外
static task_t *task_search(task_state_t state)
{
    assert(!get_interrupt_state());
    task_t *task = NULL;
    task_t *current = running_task();

    for (size_t i = 0; i < TASK_NR; i++)
    {
        task_t *ptr = task_table[i];
        if (ptr == NULL)
            continue;

        if (ptr->state != state)
            continue;
        if (current == ptr)
            continue;
        if (task == NULL || task->ticks < ptr->ticks || ptr->jiffies < task->jiffies)
            task = ptr;
    }

    if (task == NULL && state == TASK_READY)
    {
        task = idle_task;
    }

    return task;
}

void task_yield()
{
    schedule();
}

bool _inline task_leader(task_t *task)
{
    return task->sid == task->pid;
}

// 任务阻塞
int task_block(task_t *task, list_t *blist, task_state_t state, int timeout_ms)
{
    assert(!get_interrupt_state());
    assert(task->node.next == NULL);
    assert(task->node.prev == NULL);

    if (blist == NULL)
    {
        blist = &block_list;
    }

    assert(state != TASK_READY && state != TASK_RUNNING);

    list_push(blist, &task->node);
    if (timeout_ms > 0)
    {
        timer_add(timeout_ms, NULL, NULL);
    }

    task->state = state;

    task_t *current = running_task();
    if (current == task)
    {
        schedule();
    }

    return task->status;
}

// 解除任务阻塞
void task_unblock(task_t *task, int reason)
{
    assert(!get_interrupt_state());

    list_remove(&task->node);

    assert(task->node.next == NULL);
    assert(task->node.prev == NULL);

    task->status = reason;
    task->state = TASK_READY;
}

void task_sleep(u32 ms)
{
    assert(!get_interrupt_state()); // 不可中断

    task_t *task = running_task();

    task_block(task, &sleep_list, TASK_SLEEPING, ms);
}

// 激活任务
void task_activate(task_t *task)
{
    assert(task->magic == ONIX_MAGIC);

    if (task->pde != get_cr3())
    {
        set_cr3(task->pde);
        // BMB;
    }

    if (task->uid != KERNEL_USER)
    {
        tss.esp0 = (u32)task + PAGE_SIZE;
    }
}

task_t *running_task()
{
    asm volatile(
        "movl %esp, %eax\n"
        "andl $0xfffff000, %eax\n");
}

void schedule()
{
    assert(!get_interrupt_state()); // 不可中断

    task_t *current = running_task();
    task_t *next = task_search(TASK_READY);

    assert(next != NULL);
    assert(next->magic == ONIX_MAGIC);

    if (current->state == TASK_RUNNING)
    {
        current->state = TASK_READY;
    }

    if (!current->ticks)
    {
        current->ticks = current->priority;
    }

    next->state = TASK_RUNNING;
    if (next == current)
        return;

    task_activate(next);
    task_switch(next);
}

static task_t *task_create(target_t target, const char *name, u32 priority, u32 uid)
{
    task_t *task = get_free_task();

    u32 stack = (u32)task + PAGE_SIZE;

    stack -= sizeof(task_frame_t);
    task_frame_t *frame = (task_frame_t *)stack;
    frame->ebx = 0x11111111;
    frame->esi = 0x22222222;
    frame->edi = 0x33333333;
    frame->ebp = 0x44444444;
    frame->eip = (void *)target;

    strcpy((char *)task->name, name);

    task->stack = (u32 *)stack;
    task->priority = priority;
    task->ticks = task->priority;
    task->jiffies = 0;
    task->state = TASK_READY;
    task->uid = uid;
    task->gid = 0; // TODO: group
    task->pgid = 0;
    task->sid = 0;
    task->vmap = &kernel_map;
    task->pde = KERNEL_PAGE_DIR; // page directory entry
    task->brk = USER_EXEC_ADDR;
    task->text = USER_EXEC_ADDR;
    task->data = USER_EXEC_ADDR;
    task->end = USER_EXEC_ADDR;
    task->iexec = NULL;
    task->iroot = task->ipwd = get_root_inode();
    task->iroot->count += 2;

    task->pwd = (void *)alloc_kpage(1);
    strcpy(task->pwd, "/");

    task->umask = 0022; // 对应 0755

    task->files[STDIN_FILENO] = &file_table[STDIN_FILENO];
    task->files[STDOUT_FILENO] = &file_table[STDOUT_FILENO];
    task->files[STDERR_FILENO] = &file_table[STDERR_FILENO];
    task->files[STDIN_FILENO]->count++;
    task->files[STDOUT_FILENO]->count++;
    task->files[STDERR_FILENO]->count++;

    // 初始化信号
    task->signal = 0;
    task->blocked = 0;
    for (size_t i = 0; i < MAXSIG; i++)
    {
        sigaction_t *action = &task->actions[i];
        action->flags = 0;
        action->mask = 0;
        action->handler = SIG_DFL;
        action->restorer = NULL;
    }

    task->magic = ONIX_MAGIC;

    return task;
}

extern int sys_execve();
extern int init_user_thread();

// 调用该函数的地方不能有任何局部变量
// 调用前栈顶需要准备足够的空间
void task_to_user_mode()
{
    task_t *task = running_task();

    // 创建用户进程虚拟内存位图
    task->vmap = kmalloc(sizeof(bitmap_t));
    void *buf = (void *)alloc_kpage(1);
    bitmap_init(task->vmap, buf, USER_MMAP_SIZE / PAGE_SIZE / 8, USER_MMAP_ADDR / PAGE_SIZE);

    // 创建用户进程页表
    task->pde = (u32)copy_pde();
    set_cr3(task->pde);

    u32 addr = (u32)task + PAGE_SIZE;

    addr -= sizeof(intr_frame_t);
    intr_frame_t *iframe = (intr_frame_t *)(addr);

    iframe->vector = 0x20;
    iframe->edi = 1;
    iframe->esi = 2;
    iframe->ebp = 3;
    iframe->esp_dummy = 4;
    iframe->ebx = 5;
    iframe->edx = 6;
    iframe->ecx = 7;
    iframe->eax = 8;

    iframe->gs = 0;
    iframe->ds = USER_DATA_SELECTOR;
    iframe->es = USER_DATA_SELECTOR;
    iframe->fs = USER_DATA_SELECTOR;
    iframe->ss = USER_DATA_SELECTOR;
    iframe->cs = USER_CODE_SELECTOR;

    iframe->error = ONIX_MAGIC;

    iframe->eip = (u32)init_user_thread;
    iframe->eflags = (0 << 12 | 0b10 | 1 << 9);
    iframe->esp = USER_STACK_TOP;

#ifdef ONIX_DEBUG
    // ROP 技术，直接从中断返回
    // 通过 eip 跳转到 entry 执行
    asm volatile(
        "movl %0, %%esp\n"
        "jmp interrupt_exit\n" ::"m"(iframe));
#else
    int err = sys_execve("/bin/init.out", NULL, NULL);
    panic("exec /bin/init.out failure");
#endif
}

extern void interrupt_exit();

static void task_build_stack(task_t *task)
{
    u32 addr = (u32)task + PAGE_SIZE;
    addr -= sizeof(intr_frame_t);
    intr_frame_t *iframe = (intr_frame_t *)addr;
    iframe->eax = 0;

    addr -= sizeof(task_frame_t);
    task_frame_t *frame = (task_frame_t *)addr;

    frame->ebp = 0xaa55aa55;
    frame->ebx = 0xaa55aa55;
    frame->edi = 0xaa55aa55;
    frame->esi = 0xaa55aa55;

    frame->eip = interrupt_exit;

    task->stack = (u32 *)frame;
}

pid_t task_fork()
{
    // LOGK("fork is called\n");
    task_t *task = running_task();

    // 当前进程没有阻塞，且正在执行
    assert(task->node.next == NULL && task->node.prev == NULL && task->state == TASK_RUNNING);

    // 拷贝内核栈 和 PCB
    task_t *child = get_free_task();
    pid_t pid = child->pid;
    memcpy(child, task, PAGE_SIZE);

    child->pid = pid;
    child->ppid = task->pid;

    child->ticks = child->priority;
    child->state = TASK_READY;

    // 拷贝用户进程虚拟内存位图
    child->vmap = kmalloc(sizeof(bitmap_t));
    memcpy(child->vmap, task->vmap, sizeof(bitmap_t));

    // 拷贝虚拟位图缓存
    void *buf = (void *)alloc_kpage(1);
    memcpy(buf, task->vmap->bits, PAGE_SIZE);
    child->vmap->bits = buf;

    // 拷贝页目录
    child->pde = (u32)copy_pde();

    // 拷贝 pwd
    child->pwd = (char *)alloc_kpage(1);
    strncpy(child->pwd, task->pwd, PAGE_SIZE);

    // 工作目录引用加一
    task->ipwd->count++;
    task->iroot->count++;
    if (task->iexec)
        task->iexec->count++;

    // 文件引用加一
    for (size_t i = 0; i < TASK_FILE_NR; i++)
    {
        file_t *file = child->files[i];
        if (file)
            file->count++;
    }

    // 构造 child 内核栈
    task_build_stack(child); // ROP
    // schedule();

    return child->pid;
}

// 如果进程是会话首领则向会话中所有进程发送信号 SIGHUP
static void task_kill_session(task_t *task)
{
    if (!task_leader(task))
        return;

    for (size_t i = 0; i < TASK_NR; i++)
    {
        task_t *child = task_table[i];
        if (!child)
            continue;
        if (task == child || task->sid != child->sid)
            continue;
        child->signal |= SIGMASK(SIGHUP);
    }
}

// 释放 TTY 设备
static void task_free_tty(task_t *task)
{
    if (task_leader(task) && task->tty > 0)
    {
        device_t *device = device_get(task->tty);
        tty_t *tty = (tty_t *)device->ptr;
        tty->pgid = 0;
    }
}

// 子进程退出，通知父进程
static void task_tell_father(task_t *task)
{
    if (!task->ppid)
        return;
    for (size_t i = 0; i < TASK_NR; i++)
    {
        task_t *parent = task_table[i];
        if (!parent)
            continue;
        if (parent->pid != task->ppid)
            continue;
        parent->signal |= SIGMASK(SIGCHLD);
        return;
    }
    panic("No Parent found!!!");
}

void task_exit(int status)
{
    task_t *task = running_task();

    // 当前进程没有阻塞，且正在执行
    assert(task->node.next == NULL && task->node.prev == NULL && task->state == TASK_RUNNING);

    task->state = TASK_DIED;
    task->status = status;

    task_kill_session(task);
    task_tell_father(task);
    task_free_tty(task);

    timer_remove(task);

    free_pde();

    free_kpage((u32)task->vmap->bits, 1);
    kfree(task->vmap);

    free_kpage((u32)task->pwd, 1);
    iput(task->ipwd);
    iput(task->iroot);
    iput(task->iexec);

    for (size_t i = 0; i < TASK_FILE_NR; i++)
    {
        file_t *file = task->files[i];
        if (file)
        {
            close(i);
        }
    }

    // 将子进程的父进程赋值为自己的父进程
    for (size_t i = 2; i < TASK_NR; i++)
    {
        task_t *child = task_table[i];
        if (!child)
            continue;
        if (child->ppid != task->pid)
            continue;
        child->ppid = task->ppid;
    }
    LOGK("task %s 0x%p exit....\n", task->name, task);

    task_t *parent = task_table[task->ppid];
    if (parent->state == TASK_WAITING &&
        (parent->waitpid == -1 || parent->waitpid == task->pid))
    {
        task_unblock(parent, EOK);
    }

    schedule();
}

pid_t task_waitpid(pid_t pid, int32 *status)
{
    task_t *task = running_task();
    task_t *child = NULL;

    while (true)
    {
        bool has_child = false;
        for (size_t i = 2; i < TASK_NR; i++)
        {
            task_t *ptr = task_table[i];
            if (!ptr)
                continue;

            if (ptr->ppid != task->pid)
                continue;
            if (pid != ptr->pid && pid != -1)
                continue;

            if (ptr->state == TASK_DIED)
            {
                child = ptr;
                task_table[i] = NULL;
                goto rollback;
            }

            has_child = true;
        }
        if (has_child)
        {
            task->waitpid = pid;
            task_block(task, NULL, TASK_WAITING, TIMELESS);
            continue;
        }
        break;
    }

    // 没找到符合条件的子进程
    return -1;

rollback:
    *status = child->status;
    u32 ret = child->pid;
    free_kpage((u32)child, 1);
    return ret;
}

static void task_setup()
{
    task_t *task = running_task();
    task->magic = ONIX_MAGIC;
    task->ticks = 1;

    memset(task_table, 0, sizeof(task_table));
}

extern void idle_thread();
extern void init_thread();
extern void test_thread();

void task_init()
{
    list_init(&block_list);
    list_init(&sleep_list);

    task_setup();

    idle_task = task_create(idle_thread, "idle", 1, KERNEL_USER);
    task_create(init_thread, "init", 5, NORMAL_USER);
    task_create(test_thread, "test", 5, NORMAL_USER);
}
