#ifndef ONIX_STDLIB_H
#define ONIX_STDLIB_H

#include <onix/types.h>

void delay(u32 count);
void hang();

u8 bcd_to_bin(u8 value);
u8 bin_to_bcd(u8 value);

#endif