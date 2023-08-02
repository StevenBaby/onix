#ifndef ONIX_APIC_H
#define ONIX_APIC_H

#include <onix/types.h>
#include <onix/io.h>

#define APIC_SUCCESS 0
#define APIC_E_NOTFOUND 1

#define APIC_IO_APIC_VIRT_BASE_ADDR 0xfec00000UL
#define APIC_LOCAL_APIC_VIRT_BASE_ADDR 0xfee00000UL

// 当前apic启用状态标志
#define APIC_XAPIC_ENABLED 0
#define APIC_X2APIC_ENABLED 1

// ======== local apic 寄存器虚拟地址偏移量表 =======
// 0x00~0x10 Reserved.
#define LOCAL_APIC_OFFSET_Local_APIC_ID 0x20
#define LOCAL_APIC_OFFSET_Local_APIC_Version 0x30
// 0x40~0x70 Reserved.
#define LOCAL_APIC_OFFSET_Local_APIC_TPR 0x80
#define LOCAL_APIC_OFFSET_Local_APIC_APR 0x90
#define LOCAL_APIC_OFFSET_Local_APIC_PPR 0xa0
#define LOCAL_APIC_OFFSET_Local_APIC_EOI 0xb0
#define LOCAL_APIC_OFFSET_Local_APIC_RRD 0xc0
#define LOCAL_APIC_OFFSET_Local_APIC_LDR 0xd0
#define LOCAL_APIC_OFFSET_Local_APIC_DFR 0xe0
#define LOCAL_APIC_OFFSET_Local_APIC_SVR 0xf0

#define LOCAL_APIC_OFFSET_Local_APIC_ISR_31_0 0x100
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_63_32 0x110
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_95_64 0x120
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_127_96 0x130
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_159_128 0x140
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_191_160 0x150
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_223_192 0x160
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_255_224 0x170

#define LOCAL_APIC_OFFSET_Local_APIC_TMR_31_0 0x180
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_63_32 0x190
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_95_64 0x1a0
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_127_96 0x1b0
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_159_128 0x1c0
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_191_160 0x1d0
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_223_192 0x1e0
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_255_224 0x1f0

#define LOCAL_APIC_OFFSET_Local_APIC_IRR_31_0 0x200
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_63_32 0x210
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_95_64 0x220
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_127_96 0x230
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_159_128 0x240
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_191_160 0x250
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_223_192 0x260
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_255_224 0x270

#define LOCAL_APIC_OFFSET_Local_APIC_ESR 0x280

// 0x290~0x2e0 Reserved.

#define LOCAL_APIC_OFFSET_Local_APIC_LVT_CMCI 0x2f0
#define LOCAL_APIC_OFFSET_Local_APIC_ICR_31_0 0x300
#define LOCAL_APIC_OFFSET_Local_APIC_ICR_63_32 0x310
#define LOCAL_APIC_OFFSET_Local_APIC_LVT_TIMER 0x320
#define LOCAL_APIC_OFFSET_Local_APIC_LVT_THERMAL 0x330
#define LOCAL_APIC_OFFSET_Local_APIC_LVT_PERFORMANCE_MONITOR 0x340
#define LOCAL_APIC_OFFSET_Local_APIC_LVT_LINT0 0x350
#define LOCAL_APIC_OFFSET_Local_APIC_LVT_LINT1 0x360
#define LOCAL_APIC_OFFSET_Local_APIC_LVT_ERROR 0x370
// 初始计数寄存器（定时器专用）
#define LOCAL_APIC_OFFSET_Local_APIC_INITIAL_COUNT_REG 0x380
// 当前计数寄存器（定时器专用）
#define LOCAL_APIC_OFFSET_Local_APIC_CURRENT_COUNT_REG 0x390
// 0x3A0~0x3D0 Reserved.
// 分频配置寄存器（定时器专用）
#define LOCAL_APIC_OFFSET_Local_APIC_CLKDIV 0x3e0


// 投递模式
#define LOCAL_APIC_FIXED 0
#define IO_APIC_FIXED 0
#define ICR_APIC_FIXED 0

#define IO_APIC_Lowest_Priority 1
#define ICR_Lowest_Priority 1

#define LOCAL_APIC_SMI 2
#define APIC_SMI 2
#define ICR_SMI 2

#define LOCAL_APIC_NMI 4
#define APIC_NMI 4
#define ICR_NMI 4

#define LOCAL_APIC_INIT 5
#define APIC_INIT 5
#define ICR_INIT 5

#define ICR_Start_up 6

#define IO_APIC_ExtINT 7

// 时钟模式
#define APIC_LVT_Timer_One_Shot 0
#define APIC_LVT_Timer_Periodic 1
#define APIC_LVT_Timer_TSC_Deadline 2

// 屏蔽
#define UNMASKED 0
#define MASKED 1
#define APIC_LVT_INT_MASKED 0x10000UL

// 触发模式
#define EDGE_TRIGGER 0  // 边沿触发
#define Level_TRIGGER 1 // 电平触发

// 投递模式
#define IDLE 0         // 挂起
#define SEND_PENDING 1 // 发送等待

// destination shorthand
#define ICR_No_Shorthand 0
#define ICR_Self 1
#define ICR_ALL_INCLUDE_Self 2
#define ICR_ALL_EXCLUDE_Self 3

// 投递目标模式
#define DEST_PHYSICAL 0 // 物理模式
#define DEST_LOGIC 1    // 逻辑模式

// level
#define ICR_LEVEL_DE_ASSERT 0
#define ICR_LEVEL_ASSERT 1

// 远程IRR标志位, 在处理Local APIC标志位时置位，在收到处理器发来的EOI命令时复位
#define IRR_RESET 0
#define IRR_ACCEPT 1

// 电平触发极性
#define POLARITY_HIGH 0
#define POLARITY_LOW 1

_inline void io_mfence()
{
    __asm__ __volatile__("mfence");
}

_inline void get_cpuid(u32 Mop, u32 Sop, u32 *a, u32 *b, u32 *c, u32 *d)
{
    __asm__ __volatile__("cpuid	\n\t"
                         : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                         : "0"(Mop), "2"(Sop));
}

void APIC_IOAPIC_init();
void Local_APIC_init();
void IOAPIC_init();

void IOAPIC_enable(u64 irq);
void IOAPIC_disable(u64 irq);
void IOAPIC_level_ack(u64 irq);
void IOAPIC_edge_ack(u64 irq);
void Local_APIC_edge_level_ack(u64 irq);

#endif
