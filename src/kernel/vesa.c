#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

void vesa_init()
{
    LOGK("vesa init...\n");
}