#include <onix/types.h>
#include <onix/vga.h>
#include <onix/stdlib.h>
#include <onix/fs.h>
#include <onix/memory.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/syscall.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define MEM_BASE 0xA0000
#define MEM_SIZE 0x10000

#define WIDTH 320
#define HEIGHT 200

void vga_reset()
{
    // for (size_t i = MEM_BASE; i < (MEM_BASE + WIDTH * HEIGHT); i++)
    // {
    //     *(char *)i = i & 0x0F;
    // }
}

void vga_show()
{
    fd_t fd = open("/data/bingbing.bmp", O_RDWR | O_CREAT, 0755);
    if (fd < EOK)
        return;

    char *buf = (char *)alloc_kpage(1);
    assert(lseek(fd, 0, SEEK_SET) == 0);
    int len = read(fd, buf, 1078);
    assert(len = 1078);

    bmp_hdr_t *hdr = (bmp_hdr_t *)buf;
    assert(hdr->start == 1078);

    bmp_info_t *info = (bmp_info_t *)(buf + sizeof(bmp_hdr_t));

    for (size_t i = 0; i < (320 * 200);)
    {
        int len = read(fd, buf, PAGE_SIZE);

        for (int j = 0; j < len; j++, i++)
        {
            *(char *)(MEM_BASE + i) = buf[j];
        }
    }

    free_kpage((u32)buf, 1);
    close(fd);

    LOGK("show image finished!!!\n");
}

void vga_init()
{
}