#include <onix/types.h>
#include <onix/net/chksum.h>
#include <onix/string.h>
#include <onix/debug.h>

static u32 __attribute__((section(".onix.addr"))) kernel_addr = 0x20000;
static u32 __attribute__((section(".onix.size"))) kernel_size = 0;
static u32 __attribute__((section(".onix.chksum"))) kernel_chksum = 0;
static u32 __attribute__((section(".onix.magic"))) kernel_magic = ONIX_MAGIC;

static u8 magic_msg[] = "kernel magic check failure!!!";
static u8 crc32_msg[] = "kernel crc32 check failure!!!";
static char *video = (char *)0xb8000;

extern void hang();

static void show_message(char *msg, int len)
{
    for (int i = 0; i < len; i++)
        video[i * 2] = msg[i];
}

err_t onix_init()
{
    if (kernel_magic != ONIX_MAGIC)
    {
        show_message(magic_msg, sizeof(magic_msg));
        goto failure;
    }

    u32 size = kernel_size;
    u32 chksum = kernel_chksum;

    kernel_chksum = 0;
    kernel_size = 0;

    u32 result = eth_fcs((void *)kernel_addr, size);
    if (result != chksum)
    {
        show_message(crc32_msg, sizeof(crc32_msg));
        goto failure;
    }
    return EOK;
failure:
    hang();
}
