#include <onix/types.h>
#include <onix/acpi.h>
#include <onix/string.h>
#include <onix/debug.h>

u32 *SMI_CMD;
u8 ACPI_ENABLE;
u8 ACPI_DISABLE;
u32 *PM1a_CNT;
u32 *PM1b_CNT;
u16 SLP_TYPa;
u16 SLP_TYPb;
u16 SLP_EN;
u16 SCI_EN;
u8 PM1_CNT_LEN;

// check if the given address has a valid header
unsigned int *acpi_check_RSDPtr(unsigned int *ptr)
{
    char *sig = "RSD PTR ";
    struct RSDPtr *rsdp = (struct RSDPtr *)ptr;
    u8 *bptr;
    u8 check = 0;
    int i;

    if (memcmp(sig, rsdp, 8) == 0)
    {
        // check checksum rsdpd
        bptr = (u8 *)ptr;
        for (i = 0; i < sizeof(struct RSDPtr); i++)
        {
            check += *bptr;
            bptr++;
        }

        // found valid rsdpd
        if (check == 0)
        {
            return (unsigned int *)rsdp->RsdtAddress;
        }
    }

    return NULL;
}

// finds the acpi header and returns the address of the rsdt
unsigned int *acpi_get_RSDPtr(void)
{
    unsigned int *addr;
    unsigned int *rsdp;

    // search below the 1mb mark for RSDP signature
    for (addr = (unsigned int *)0x000E0000; (int)addr < 0x00100000; addr += 0x10 / sizeof(addr))
    {
        rsdp = acpi_check_RSDPtr(addr);
        if (rsdp != NULL)
            return rsdp;
    }

    // at address 0x40:0x0E is the RM segment of the ebda
    int ebda = *((short *)0x40E);    // get pointer
    ebda = ebda * 0x10 & 0x000FFFFF; // transform segment into linear address

    // search Extended BIOS Data Area for the Root System Description Pointer signature
    for (addr = (unsigned int *)ebda; (int)addr < ebda + 1024; addr += 0x10 / sizeof(addr))
    {
        rsdp = acpi_check_RSDPtr(addr);
        if (rsdp != NULL)
            return rsdp;
    }

    return NULL;
}

// checks for a given header and validates checksum
int acpi_check_header(unsigned int *ptr, char *sig)
{
    char check = 0;
    if (memcmp(ptr, sig, 4) == 0)
    {
        char *checkPtr = (char *)ptr;
        int len = *(ptr + 1);
        while (0 < len--)
        {
            check += *checkPtr;
            checkPtr++;
        }
    }
    return !(check == 0);
}

int acpi_enable(void)
{
    // check if acpi is enabled
    if ((inw((unsigned int)PM1a_CNT) & SCI_EN) == 0 && SMI_CMD != 0 && ACPI_ENABLE != 0)
    {
        // check if acpi can be enabled
        outb((unsigned int)SMI_CMD, ACPI_ENABLE); // send acpi enable command
        // give 3 seconds time to enable acpi
        int i;
        for (i = 0; i < 300; i++)
            if ((inw((unsigned int)PM1a_CNT) & SCI_EN) == 1)
                break;

        if (PM1b_CNT != 0)
            for (; i < 300; i++)
            {
                if ((inw((unsigned int)PM1b_CNT) & SCI_EN) == 1)
                    break;
            }

        return 0;
    }
    return -1;
}

int init_acpi(void)
{
    unsigned int *ptr = acpi_get_RSDPtr();

    // check if address is correct  ( if acpi is available on this pc )
    // the RSDT contains an unknown number of pointers to acpi tables
    int entrys = *(ptr + 1);
    entrys = (entrys - 36) / 4;
    ptr += 36 / 4; // skip header information

    while (entrys-- > 0)
    {
        entrys = -2;
        struct FACP *facp = (struct FACP *)*ptr;

        // search the \_S5 package in the DSDT
        char *S5Addr = (char *)facp->DSDT + 36; // skip header
        int dsdtLength = *(facp->DSDT + 1) - 36;
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

            SMI_CMD = facp->SMI_CMD;

            ACPI_ENABLE = facp->ACPI_ENABLE;
            ACPI_DISABLE = facp->ACPI_DISABLE;

            PM1a_CNT = facp->PM1a_CNT_BLK;
            PM1b_CNT = facp->PM1b_CNT_BLK;

            PM1_CNT_LEN = facp->PM1_CNT_LEN;

            SLP_EN = 1 << 13;
            SCI_EN = 1;

            return 0;
        }
        ptr++;
    }

    DEBUGK("no valid FACP present.\n");
    return -1;
}

void acpi_shutdown(void)
{
    // SCI_EN is set to 1 if acpi shutdown is possible
    if (SCI_EN == 0)
        return;

    acpi_enable();

    // send the shutdown command
    outw((unsigned int)PM1a_CNT, SLP_TYPa | SLP_EN);
    if (PM1b_CNT != 0)
        outw((unsigned int)PM1b_CNT, SLP_TYPb | SLP_EN);
}
