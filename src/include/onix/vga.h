#ifndef ONIX_VGA_H
#define ONIX_VGA_H

#include <onix/types.h>

typedef struct bmp_hdr_t
{
    u16 type;     // 位图文件的类型，必须为BMP (2个字节)
    u32 size;     // 位图文件的大小，以字节为单位 (4个字节)
    u16 RESERVED; // 位图文件保留字，必须为0 (2个字节)
    u16 RESERVED; // 位图文件保留字，必须为0 (2个字节)
    u32 start;    // 位图数据的起始位置，以相对于位图 (4个字节)
} _packed bmp_hdr_t;

typedef struct bmp_info_t
{
    u32 info_size;       // 本结构所占用字节数  (4个字节)
    int width;           // 位图的宽度，以像素为单位(4个字节)
    int height;          // 位图的高度，以像素为单位(4个字节)
    u16 plane;           // 目标设备的级别，必须为1(2个字节)
    u16 count;           // 每个像素所需的位数，必须是1(双色)、
                         // 4(16色)、8(256色)、
                         // 24(真彩色)或32(增强真彩色)之一 (2个字节)
    u32 comp;            // 位图压缩类型，必须是 0(不压缩)、 1(BI_RLE8
                         // 压缩类型)或2(BI_RLE4压缩类型)之一 ) (4个字节)
    u32 image_size;      // 位图的大小，以字节为单位(4个字节)
    u32 x_pix;           // 位图水平分辨率，每米像素数(4个字节)
    u32 y_pix;           // 位图垂直分辨率，每米像素数(4个字节)
    u32 used_color;      // 位图实际使用的颜色表中的颜色数(4个字节)
    u32 important_color; // 位图显示过程中重要的颜色数(4个字节)
} _packed bmp_info_t;

typedef struct rgb_t
{
    u8 B; // 蓝色的亮度(值范围为0-255)
    u8 G; // 绿色的亮度(值范围为0-255)
    u8 R; // 红色的亮度(值范围为0-255)
} _packed rgb_t;

typedef struct pixel_t
{
    union
    {
        struct
        {
            u8 B; // 蓝色的亮度(值范围为0-255)
            u8 G; // 绿色的亮度(值范围为0-255)
            u8 R; // 红色的亮度(值范围为0-255)
        };
        rgb_t color;
    };
    u8 alpha;
} _packed pixel_t;

#endif