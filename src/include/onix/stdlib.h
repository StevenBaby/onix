#ifndef ONIX_STDLIB_H
#define ONIX_STDLIB_H

#include <onix/types.h>

#define MAX(a, b) (a < b ? b : a)
#define MIN(a, b) (a < b ? a : b)

#define ALIGN(d, a) (((d) + (a - 1)) & ~(a - 1))

void delay(u32 count);
void hang();

u8 bcd_to_bin(u8 value);
u8 bin_to_bcd(u8 value);

u32 div_round_up(u32 num, u32 size);

int atoi(const char *str);

void srand(u32 seed);

u32 rand();

#endif