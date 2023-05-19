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

static u32 seed_ = 1;             // 默认种子值
static const u32 M = 0x7fffffffu; // M 表示周期，2^31-1
static const u32 A = 16807;       // bits 14, 8, 7, 5, 2, 1, 0

void srand(u32 seed)
{
    // 保证 seed_ 在区间 [0, M] 内
    seed_ = seed & M;

    if (seed_ == 0 || seed_ == M)
    {
        // 种子值不能为 0 或 M，否则 seed_ 的值会一直不变
        seed_ = 1;
    }
}

u32 rand()
{
    // 线性同余生成器(LCG)
    // seed_ = (seed_ * A) % M
    u64 product = seed_ * A;

    // 取 M = 2^31-1，存在一个等式 (x << 31) % M == (x * (M + 1)) % M == x
    // 记 product 的高 33 位为 H，低 31 位为 L
    // product % M = (H << 31 + L) % M
    //             = (H << 31) % M + L % M
    //             = H + L
    //             = (product >> 31) + (product & M)
    // 这样可以避免求余的除法运算
    seed_ = (u32)((product >> 31) + (product & M));

    if (seed_ > M)
    {
        seed_ -= M;
    }

    return seed_;
}
