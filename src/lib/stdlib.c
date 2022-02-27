#include <onix/stdlib.h>

void delay(u32 count)
{
    while (count--)
        ;
}

void hang()
{
    while (true)
        ;
}
