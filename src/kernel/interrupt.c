#include <onix/interrupt.h>
#include <onix/global.h>
#include <onix/debug.h>

gate_t idt[IDT_SIZE];
pointer_t idt_ptr;

extern void interrupt_handler();

void interrupt_init()
{
    for (size_t i = 0; i < IDT_SIZE; i++)
    {
        gate_t *gate = &idt[i];
        gate->offset0 = (u32)interrupt_handler & 0xffff;
        gate->offset1 = ((u32)interrupt_handler >> 16) & 0xffff;
        gate->selector = 1 << 3; // 代码段
        gate->reserved = 0;      // 保留不用
        gate->type = 0b1110;     // 中断门
        gate->segment = 0;       // 系统段
        gate->DPL = 0;           // 内核态
        gate->present = 1;       // 有效
    }
    idt_ptr.base = (u32)idt;
    idt_ptr.limit = sizeof(idt) - 1;
    BMB;
    asm volatile("lidt idt_ptr\n");
}
