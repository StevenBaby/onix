#include <onix/onix.h>
#include <onix/types.h>
#include <onix/io.h>
#include <onix/string.h>
#include <onix/console.h>
#include <onix/stdarg.h>

void test_args(int cnt, ...)
{
    va_list args;
    va_start(args, cnt);

    int arg;
    while (cnt--)
    {
        arg = va_arg(args, int);
    }
    va_end(args);
}

void kernel_init()
{
    console_init();
    test_args(5, 1, 0xaa, 5, 0x55, 10);
    return;
}
