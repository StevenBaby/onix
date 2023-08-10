#include <onix/types.h>
#include <onix/vm86.h>
#include <onix/string.h>
#include <onix/syscall.h>
#include <onix/memory.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/fs.h>
#include <onix/stdlib.h>
#include <onix/task.h>
#include <onix/mouse.h>
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
static u32 *screen;
static u32 mode_number;
static task_t *vesa_task;

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

    screen = (u32 *)mode->framebuffer;

    mode_number = *mode_list | VESA_LBF;
    mode_handler = *mode;
    return EOK;
}

void vesa_update_background()
{
    u32 *screen = (u32 *)mode_handler.framebuffer;
    for (size_t x = 0; x < VESA_WIDTH; x++)
    {
        for (size_t y = 0; y < VESA_HEIGHT; y++)
        {
            screen[y * VESA_WIDTH + x] = ((x ^ y) * 104729);
            // screen[y * VESA_WIDTH + x] = 0x333333;
        }
    }
}

void vesa_reset()
{
    err_t ret;
    vm86_reg_t reg;

    reg.ax = VESA_SET_MODE;
    reg.bx = mode_number;

    ret = vm86(0x10, &reg);
    assert(reg.ax == VESA_SUCCESS);
    vesa_update_background();
}

u32 *mouse_image = NULL;
static int X = 0;
static int Y = 0;

#define MOUSE_WIDTH 20
#define MOUSE_HEIGHT 20

static void vesa_show_mouse()
{
    for (size_t i = 0; i < MOUSE_WIDTH; i++)
    {
        for (size_t j = 0; j < MOUSE_HEIGHT; j++)
        {
            pixel_t *pix = (pixel_t *)&mouse_image[j * MOUSE_WIDTH + i];
            if (*(int *)pix == 0)
                continue;
            screen[(Y + j) * VESA_WIDTH + (X + i)] = mouse_image[j * MOUSE_WIDTH + i];
        }
    }
}

u32 vesa_update_position(int x, int y)
{
    // if (ABS(x) > 50 || ABS(y) > 50)
    //     return 0;
    X += x;
    Y -= y;

    if (X < 0)
        X = 0;
    if (X >= VESA_WIDTH - MOUSE_WIDTH)
        X = VESA_WIDTH - MOUSE_WIDTH - 1;
    if (Y < 0)
        Y = 0;
    if (Y >= VESA_HEIGHT - MOUSE_HEIGHT)
        Y = VESA_HEIGHT - MOUSE_HEIGHT - 1;
    // LOGK("update mouse position (%d, %d)\n", X, Y);
    vesa_show_mouse();
}

static err_t vesa_init_mouse()
{
    fd_t fd = open("/data/mouse.bmp", O_RDWR, 0755);
    if (fd < EOK)
        return -ERROR;

    char *buf = (char *)alloc_kpage(1);
    assert(lseek(fd, 0, SEEK_SET) == 0);
    int len = read(fd, buf, 128);
    assert(len = 128);

    bmp_hdr_t *hdr = (bmp_hdr_t *)buf;
    int start = hdr->start;
    assert(lseek(fd, start, SEEK_SET) == start);
    assert(hdr->size < PAGE_SIZE);

    int size = hdr->size;
    assert(read(fd, buf, size) > 0);

    close(fd);
    mouse_image = (u32 *)buf;

    for (size_t i = 0; i < MOUSE_WIDTH; i++)
    {
        for (size_t j = 0; j < MOUSE_HEIGHT; j++)
        {
            pixel_t *pix = (pixel_t *)&mouse_image[j * MOUSE_WIDTH + i];
            pix->alpha = 0;
        }
    }

    vesa_show_mouse();
    LOGK("init mouse image finished!!!\n");
}

// void vesa_show()
// {
//     fd_t fd = open("/data/taklimakan.bmp", O_RDWR | O_CREAT, 0755);
//     if (fd < EOK)
//         return;

//     char *buf = (char *)alloc_kpage(1);
//     assert(lseek(fd, 0, SEEK_SET) == 0);
//     int len = read(fd, buf, 1078);
//     assert(len = 1078);

//     bmp_hdr_t *hdr = (bmp_hdr_t *)buf;

//     pixel_t *screen = (pixel_t *)mode_handler.framebuffer;

//     int next = hdr->start;
//     LOGK("screen 0x%p\n", screen);
//     for (size_t i = 0; i < VESA_WIDTH * VESA_HEIGHT;)
//     {
//         assert(lseek(fd, next, SEEK_SET) == next);
//         int len = read(fd, buf, PAGE_SIZE);
//         next += (len / 3 * 3);
//         rgb_t *table = (rgb_t *)buf;
//         for (size_t j = 0; j < len / 3; j++, i++)
//         {
//             screen[i].color = table[j];
//         }
//     }

//     free_kpage((u32)buf, 1);
//     close(fd);

//     LOGK("show image finished!!!\n");
// }

void vesa_thread()
{
    fd_t fd = open("/dev/mouse", O_RDONLY, 0);
    assert(fd > 0);
    mpacket_t pkt;
    while (true)
    {
        int len = read(fd, (void *)&pkt, sizeof(mpacket_t));
        if (len < 0)
        {
            sleep(10);
            continue;
        }
        // LOGK("mouse move (%d, %d) buttons %d %d\n", pkt.x, pkt.y, pkt.buttons, len);
        // vesa_update_background();
        vesa_update_position(pkt.x, pkt.y);
    }
}

void vesa_init()
{
    LOGK("vesa init...\n");
    assert(vesa_set_mode(VESA_WIDTH, VESA_HEIGHT, VESA_BBP) == EOK);
    vesa_update_background();
    vesa_init_mouse();
    vesa_task = task_create(vesa_thread, "vesa", 5, KERNEL_USER);
}