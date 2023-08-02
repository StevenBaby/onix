#include <onix/apic.h>
#include <onix/memory.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

struct IOAPIC_map
{
    u32 physical_address;
    u8 *virtual_index_address;
    u32 *virtual_data_address;
    u32 *virtual_EOI_address;
} ioapic_map;

u64 ioapic_rte_read(u8 index)
{
    u64 ret;

    *ioapic_map.virtual_index_address = index + 1;
    io_mfence();
    ret = *ioapic_map.virtual_data_address;
    ret <<= 32;
    io_mfence();

    *ioapic_map.virtual_index_address = index;
    io_mfence();
    ret |= *ioapic_map.virtual_data_address;
    io_mfence();

    return ret;
}

void ioapic_rte_write(u8 index, u64 value)
{
    *ioapic_map.virtual_index_address = index;
    io_mfence();
    *ioapic_map.virtual_data_address = value & 0xffffffff;
    value >>= 32;
    io_mfence();

    *ioapic_map.virtual_index_address = index + 1;
    io_mfence();
    *ioapic_map.virtual_data_address = value & 0xffffffff;
    io_mfence();
}

void IOAPIC_enable(u64 irq)
{
    u64 value = 0;
    value = ioapic_rte_read(irq * 2 + 0x10);
    value &= ~0x10000UL;
    ioapic_rte_write(irq * 2 + 0x10, value);
}

void IOAPIC_disable(u64 irq)
{
    u64 value = 0;
    value = ioapic_rte_read(irq * 2 + 0x10);
    value |= 0x10000UL;
    ioapic_rte_write(irq * 2 + 0x10, value);
}

void IOAPIC_level_ack(u64 irq)
{
    __asm__ __volatile__("movl	$0x00,	%%edx	\n\t"
                         "movl	$0x00,	%%eax	\n\t"
                         "movl 	$0x80b,	%%ecx	\n\t"
                         "wrmsr	\n\t" ::
                             : "memory");

    *ioapic_map.virtual_EOI_address = 0;
}

void IOAPIC_edge_ack(u64 irq)
{
    __asm__ __volatile__("movl	$0x00,	%%edx	\n\t"
                         "movl	$0x00,	%%eax	\n\t"
                         "movl 	$0x80b,	%%ecx	\n\t"
                         "wrmsr	\n\t" ::
                             : "memory");
}

void Local_APIC_edge_level_ack(u64 irq)
{
    __asm__ __volatile__("movl	$0x00,	%%edx	\n\t"
                         "movl	$0x00,	%%eax	\n\t"
                         "movl 	$0x80b,	%%ecx	\n\t"
                         "wrmsr	\n\t" ::
                             : "memory");
}

void IOAPIC_pagetable_remap()
{
    ioapic_map.physical_address = 0xfec00000;
    ioapic_map.virtual_index_address = (u8 *)0xfec00000;
    ioapic_map.virtual_data_address = (u32 *)(0xfec00000 + 0x10);
    ioapic_map.virtual_EOI_address = (u32 *)(0xfec00000 + 0x40);

    map_area(ioapic_map.physical_address, 0x400000);
}

void Local_APIC_init()
{
    u32 eax, edx;
    u32 a, b, c, d;

    get_cpuid(1, 0, &a, &b, &c, &d);

    if((1<<9) & d)
		LOGK("HW support APIC&xAPIC\n");
	else
		LOGK("HW NO support APIC&xAPIC\n");
	
	if((1<<21) & c)
		LOGK("HW support x2APIC\n");
	else
		LOGK("HW NO support x2APIC\n");

	//enable xAPIC & x2APIC
	__asm__ __volatile__(	"movl 	$0x1b,	%%ecx	\n\t"
				"rdmsr	\n\t"
				"bts	$10,	%%eax	\n\t"
				"bts	$11,	%%eax	\n\t"
				"wrmsr	\n\t"
				"movl 	$0x1b,	%%ecx	\n\t"
				"rdmsr	\n\t"
				:"=a"(eax),"=d"(edx)
				:
				:"memory");

	LOGK("eax:%#010x,edx:%#010x\n",eax,edx);
	
	if(eax&0xc00)
		LOGK("xAPIC & x2APIC enabled\n");

	//enable SVR[8]
	__asm__ __volatile__(	"movl 	$0x80f,	%%ecx	\n\t"
				"rdmsr	\n\t"
				"bts	$8,	%%eax	\n\t"
				"bts	$12,	%%eax\n\t"
				"wrmsr	\n\t"
				"movl 	$0x80f,	%%ecx	\n\t"
				"rdmsr	\n\t"
				:"=a"(eax),"=d"(edx)
				:
				:"memory");

    __asm__ __volatile__("movl $0x80f, %%ecx    \n\t"
                         "rdmsr  \n\t"
                         "bts $8, %%eax  \n\t"
                         //"bts $12, %%eax \n\t"
                         "movl $0x80f, %%ecx    \n\t"
                         "wrmsr  \n\t"
                         "movl $0x80f , %%ecx   \n\t"
                         "rdmsr \n\t"
                         : "=a"(eax), "=d"(edx)::"memory");
    if (eax & 0x100)
        LOGK("APIC Software Enabled.\n");
    if (eax & 0x1000)
        LOGK("EOI-Broadcast Suppression Enabled.\n");

    // 获取Local APIC Version
    // 0x803处是 Local APIC Version register
    __asm__ __volatile__("movl $0x803, %%ecx    \n\t"
                         "rdmsr  \n\t"
                         : "=a"(eax), "=d"(edx)::"memory");

    if ((eax & 0xff) < 0x10)
        LOGK("82489DX discrete APIC.\n");
    else if (((eax & 0xff) >= 0x10) && ((eax & 0xff) <= 0x15))
        LOGK("Integrated APIC.\n");

    // 由于尚未配置LVT对应的处理程序，因此先屏蔽所有的LVT
    __asm__ __volatile__( 
        "movl 	$0x832,	%%ecx	\n\t" // Timer
        "wrmsr	\n\t"
        "movl 	$0x833,	%%ecx	\n\t" // Thermal Monitor
        "wrmsr	\n\t"
        "movl 	$0x834,	%%ecx	\n\t" // Performance Counter
        "wrmsr	\n\t"
        "movl 	$0x835,	%%ecx	\n\t" // LINT0
        "wrmsr	\n\t"
        "movl 	$0x836,	%%ecx	\n\t" // LINT1
        "wrmsr	\n\t"
        "movl 	$0x837,	%%ecx	\n\t" // Error
        "wrmsr	\n\t"
        :
        : "a"(0x10000), "d"(0x00)
        : "memory");
    LOGK("All LVT Masked.");
}

void IOAPIC_init()
{
    int i;
    //	I/O APIC
    //	I/O APIC	ID
    *ioapic_map.virtual_index_address = 0x00;
    io_mfence();
    *ioapic_map.virtual_data_address = 0x0f000000;
    io_mfence();

    //	I/O APIC	Version
    *ioapic_map.virtual_index_address = 0x01;
    io_mfence();

    // RTE
    for (i = 0x10; i < 0x40; i += 2)
        ioapic_rte_write(i, 0x10020 + ((i - 0x10) >> 1));
}

/*

*/

void APIC_IOAPIC_init()
{
    //	init trap abort fault
    int i;
    u32 x;
    u32 *p = NULL;

    IOAPIC_pagetable_remap();

    // mask 8259A
    outb(0x21, 0xff);
    outb(0xa1, 0xff);

    // enable IMCR
    outb(0x22, 0x70);
    outb(0x23, 0x01);

    // init local apic
    Local_APIC_init();

    // init ioapic
    IOAPIC_init();
}
