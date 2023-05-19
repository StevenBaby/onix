#ifndef APIC_H
#define APIC_H

#include "types.h"

/*
	LVT
*/

struct APIC_LVT
{
	u32 	vector	:8,	//0~7	ALL
		deliver_mode	:3,	//8~10	      CMCI LINT0 LINT1 PerformCounter ThermalSensor
			res_1	:1,	//11
		deliver_status	:1,	//12	ALL
			polarity:1,	//13	           LINT0 LINT1
			irr	:1,	//14	           LINT0 LINT1
			trigger	:1,	//15	           LINT0 LINT1
			mask	:1,	//16	ALL
		timer_mode	:2,	//17~18	Timer
			res_2	:13;	//19~31
}_packed;		//disable align in struct

/*
	ICR
*/

struct INT_CMD_REG
{
	u32 	vector	:8,	//0~7
		deliver_mode	:3,	//8~10
		dest_mode	:1,	//11
		deliver_status	:1,	//12
			res_1	:1,	//13
			level	:1,	//14
			trigger	:1,	//15
			res_2	:2,	//16~17
		dest_shorthand	:2,	//18~19
			res_3	:12;	//20~31
	
	union {
		struct {
			u32	res_4	:24,	//32~55
			dest_field	:8;	//56~63		
			}apic_destination;
			
		u32 x2apic_destination;	//32~63
		}destination;
		
}_packed;

/*
	RTE
*/

struct IO_APIC_RET_entry
{
	u32 	vector	:8,	//0~7
		deliver_mode	:3,	//8~10
		dest_mode	:1,	//11
		deliver_status	:1,	//12
			polarity:1,	//13
			irr	:1,	//14
			trigger	:1,	//15
			mask	:1,	//16
			reserved:15;	//17~31

	union{
		struct {
			u32 reserved1	:24,	//32~55
				phy_dest	:4,	//56~59
				     reserved2	:4;	//60~63
			}physical;

		struct {
			u32 reserved1	:24,	//32~55
				logical_dest	:8;	//56~63
			}logical;
		}destination;
}_packed;

/*

*/

//delivery mode
#define	APIC_ICR_IOAPIC_Fixed 		 0	//LAPIC	IOAPIC 	ICR
#define	IOAPIC_ICR_Lowest_Priority 	 1	//	IOAPIC 	ICR
#define	APIC_ICR_IOAPIC_SMI		 2	//LAPIC	IOAPIC 	ICR

#define	APIC_ICR_IOAPIC_NMI		 4	//LAPIC	IOAPIC 	ICR
#define	APIC_ICR_IOAPIC_INIT		 5	//LAPIC	IOAPIC 	ICR
#define	ICR_Start_up			 6	//		ICR
#define	IOAPIC_ExtINT			 7	//	IOAPIC

//timer mode
#define APIC_LVT_Timer_One_Shot		0
#define APIC_LVT_Timer_Periodic		1
#define APIC_LVT_Timer_TSC_Deadline	2

//mask
#define APIC_ICR_IOAPIC_Masked		1
#define APIC_ICR_IOAPIC_UN_Masked	0

//trigger mode
#define APIC_ICR_IOAPIC_Edge		0
#define APIC_ICR_IOAPIC_Level		1

//delivery status
#define APIC_ICR_IOAPIC_Idle		0
#define APIC_ICR_IOAPIC_Send_Pending	1

//destination shorthand
#define ICR_No_Shorthand		0
#define ICR_Self			1
#define ICR_ALL_INCLUDE_Self		2
#define ICR_ALL_EXCLUDE_Self		3

//destination mode
#define ICR_IOAPIC_DELV_PHYSICAL	0
#define ICR_IOAPIC_DELV_LOGIC		1

//level
#define ICR_LEVEL_DE_ASSERT		0
#define ICR_LEVLE_ASSERT		1

//remote irr
#define APIC_IOAPIC_IRR_RESET		0
#define APIC_IOAPIC_IRR_ACCEPT		1

//pin polarity
#define APIC_IOAPIC_POLARITY_HIGH	0
#define APIC_IOAPIC_POLARITY_LOW	1

#define APIC_RCBA_ADDR 0xFEE00000

struct IOAPIC_map
{
	u32 physical_address;
	u8 * virtual_index_address;
	u32 *  virtual_data_address;
	u32 *  virtual_EOI_address;
};

u32 ioapic_rte_read(u8 index);
void ioapic_rte_write(u8 index,u32 value);
void IOAPIC_pagetable_remap();
void APIC_IOAPIC_init();
void Local_APIC_init();
void IOAPIC_init();
void IOAPIC_enable(u32 irq);
void IOAPIC_disable(u32 irq);
u32 IOAPIC_install(u32 irq,void * arg);
void IOAPIC_uninstall(u32 irq);

#endif
