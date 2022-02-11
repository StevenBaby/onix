#include <onix/onix.h>
#include <onix/types.h>
#include <onix/io.h>
#include <onix/string.h>
#include <onix/console.h>

char message[] = "hello onix!!!\n";
char buf[1024];

void kernel_init()
{
    console_init();
    while (true)
    {
        console_write(message, sizeof(message) - 1);
    }

    return;
}
