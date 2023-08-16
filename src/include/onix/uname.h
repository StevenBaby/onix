#ifndef ONIX_UNAME_H
#define ONIX_UNAME_H

#include <onix/types.h>

typedef struct utsname_t
{
    char sysname[9];  // 本版本操作系统的名称
    char nodename[9]; // 与实现相关的网络中节点名称
    char release[9];  // 本实现的当前发行级别
    char version[9];  // 本次发行的版本级别
    char machine[9];  // 系统运行的硬件类型名称
} utsname_t;

#endif