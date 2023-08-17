#ifndef ONIX_VM86_H
#define ONIX_VM86_H

#include <onix/types.h>

typedef struct vm86_reg_t
{
    u16 es;
    u16 ds;
    u16 bp;
    u16 di;
    u16 si;
    u16 dx;
    u16 cx;
    u16 bx;
    u16 ax;
} _packed vm86_reg_t;

#endif