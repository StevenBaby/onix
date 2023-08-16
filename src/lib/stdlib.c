#include <onix/stdlib.h>

// 延迟
void delay(u32 count)
{
    while (count--)
        ;
}

// 阻塞函数
void hang()
{
    while (true)
        ;
}

char toupper(char ch)
{
    if (ch >= 'a' && ch <= 'z')
        ch -= 0x20;
    return ch;
}

char tolower(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        ch += 0x20;
    return ch;
}

// 将 bcd 码转成整数
u8 bcd_to_bin(u8 value)
{
    return (value & 0xf) + (value >> 4) * 10;
}

// 将整数转成 bcd 码
u8 bin_to_bcd(u8 value)
{
    return (value / 10) * 0x10 + (value % 10);
}

// 计算 num 分成 size 的数量
u32 div_round_up(u32 num, u32 size)
{
    return (num + size - 1) / size;
}

// 判断是否是数字
bool isdigit(int c)
{
    return c >= '0' && c <= '9';
}

int atoi(const char *str)
{
    if (str == NULL)
        return 0;
    int sign = 1;
    int result = 0;
    if (*str == '-')
    {
        sign = -1;
        str++;
    }
    for (; *str; str++)
    {
        result = result * 10 + (*str - '0');
    }
    return result * sign;
}
