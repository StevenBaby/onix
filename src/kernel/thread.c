#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

void idle_thread()
{
    set_interrupt_state(true);
    u32 counter = 0;
    while (true)
    {
        // LOGK("idle task.... %d\n", counter++);
        asm volatile(
            "sti\n" // 开中断
            "hlt\n" // 关闭 CPU，进入暂停状态，等待外中断的到来
        );
        yield(); // 放弃执行权，调度执行其他任务
    }
}

#include <onix/mutex.h>

lock_t lock;

void init_thread()
{
    lock_init(&lock);
    set_interrupt_state(true);
    u32 counter = 0;

    while (true)
    {
        lock_acquire(&lock);
        LOGK("init task %d....\n", counter++);
        lock_release(&lock);
        // sleep(500);
    }
}

void test_thread()
{
    set_interrupt_state(true);
    u32 counter = 0;

    while (true)
    {
        lock_acquire(&lock);
        LOGK("test task %d....\n", counter++);
        lock_release(&lock);
        // sleep(709);
    }
}