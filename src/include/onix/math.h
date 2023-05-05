#ifndef ONIX_MATH_H
#define ONIX_MATH_H

#include <onix/types.h>

#define NAN (0.0f / 0.0f)

// 当小数部分为全 0 时，表示一个 quiet NaN
extern const unsigned int _QNAN_F;
#define NANF (*((float *)(&_QNAN_F)))

// 当小数部分非全 0 时，表示一个 signaling NaN
extern const unsigned int _SNAN_F;
#define NANSF (*((float *)(&_SNAN_F)))

extern const unsigned int _SNAN_D[2];
#define NANS (*((double *)(&_SNAN_D)))

// ISO C99 function nan.
#define __builtin_nan(__dummy) NAN
#define __builtin_nanf(__dummy) NANF
#define __builtin_nans(__dummy) NANS
#define __builtin_nansf(__dummy) NANSF

#define INF (1.0f / 0.0f)

#define __builtin_infinity(__dummy) INF

// 弧度制
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);

// x 开方
double sqrt(double x);

// x 的自然对数(基数为 e 的对数)
double log(double x);

double log2(double x);

double log10(double x);

// x 的绝对值
double fabs(double x);

double fmax(double x, double y);

double fmin(double x, double y);

// x 分解成尾数和指数
// 返回值是尾数，并将指数存入 exp 中
double frexp(double x, int *exp);

// x 舍尾取整，或称为向零取整
double trunc(double x);

// x 舍入到最接近的整数，在中间情况下舍入到零
double round(double x);

// x / y 的余数
// 设 r = x – ny , 其中 n 是接近 x/y 实际值的整数，返回 r
double remainder(double x, double y);

// e ^ x
double exp(double x);

// 2 ^ x
double exp2(double x);

// (e ^ x) - 1
double expm1(double x);

// x ^ (1/3)
double cbrt(double x);

// x ^ y
double pow(double x, double y);

// 取 x 的值和 y 的符号组成一个值
double copysign(double x, double y);

// x - y 的正差
// 如果 x > y 返回 x - y ，否则返回0
double fdim(double x, double y);

// x * (2 ^ n)
// n 必须是 -32768 到 32768 之间
double ldexp(double x, int n);

// x 向下取整
double floor(double x);

// >= x 的最小整数
double ceil(double x);

// 返回 x / y 的余数
double fmod(double x, double y);

// 判断 x 是否非数字
bool isnan(double x);

// 判断 x 是否有限
bool isfinite(double x);

// 判断 x 是否无穷
bool isinf(double x);

// 判断 x 是否正规值
bool isnormal(double x);

#endif