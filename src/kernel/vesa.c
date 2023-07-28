#include <onix/types.h>
#include <onix/vm86.h>
#include <onix/string.h>
#include <onix/syscall.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define VESA_SUCCESS 0x004F
#define VESA_BUFFER 0x83000

#define VESA_MODE_END 0xFFFF

enum
{
    VESA_INFO = 0x4F00,
    VESA_GET_MODE = 0x4F01,
    VESA_SET_MODE = 0x4F02,
    VESA_CURRENT_MODE = 0x4F03,
    VESA_PROTECTED_INTERFACE = 0x4F0A,
};

typedef struct vbe_info_t
{
    char signature[4];     // must be "VESA" to indicate valid VBE support
    u16 version;           // VBE version; high byte is major version, low byte is minor version
    u32 oem;               // segment:offset pointer to OEM
    u32 capabilities;      // bitfield that describes card capabilities
    u32 mode_ptr;          // segment:offset pointer to list of supported video modes
    u16 total_memory;      // amount of video memory in 64KB blocks
    u16 software_revision; // software revision
    u32 vendor;            // segment:offset to card vendor string
    u32 product_name;      // segment:offset to card model name
    u32 product_revision;  // segment:offset pointer to product revision
    char reserved[222];    // reserved for future expansion
    char oem_data[256];    // OEM BIOSes store their strings in this area
} _packed vbe_info_t;

typedef struct vbe_mode_t
{
    u16 attributes;  // deprecated, only bit 7 should be of interest to you, and it indicates the mode supports a linear frame buffer.
    u8 window_a;     // deprecated
    u8 window_b;     // deprecated
    u16 granularity; // deprecated; used while calculating bank numbers
    u16 window_size;
    u16 segment_a;
    u16 segment_b;
    u32 win_func_ptr; // deprecated; used to switch banks from protected mode without returning to real mode
    u16 pitch;        // number of bytes per horizontal line
    u16 width;        // width in pixels
    u16 height;       // height in pixels
    u8 w_char;        // unused...
    u8 y_char;        // ...
    u8 planes;
    u8 bpp;   // bits per pixel in this mode
    u8 banks; // deprecated; total number of banks in this mode
    u8 memory_model;
    u8 bank_size; // deprecated; size of a bank, almost always 64 KB but may be 16 KB...
    u8 image_pages;
    u8 reserved0;

    u8 red_mask;
    u8 red_position;
    u8 green_mask;
    u8 green_position;
    u8 blue_mask;
    u8 blue_position;
    u8 reserved_mask;
    u8 reserved_position;
    u8 direct_color_attributes;

    u32 framebuffer; // physical address of the linear frame buffer; write here to draw to the screen
    u32 off_screen_mem_off;
    u16 off_screen_mem_size; // size of memory in the framebuffer but not being displayed on the screen
    u8 reserved1[206];
} _packed vbe_mode_t;

void vesa_init()
{
    LOGK("vesa init...\n");

    err_t ret;
    vm86_reg_t reg;

    reg.ax = VESA_INFO;
    reg.es = (VESA_BUFFER >> 4);
    reg.di = 0;
    ret = vm86(0x10, &reg);

    assert(reg.ax == VESA_SUCCESS);
    vbe_info_t *info = (vbe_info_t *)VESA_BUFFER;
    assert(memcmp(info->signature, "VESA", 4) == 0);

    u16 *mode_list = (u16 *)((((info->mode_ptr >> 16) & 0xFFFF) << 4) + (info->mode_ptr & 0xFFFF));
    while (*mode_list != VESA_MODE_END)
    {
        LOGK("find vesa mode 0x%04X\n", *mode_list);
        reg.ax = VESA_GET_MODE;
        reg.es = (VESA_BUFFER >> 4);
        reg.di = sizeof(vbe_info_t);
        reg.cx = *mode_list;

        ret = vm86(0x10, &reg);

        assert(reg.ax == VESA_SUCCESS);
        vbe_mode_t *mode = (vbe_mode_t *)(VESA_BUFFER + sizeof(vbe_info_t));
        LOGK("    w:%d h:%d bpp:%d frame 0x%p\n", mode->width, mode->height, mode->bpp, mode->framebuffer);
        mode_list++;
    }

    int i = 6;
}