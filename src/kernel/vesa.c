#include <onix/types.h>
#include <onix/vm86.h>
#include <onix/string.h>
#include <onix/syscall.h>
#include <onix/memory.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/fs.h>
#include <onix/vga.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define VESA_SUCCESS 0x004F
#define VESA_BUFFER 0x83000

#define VESA_MODE_END 0xFFFF

#define VESA_WIDTH 1024
#define VESA_HEIGHT 768
#define VESA_BBP 32
#define VESA_LBF (1 << 14)

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

static vbe_mode_t mode_handler;

static err_t vesa_set_mode(u32 width, u32 height, u32 bpp)
{
    err_t ret;
    vm86_reg_t reg;

    reg.ax = VESA_INFO;
    reg.es = (VESA_BUFFER >> 4);
    reg.di = 0;
    ret = vm86(0x10, &reg);

    assert(reg.ax == VESA_SUCCESS);
    vbe_info_t *info = (vbe_info_t *)VESA_BUFFER;
    assert(memcmp(info->signature, "VESA", 4) == 0);

    vbe_mode_t *mode = (vbe_mode_t *)(VESA_BUFFER + sizeof(vbe_info_t));
    u16 *mode_list = (u16 *)((((info->mode_ptr >> 16) & 0xFFFF) << 4) + (info->mode_ptr & 0xFFFF));
    for (; *mode_list != VESA_MODE_END; mode_list++)
    {
        reg.ax = VESA_GET_MODE;
        reg.es = (VESA_BUFFER >> 4);
        reg.di = sizeof(vbe_info_t);
        reg.cx = *mode_list;

        ret = vm86(0x10, &reg);

        assert(reg.ax == VESA_SUCCESS);
        if (mode->width != width)
            continue;
        if (mode->height != height)
            continue;
        if (mode->bpp != bpp)
            continue;
        goto success;
    }
    return -EVESA;
success:

    assert((mode->framebuffer & 0xfff) == 0);
    int size = (width * height * bpp) / 8;
    map_area(mode->framebuffer, size);

    reg.ax = VESA_SET_MODE;
    reg.bx = *mode_list | VESA_LBF;

    ret = vm86(0x10, &reg);
    assert(reg.ax == VESA_SUCCESS);

    mode_handler = *mode;
    return EOK;
}

void vesa_reset()
{
    u32 *screen = (u32 *)mode_handler.framebuffer;
    LOGK("screen 0x%p\n", screen);
    for (size_t x = 0; x < VESA_WIDTH; x++)
    {
        for (size_t y = 0; y < VESA_HEIGHT; y++)
        {
            screen[y * VESA_WIDTH + x] = 0x0000FF00;
        }
    }
}

void vesa_show()
{
    fd_t fd = open("/data/taklimakan.bmp", O_RDWR | O_CREAT, 0755);
    if (fd < EOK)
        return;

    char *buf = (char *)alloc_kpage(1);
    assert(lseek(fd, 0, SEEK_SET) == 0);
    int len = read(fd, buf, 1078);
    assert(len = 1078);

    bmp_hdr_t *hdr = (bmp_hdr_t *)buf;

    pixel_t *screen = (pixel_t *)mode_handler.framebuffer;

    int next = hdr->start;
    LOGK("screen 0x%p\n", screen);
    for (size_t i = 0; i < VESA_WIDTH * VESA_HEIGHT;)
    {
        assert(lseek(fd, next, SEEK_SET) == next);
        int len = read(fd, buf, PAGE_SIZE);
        next += (len / 3 * 3);
        rgb_t *table = (rgb_t *)buf;
        for (size_t j = 0; j < len / 3; j++, i++)
        {
            screen[i].color = table[j];
        }
    }

    free_kpage((u32)buf, 1);
    close(fd);

    LOGK("show image finished!!!\n");
}

void vesa_init()
{
    LOGK("vesa init...\n");
    assert(vesa_set_mode(VESA_WIDTH, VESA_HEIGHT, VESA_BBP) == EOK);
    vesa_reset();
}