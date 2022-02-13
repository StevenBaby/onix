#include <onix/onix.h>
#include <onix/types.h>
#include <onix/io.h>
#include <onix/string.h>
#include <onix/console.h>
#include <onix/printk.h>
#include <onix/assert.h>
#include <onix/debug.h>

void kernel_init()
{
    console_init();

    BMB;

    DEBUGK("debug onix!!!\n");

    return;
}
