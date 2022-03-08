#ifndef ONIX_TIME_H
#define ONIX_TIME_H

#include <onix/types.h>

typedef struct tm
{
    int tm_sec;   // 秒数 [0，59]
    int tm_min;   // 分钟数 [0，59]
    int tm_hour;  // 小时数 [0，59]
    int tm_mday;  // 1 个月的天数 [0，31]
    int tm_mon;   // 1 年中月份 [0，11]
    int tm_year;  // 从 1900 年开始的年数
    int tm_wday;  // 1 星期中的某天 [0，6] (星期天 =0)
    int tm_yday;  // 1 年中的某天 [0，365]
    int tm_isdst; // 夏令时标志
} tm;

void time_read_bcd(tm *time);
void time_read(tm *time);
time_t mktime(tm *time);

#endif