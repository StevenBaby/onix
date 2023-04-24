#ifndef ONIX_FPU_H
#define ONIX_FPU_H

#include <onix/types.h>
#include <onix/task.h>

enum
{
    CR0_PE = 1 << 0,  // Protection Enable 启用保护模式
    CR0_MP = 1 << 1,  // Monitor Coprocessor
    CR0_EM = 1 << 2,  // Emulation 启用模拟，表示没有 FPU
    CR0_TS = 1 << 3,  // Task Switch 任务切换，延迟保存浮点环境
    CR0_ET = 1 << 3,  // Extension Type 保留
    CR0_NE = 1 << 5,  // Numeric Error 启用内部浮点错误报告
    CR0_WP = 1 << 16, // Write Protect 写保护（禁止超级用户写入只读页）帮助写时复制
    CR0_AM = 1 << 18, // Alignment Mask 对齐掩码
    CR0_NW = 1 << 29, // Not Write-Through 不是直写
    CR0_CD = 1 << 30, // Cache Disable 禁用内存缓冲
    CR0_PG = 1 << 31, // Paging 启用分页
};

// Intel® 64 and IA-32 Architectures Software Developer's Manual
// Figure 8-9. Protected Mode x87 FPU State Image in Memory, 32-Bit Format

typedef struct fpu_t
{
    u16 control;
    u16 RESERVED;
    u16 status;
    u16 RESERVED;
    u16 tag;
    u16 RESERVED;
    u32 fip0;
    u32 fop0;
    u32 fdp0;
    u32 fdp1;
    u8 regs[80];
} _packed fpu_t;

bool fpu_check();
void fpu_disable(task_t *task);
void fpu_enable(task_t *task);

typedef enum fpu_examine_t
{
    // FPU Status Word 相关
    FPU_EXAMINE_UNSPPORTED = 0x0000,
    FPU_EXAMINE_NAN = 0x0100, // Not a number
    FPU_EXAMINE_NORMAL = 0x0400,
    FPU_EXAMINE_INFINITY = 0x0500,
    FPU_EXAMINE_ZERO = 0x4000,
    FPU_EXAMINE_EMPTY = 0x4100,
    FPU_EXAMINE_DENORMAL = 0x4400,
    FPU_EXAMINE_REVERSED = 0x4500,
} fpu_examine_t;

// 浮点数检查
static inline fpu_examine_t fxam(bool *sign, double x)
{
    u16 fsw; // fpu status word
    asm volatile(
        "fxam \n"
        "fstsw %%ax \n"
        : "=a"(fsw)
        : "t"(x));
    *sign = fsw & 0x0200; // C1
    return (fpu_examine_t)(fsw & 0x4500);
}

// 得到圆周率 π
noglobal static inline double fldpi(void)
{
    double ret;
    asm volatile(
        "fldpi \n"
        : "=&t"(ret));
    return ret;
}

// x / y 的余数
static inline double fremainder(bool (*bits)[3], double x, double y)
{
    double ret;
    u16 fsw;
    asm volatile(
        "1: \n"
        "  fprem1 \n"
        "  fstsw %%ax \n"
        "  testb $0x04, %%ah \n"
        "  jnz 1b \n"
        : "=&t"(ret), "=a"(fsw)
        : "0"(x), "u"(y));
    (*bits)[2] = fsw & 0x0100; // C0
    (*bits)[1] = fsw & 0x4000; // C3
    (*bits)[0] = fsw & 0x0200; // C1
    return ret;
}

// 返回 -x
static inline double fchs(double x)
{
    double ret;
    asm volatile(
        "fchs \n"
        : "=&t"(ret)
        : "0"(x));
    return ret;
}

// 返回 +0.0
noglobal static inline double fldz(void)
{
    double ret;
    asm volatile(
        "fldz \n"
        : "=&t"(ret));
    return ret;
}

static inline double fsin(double x)
{
    double ret;
    asm volatile(
        "fsin \n"
        : "=&t"(ret)
        : "0"(x));
    return ret;
}

static inline double fcos(double x)
{
    double ret;
    asm volatile(
        "fcos \n"
        : "=&t"(ret)
        : "0"(x));
    return ret;
}

static inline double ftan(double x)
{
    double one, ret;
    asm volatile(
        "fptan \n"
        : "=&t"(one), "=&u"(ret)
        : "0"(x));
    return ret;
}

// 计算 arctan(y / x)
static inline double fpatan(double y, double x)
{
    double ret;
    asm volatile(
        "fpatan \n"
        : "=&t"(ret)
        : "0"(x), "u"(y)
        : "st(1)");
    return ret;
}

// 对 x 开方
static inline double fsqrt(double x)
{
    double ret;
    asm volatile(
        "fsqrt \n"
        : "=&t"(ret)
        : "0"(x));
    return ret;
}

// x ^ 2
static inline double fsquare(double x)
{
    double ret;
    asm volatile(
        "fmul %%st \n"
        : "=&t"(ret)
        : "0"(x));
    return ret;
}

// 计算 y ∗ log2(x)
static inline double fyl2x(double y, double x)
{
    double ret;
    asm volatile(
        "fyl2x \n"
        : "=&t"(ret)
        : "0"(x), "u"(y)
        : "st(1)");
    return ret;
}

// 得到 loge(2)
noglobal static inline double fldln2(void)
{
    double ret;
    asm volatile(
        "fldln2 \n"
        : "=&t"(ret));
    return ret;
}

// 得到 log2(e)
noglobal static inline double fldl2e(void)
{
    double ret;
    asm volatile(
        "fldl2e \n"
        : "=&t"(ret));
    return ret;
}

// 将 x 舍入为整数
static inline double frndintany(double x)
{
    double ret;
    asm volatile(
        "frndint \n"
        : "=&t"(ret)
        : "0"(x));
    return ret;
}

// 得到 x * (2 ^ n)
// n 必须是 -32768 到 32768 之间
static inline double fscale(double x, double n)
{
    double ret;
    asm volatile(
        "fscale \n"
        : "=&t"(ret)
        : "0"(x), "u"(n));
    return ret;
}

// 得到 x % y
static inline double ffmod(bool (*bits)[3], double x, double y)
{
    double ret;
    u16 fsw;
    asm volatile(
        "1: \n"
        "  fprem \n"
        "  fstsw %%ax \n"
        "  testb $0x04, %%ah \n"
        "  jnz 1b \n"
        : "=&t"(ret), "=a"(fsw)
        : "0"(x), "u"(y));
    (*bits)[2] = fsw & 0x0100;
    (*bits)[1] = fsw & 0x4000;
    (*bits)[0] = fsw & 0x0200;
    return ret;
}

// 得到 2 ^ x - 1
static inline double f2xm1(double x)
{
    double ret;
    asm volatile(
        "f2xm1 \n"
        : "=&t"(ret)
        : "0"(x));
    return ret;
}

// 提取指数和有效值
// 设返回值为 m，则有 x = m * (2 ^ pn)
static inline double fxtract(double *pn, double x)
{
    double ret;
    asm volatile(
        "fxtract \n"
        : "=&t"(ret), "=&u"(*pn)
        : "0"(x));
    return ret;
}

// 把 x 转成 int 存入 p 中，并将 ST(0) 出栈
static inline void fistp(int *p, double x)
{
    asm volatile(
        "fistp %0 \n"
        : "=m"(*p)
        : "t"(x)
        : "st");
}

// 获取 log10(2)
noglobal static inline double fldlg2(void)
{
    double ret;
    asm volatile(
        "fldlg2 \n"
        : "=&t"(ret));
    return ret;
}

// 截断 x 的小数部分，保留整数
static inline double ftrunc(double x)
{
    double ret;
    u16 fcw;
    asm volatile(
        "fstcw %1 \n"
        "movzx %1, %%ecx  \n" // 保存 fcw
        "movl %%ecx, %%edx  \n"
        "or $0x0c00, %%edx \n" // 设置本次运算的 fcw
        "movw %%dx, %1 \n"
        "fldcw %1 \n"
        "frndint \n"
        "movw %%cx, %1 \n"
        "fldcw %1 \n" // 恢复 fcw
        : "=&t"(ret), "=m"(fcw)
        : "0"(x)
        : "cx", "dx");
    return ret;
}

// x 向下取整
static inline double ffloor(double x)
{
    double ret;
    u16 fcw;
    asm volatile(
        "fstcw %1 \n"
        "movzx %1, %%ecx \n"
        "movl %%ecx, %%edx \n"
        "and $0xf3ff, %%edx \n"
        "or $0x0400, %%edx \n"
        "movw %%dx, %1 \n"
        "fldcw %1 \n"
        "frndint \n"
        "movw %%cx, %1 \n"
        "fldcw %1 \n"
        : "=&t"(ret), "=m"(fcw)
        : "0"(x)
        : "cx", "dx");
    return ret;
}

// >= x 的最小整数
static inline double fceil(double x)
{
    double ret;
    u16 fcw;
    asm volatile(
        "fstcw %1 \n"
        "movzx %1, %%ecx \n"
        "movl %%ecx, %%edx \n"
        "and $0xf3ff, %%edx \n"
        "or $0x0800, %%edx \n"
        "movw %%dx, %1 \n"
        "fldcw %1 \n"
        "frndint \n"
        "movw %%cx, %1 \n"
        "fldcw %1 \n"
        : "=&t"(ret), "=m"(fcw)
        : "0"(x)
        : "cx", "dx");
    return ret;
}

#endif