#ifndef ONIX_MOUSE_H
#define ONIX_MOUSE_H

#include <onix/types.h>
#include <onix/list.h>

enum
{
    MOUSE_LEFT_CLICK = 0x1,
    MOUSE_RIGHT_CLICK = 0x2,
    MOUSE_MIDDLE_CLICK = 0x4,
    MOUSE_SCROLL_UP = 0x10,
    MOUSE_SCROLL_DOWN = 0x20,
};

typedef struct mpacket_t
{
    int x;
    int y;
    int buttons;
} mpacket_t;

#endif