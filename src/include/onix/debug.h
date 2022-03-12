
#ifndef ONIX_DEBUG_H
#define ONIX_DEBUG_H

void debugk(char *file, int line, const char *fmt, ...);

#define BMB asm volatile("xchgw %bx, %bx") // bochs magic breakpoint
#define DEBUGK(fmt, args...) debugk(__BASE_FILE__, __LINE__, fmt, ##args)

// #define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#endif
