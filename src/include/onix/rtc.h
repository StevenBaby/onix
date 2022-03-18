#ifndef ONIX_RTC_H
#define ONIX_RTC_H

void set_alarm(u32 secs);
u8 cmos_read(u8 addr);
void cmos_write(u8 addr, u8 value);

#endif