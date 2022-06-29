#ifndef ONIX_STDIO_H
#define ONIX_STDIO_H

#include <onix/stdarg.h>

int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);
int printf(const char *fmt, ...);

#endif