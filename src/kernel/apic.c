#include <onix/apic.h>
#include <onix/string.h>
#include <onix/debug.h>
#include <onix/memory.h>
#include <onix/types.h>
#include <onix/io.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)
// #define LOGK(fmt, args...)

struct IOAPIC_map ioapic_map;

static inline void get_cpuid(u32 Mop,u32 Sop,u32 * a,u32 * b,u32 * c,u32 * d)
{
	__asm__ __volatile__	(	"cpuid	\n\t"
					:"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d)
					:"0"(Mop),"2"(Sop)
				);
}

void IOAPIC_enable(u32 irq)
{
	u32 value = 0;
	value = ioapic_rte_read((irq) * 2 + 0x10);
	value = value & (~0x10000UL); 
	ioapic_rte_write((irq) * 2 + 0x10, value);
}

void IOAPIC_disable(u32 irq)
{
	u32 value = 0;
	value = ioapic_rte_read((irq) * 2 + 0x10);
	value = value | 0x10000UL; 
	ioapic_rte_write((irq) * 2 + 0x10,value);
}

u32 IOAPIC_install(u32 irq,void * arg)
{
	struct IO_APIC_RET_entry *entry = (struct IO_APIC_RET_entry *)arg;
	ioapic_rte_write((irq) * 2 + 0x10,*(u32 *)entry);

	return 1;
}

void IOAPIC_uninstall(u32 irq)
{
	ioapic_rte_write((irq) * 2 + 0x10,0x10000UL);
}

u32 ioapic_rte_read(u8 index)
{
	u32 ret;

	*ioapic_map.virtual_index_address = index;
	io_mfence();
	ret = *ioapic_map.virtual_data_address;
	io_mfence();

	return ret;
}

void ioapic_rte_write(u8 index,u32 value)
{
	*ioapic_map.virtual_index_address = index;
	io_mfence();
	*ioapic_map.virtual_data_address = value;
	io_mfence();
}

void Local_APIC_init()
{
	u32 x,y;
	u32 a,b,c,d;

	//check APIC & x2APIC support
	get_cpuid(1,0,&a,&b,&c,&d);
	LOGK("CPUID\t01,eax:%#010x,ebx:%#010x,ecx:%#010x,edx:%#010x\n",a,b,c,d);

	if((1<<9) & d)
		LOGK("HW support APIC&xAPIC\t");
	else
		LOGK("HW NO support APIC&xAPIC\t");
	
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
				:"=a"(x),"=d"(y)
				:
				:"memory");

	LOGK("eax:%#010x,edx:%#010x\t",x,y);
	
	if(x&0xc00)
		LOGK("xAPIC & x2APIC enabled\n");

	//enable SVR[8]
	__asm__ __volatile__(	"movl 	$0x80f,	%%ecx	\n\t"
				"rdmsr	\n\t"
				"bts	$8,	%%eax	\n\t"
				"bts	$12,	%%eax\n\t"
				"wrmsr	\n\t"
				"movl 	$0x80f,	%%ecx	\n\t"
				"rdmsr	\n\t"
				:"=a"(x),"=d"(y)
				:
				:"memory");

	LOGK("eax:%#010x,edx:%#010x\t",x,y);

	if(x&0x100)
		LOGK("SVR[8] enabled\n");
	if(x&0x1000)
		LOGK("SVR[12] enabled\n");

	//get local APIC ID
	__asm__ __volatile__(	"movl $0x802,	%%ecx	\n\t"
				"rdmsr	\n\t"
				:"=a"(x),"=d"(y)
				:
				:"memory");
	
	LOGK("eax:%#010x,edx:%#010x\tx2APIC ID:%#010x\n",x,y,x);
	
	//get local APIC version
	__asm__ __volatile__(	"movl $0x803,	%%ecx	\n\t"
				"rdmsr	\n\t"
				:"=a"(x),"=d"(y)
				:
				:"memory");

	LOGK("local APIC Version:%#010x,Max LVT Entry:%#010x,SVR(Suppress EOI Broadcast):%#04x\t",x & 0xff,(x >> 16 & 0xff) + 1,x >> 24 & 0x1);

	if((x & 0xff) < 0x10)
		LOGK("82489DX discrete APIC\n");

	else if( ((x & 0xff) >= 0x10) && ((x & 0xff) <= 0x15) )
		LOGK("Integrated APIC\n");

	//mask all LVT	
	__asm__ __volatile__(	"movl 	$0x82f,	%%ecx	\n\t"	//CMCI
				"wrmsr	\n\t"
				"movl 	$0x832,	%%ecx	\n\t"	//Timer
				"wrmsr	\n\t"
				"movl 	$0x833,	%%ecx	\n\t"	//Thermal Monitor
				"wrmsr	\n\t"
				"movl 	$0x834,	%%ecx	\n\t"	//Performance Counter
				"wrmsr	\n\t"
				"movl 	$0x835,	%%ecx	\n\t"	//LINT0
				"wrmsr	\n\t"
				"movl 	$0x836,	%%ecx	\n\t"	//LINT1
				"wrmsr	\n\t"
				"movl 	$0x837,	%%ecx	\n\t"	//Error
				"wrmsr	\n\t"
				:
				:"a"(0x10000),"d"(0x00)
				:"memory");

	LOGK("Mask ALL LVT\n");

	//TPR
	__asm__ __volatile__(	"movl 	$0x808,	%%ecx	\n\t"
				"rdmsr	\n\t"
				:"=a"(x),"=d"(y)
				:
				:"memory");

	LOGK("Set LVT TPR:%#010x\t",x);

	//PPR
	__asm__ __volatile__(	"movl 	$0x80a,	%%ecx	\n\t"
				"rdmsr	\n\t"
				:"=a"(x),"=d"(y)
				:
				:"memory");

	LOGK("Set LVT PPR:%#010x\n",x);
}

void IOAPIC_init()
{
	int i ;
	//	I/O APIC
	//	I/O APIC	ID	
	*ioapic_map.virtual_index_address = 0x00;
	io_mfence();
	*ioapic_map.virtual_data_address = 0x0f000000;
	io_mfence();
	LOGK("Get IOAPIC ID REG:%#010x,ID:%#010x\n",*ioapic_map.virtual_data_address, *ioapic_map.virtual_data_address >> 24 & 0xf);
	io_mfence();

	//	I/O APIC	Version
	*ioapic_map.virtual_index_address = 0x01;
	io_mfence();
	LOGK("Get IOAPIC Version REG:%#010x,MAX redirection enties:%#08d\n",*ioapic_map.virtual_data_address ,((*ioapic_map.virtual_data_address >> 16) & 0xff) + 1);

	//RTE	
	for(i = 0x10;i < 0x40;i += 2)
		ioapic_rte_write(i,0x10020 + ((i - 0x10) >> 1));

	LOGK("I/O APIC Redirection Table Entries Set Finished.\n");	
}

void APIC_IOAPIC_init()
{
	int i ;
	u32 x;
	u32 * p = NULL;

	map_area(0xfec00000, 0x300000);
	u8* IOAPIC_addr = (u8 *)0xfec00000;

	ioapic_map.physical_address = (u32)IOAPIC_addr;
	ioapic_map.virtual_index_address = IOAPIC_addr;
	ioapic_map.virtual_data_address = (u32 *)(IOAPIC_addr + 0x10);
	ioapic_map.virtual_EOI_address = (u32 *)(IOAPIC_addr + 0x40);

	//mask 8259A
	LOGK("MASK 8259A\n");
	outb(0x21,0xff);
	outb(0xa1,0xff);

	//enable IMCR
	outb(0x22,0x70);
	outb(0x23,0x01);	
	
	//init local apic
	Local_APIC_init();

	//init ioapic
	IOAPIC_init();

	x = APIC_RCBA_ADDR;

	LOGK("Get RCBA Address:%#010x\n",x);
}
