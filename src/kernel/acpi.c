#include <onix/types.h>
#include <onix/acpi.h>
#include <onix/string.h>
#include <onix/debug.h>
#include <onix/stdlib.h>
#include <onix/io.h>
#include <onix/interrupt.h>

u32 *SMI_CMD;
u8 ACPI_ENABLE;
u8 ACPI_DISABLE;
u32 *PM1a_CNT;
u32 *PM1b_CNT;
u32 *PM1a_EVT;
u32 *PM1b_EVT;
u32 PM1_EVT_LEN;
u16 SLP_TYPa;
u16 SLP_TYPb;
u16 SLP_EN;
u16 SCI_EN;
u16 SCI_Interrupt;

FADT *acpiFadt;

// check if the given address has a valid header
u32 *acpi_check_RSDPtr(u32 *ptr)
{
    char *sig = "RSD PTR ";
    RSDPtr *rsdp = (RSDPtr *)ptr;
    u8 *bptr;
    u8 check = 0;
    int i;

    if (memcmp(sig, rsdp, 8) == 0)
    {
        // check checksum rsdpd
        bptr = (u8 *)ptr;
        for (i = 0; i < sizeof(RSDPtr); i++)
        {
            check += *bptr;
            bptr++;
        }

        // found valid rsdpd
        if (check == 0)
        {
            return (u32 *)rsdp->RsdtAddress;
        }
    }

    return NULL;
}

// finds the acpi header and returns the address of the rsdt
u32 *acpi_get_RSDPtr(void)
{
    u32 *addr;
    u32 *rsdp;

    // search below the 1mb mark for RSDP signature
    for (addr = (u32 *)0x000E0000; (int)addr < 0x00100000; addr += 0x10 / sizeof(addr))
    {
        rsdp = acpi_check_RSDPtr(addr);
        if (rsdp != NULL)
            return rsdp;
    }

    // at address 0x40:0x0E is the RM segment of the ebda
    int ebda = *((short *)0x40E);    // get pointer
    ebda = ebda * 0x10 & 0x000FFFFF; // transform segment into linear address

    // search Extended BIOS Data Area for the Root System Description Pointer signature
    for (addr = (u32 *)ebda; (int)addr < ebda + 1024; addr += 0x10 / sizeof(addr))
    {
        rsdp = acpi_check_RSDPtr(addr);
        if (rsdp != NULL)
            return rsdp;
    }

    return NULL;
}

// 初始化 ACPI
int acpi_init(void)
{
    u32 *ptr = acpi_get_RSDPtr();

    // check if address is correct  ( if acpi is available on this pc )
    // the RSDT contains an unknown number of pointers to acpi tables
    int entrys = *(ptr + 1);
    entrys = (entrys - 36) / 4;
    ptr += 36 / 4; // skip header information

    while (entrys-- > 0)
    {
        entrys = -2;
        acpiFadt = (FADT *)*ptr;

        // search the \_S5 package in the DSDT
        char *S5Addr = (char *)acpiFadt->Dsdt + 36; // skip header
        int dsdtLength = *((u32 *)acpiFadt->Dsdt + 1) - 36;
        while (0 < dsdtLength--)
        {
            if (memcmp(S5Addr, "_S5_", 4) == 0)
                break;
            S5Addr++;
        }
        // check for valid AML structure
        if ((*(S5Addr - 1) == 0x08 || (*(S5Addr - 2) == 0x08 && *(S5Addr - 1) == '\\')) && *(S5Addr + 4) == 0x12)
        {
            S5Addr += 5;
            S5Addr += ((*S5Addr & 0xC0) >> 6) + 2; // calculate PkgLength size

            if (*S5Addr == 0x0A)
                S5Addr++; // skip u8prefix
            SLP_TYPa = *(S5Addr) << 10;
            S5Addr++;

            if (*S5Addr == 0x0A)
                S5Addr++; // skip u8prefix
            SLP_TYPb = *(S5Addr) << 10;

            SMI_CMD = (u32 *)acpiFadt->SMI_CommandPort;

            ACPI_ENABLE = acpiFadt->AcpiEnable;
            ACPI_DISABLE = acpiFadt->AcpiDisable;

            PM1a_CNT = (u32 *)acpiFadt->PM1aControlBlock;
            PM1b_CNT = (u32 *)acpiFadt->PM1bControlBlock;

            SLP_EN = 1 << 13;
            SCI_EN = 1;

            PM1_EVT_LEN = acpiFadt->PM1EventLength;

            SCI_Interrupt = acpiFadt->SCI_Interrupt;

            acpi_enable();

            return 0;
        }
        ptr++;
    }

    DEBUGK("no valid FADT present.\n");
    return -1;
}

int acpi_enable(void)
{
    // check if acpi is enabled
    if ((inw((u32)PM1a_CNT) & SCI_EN) == 0 && SMI_CMD != 0 && ACPI_ENABLE != 0)
    {
        // check if acpi can be enabled
        outb((u32)SMI_CMD, ACPI_ENABLE); // send acpi enable command
        // give 3 seconds time to enable acpi
        int i;
        for (i = 0; i < 300; i++)
        {
            if ((inw((u32)PM1a_CNT) & SCI_EN) == 1)
                break;
        }
        if (PM1b_CNT != 0)
            for (; i < 300; i++)
            {
                if ((inw((u32)PM1b_CNT) & SCI_EN) == 1)
                    break;
            }
        if (i < 300)
        {
            DEBUGK("enabled acpi.\n");
            return 0;
        }
        else
        {
            DEBUGK("couldn't enable acpi.\n");
            return -1;
        }
    }
}

void sys_shutdown()
{
    // SCI_EN is set to 1 if acpi shutdown is possible
    if (SCI_EN == 0)
        return;

    // 发送关机命令
    outw((u32)PM1a_CNT, SLP_TYPa | SLP_EN);
    if (PM1b_CNT != 0)
        outw((u32)PM1b_CNT, SLP_TYPb | SLP_EN);
}

void sys_reboot()
{
    while (inb(0x64) & 0x3)
        inb(0x60);
    outb(0x64, 0xfe);

    // 第一种不行使用第二种

    while (1)
        outb(acpiFadt->ResetReg.Address, acpiFadt->ResetValue);
}
