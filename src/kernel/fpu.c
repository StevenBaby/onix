#include <onix/fpu.h>
#include <onix/task.h>
#include <onix/interrupt.h>
#include <onix/arena.h>
#include <onix/debug.h>
#include <onix/assert.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

task_t *last_fpu_task = NULL;

// 检查机器是否支持 FPU
bool fpu_check()
{
    u32 testword = 0x55AA;
    u32 ret;
    asm volatile(
        "movl %%cr0, %%edx\n"
        "andl %%ecx, %%edx\n"
        "movl %%edx, %%cr0\n"

        "fninit\n"
        "fnstsw %1\n"

        "movl %1, %%eax\n"
        : "=a"(ret)
        : "m"(testword), "c"(-1 - CR0_EM - CR0_TS));
    return ret == 0;
}

// 得到 cr3 寄存器
u32 get_cr0()
{
    // 直接将 mov eax, cr3，返回值在 eax 中
    asm volatile("movl %cr0, %eax\n");
}

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr0(u32 cr0)
{
    asm volatile("movl %%eax, %%cr0\n" ::"a"(cr0));
}

// 激活 fpu
void fpu_enable(task_t *task)
{
    LOGK("fpu enable...\n");

    set_cr0(get_cr0() & ~(CR0_EM | CR0_TS));
    // 如果使用的任务没有变化，则无需恢复浮点环境
    if (last_fpu_task == task)
        return;

    last_fpu_task = task;

    // 如果 fpu 不为空，则恢复浮点环境
    if (task->fpu)
    {
        asm volatile("frstor (%%eax) \n" ::"a"(task->fpu));
    }
    else
    {
        // 否则，初始化浮点环境
        asm volatile(
            "fnclex \n"
            "fninit \n");
    }
}

// 禁用 fpu
void fpu_disable(task_t *task)
{
    // 如果使用过浮点处理单元，则保存浮点环境
    if (task->flags & TASK_FPU_ENABLED)
    {
        assert(task->fpu);
        asm volatile("fnsave (%%eax) \n" ::"a"(task->fpu));
        task->flags &= ~TASK_FPU_ENABLED;
        LOGK("fpu disable...\n");
    }
    set_cr0(get_cr0() | CR0_EM);
}

void fpu_handler(int vector)
{
    LOGK("fpu handler...\n");
    assert(vector == INTR_FPU);
    task_t *task = running_task();
    assert(task->uid);

    fpu_enable(task);
    task->flags |= (TASK_FPU_ENABLED | TASK_FPU_USED);
    if (!task->fpu)
    {
        LOGK("FPU create state for task 0x%p\n", task);
        task->fpu = (fpu_t *)kmalloc(sizeof(fpu_t));
    }
}

// 初始化 FPU
void fpu_init()
{
    LOGK("fpu init...\n");

    bool exist = fpu_check();
    if (exist)
    {
        // 设置异常处理函数，非常类似于中断
        set_exception_handler(INTR_FPU, fpu_handler);
        // 设置 CR0 寄存器
        set_cr0(get_cr0() | CR0_EM | CR0_NE);
    }
    else
        LOGK("fpu not exists...\n");
}