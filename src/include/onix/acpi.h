#include <onix/types.h>

struct RSDPtr
{
    u8 Signature[8];
    u8 CheckSum;
    u8 OemID[6];
    u8 Revision;
    u32 *RsdtAddress;
};

struct FACP
{
    u8 Signature[4];
    u32 Length;
    u8 unneded1[40 - 8];
    u32 *DSDT;
    u8 unneded2[48 - 44];
    u32 *SMI_CMD;
    u8 ACPI_ENABLE;
    u8 ACPI_DISABLE;
    u8 unneded3[64 - 54];
    u32 *PM1a_CNT_BLK;
    u32 *PM1b_CNT_BLK;
    u8 unneded4[89 - 72];
    u8 PM1_CNT_LEN;
};
