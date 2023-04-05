#include <onix/interrupt.h>

extern void tss_init();
extern void acpi_init();
extern void memory_map_init();
extern void mapping_init();
extern void arena_init();

extern void interrupt_init();
extern void clock_init();
extern void timer_init();
extern void syscall_init();
extern void tss_init();
extern void hang();

void kernel_init()
{
    tss_init();
    acpi_init();
    memory_map_init();
    mapping_init();
    arena_init();

    interrupt_init(); // 初始化中断
    timer_init();     // 初始化定时器
    clock_init();     // 初始化时钟

    syscall_init(); // 初始化系统调用
    task_init();    // 初始化任务

    set_interrupt_state(true);
}
