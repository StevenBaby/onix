#include <onix/math.h>
#include <onix/types.h>
#include <onix/fpu.h>
#include <onix/limits.h>

#define FPU_EXAMINE_NAN_RETURN(x, y)                   \
    bool x##sign;                                      \
    const fpu_examine_t x##exam = fxam(&(x##sign), x); \
    if (x##exam == FPU_EXAMINE_NAN)                    \
    {                                                  \
        return y;                                      \
    }

double sin(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    // Taylor series expansion is another way.
    return fsin(x);
}

double cos(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    return fcos(x);
}

double tan(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    return ftan(x);
}

double asin(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    // 由于 fpu 没有直接计算 arcsin 的微指令
    // 不妨设直角三角形斜边为 1 ，θ 角的对边为 x，那么根据 tan 函数计算公式 tan(θ) = x / sqrt(1 - square(x))
    // 可以用 arctan(x / sqrt(1 - square(x))) 得到 θ
    return fpatan(x, fsqrt(1 - fsquare(x)));
}

double acos(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    return fpatan(fsqrt(1 - fsquare(x)), x);
}

double atan(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    return fpatan(x, 1);
}

double sqrt(double x)
{
    return fsqrt(x);
}

double log(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    // 换底公式
    return fyl2x(fldln2(), x);
}

double log2(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    return fyl2x(1, x);
}

double log10(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    // 换底公式
    return fyl2x(fldlg2(), x);
}

double fabs(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    double ret;
    asm volatile(
        "fabs \n"
        : "=&t"(ret)
        : "0"(x));
    return ret;
}

double fmax(double x, double y)
{
    FPU_EXAMINE_NAN_RETURN(x, y)
    FPU_EXAMINE_NAN_RETURN(y, x)
    return (x > y) ? x : y;
}

double fmin(double x, double y)
{
    FPU_EXAMINE_NAN_RETURN(x, y)
    FPU_EXAMINE_NAN_RETURN(y, x)
    return (x < y) ? x : y;
}

// 把浮点数 x 分解成尾数和指数
// 返回值是尾数，并将指数存入 exp 中
double frexp(double x, int *exp)
{
    bool sign;
    const fpu_examine_t exam = fxam(&sign, x);
    if (exam == FPU_EXAMINE_NAN)
    {
        *exp = (int)0xDEADBEEF;
        return x;
    }
    if (exam == FPU_EXAMINE_ZERO)
    {
        *exp = 0;
        return x;
    }
    if (exam == FPU_EXAMINE_INFINITY)
    {
        *exp = INT_MAX;
        return x;
    }
    double n;
    // x = m * (2 ^ n)
    const double m = fxtract(&n, x);
    // 上面得到的 m >= 1.0
    // 比如 float 的规格化尾数二进制 0.1xxxxxx... 小数点后共 23 位
    // 需要把尾数规格化成 [0.5, 1.0) 内
    // 指数+1，尾数/2即可
    fistp(exp, n + 1);
    return m / 2;
}

double trunc(double x)
{
    return ftrunc(x);
}

double round(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    return xsign ? ftrunc(x - 0.5l) : ftrunc(x + 0.5l);
}

double remainder(double x, double y)
{
    bool xsign;
    const fpu_examine_t xexam = fxam(&xsign, x);
    if (xexam == FPU_EXAMINE_NAN)
    {
        return x;
    }
    if (xexam == FPU_EXAMINE_INFINITY)
    {
        return __builtin_nans() + fldz();
    }
    bool ysign;
    const fpu_examine_t yexam = fxam(&ysign, y);
    if (yexam == FPU_EXAMINE_NAN)
    {
        return y;
    }
    if (yexam == FPU_EXAMINE_ZERO)
    {
        return __builtin_nans() + fldz();
    }
    if (xexam == FPU_EXAMINE_ZERO)
    {
        return x;
    }
    if (yexam == FPU_EXAMINE_INFINITY)
    {
        return x;
    }
    bool bits[3];
    return fremainder(&bits, x, y);
}

double exp(double x)
{
    bool sign;
    const fpu_examine_t exam = fxam(&sign, x);
    if (exam == FPU_EXAMINE_NAN)
    {
        return x;
    }
    if (exam == FPU_EXAMINE_ZERO)
    {
        // e ^ 0 = 1
        return 1;
    }
    if (exam == FPU_EXAMINE_INFINITY)
    {
        if (sign)
        {
            // e ^ -inf = 0
            return fldz();
        }
        return x;
    }
    // e^x = 2^(x * log2(e))
    double xlog2e = x * fldl2e();
    double i = frndintany(xlog2e); // 整数部分
    double m = xlog2e - i;         // 小数部分
    return fscale(1, i) * (f2xm1(m) + 1);
}

double exp2(double x)
{
    bool sign;
    const fpu_examine_t exam = fxam(&sign, x);
    if (exam == FPU_EXAMINE_NAN)
    {
        return x;
    }
    if (exam == FPU_EXAMINE_ZERO)
    {
        // 2 ^ 0 = 1
        return 1;
    }
    if (exam == FPU_EXAMINE_INFINITY)
    {
        if (sign)
        {
            // 2 ^ -inf = 0
            return fldz();
        }
        return x;
    }
    const double i = frndintany(x);
    const double m = x - i;
    return fscale(1, i) * (f2xm1(m) + 1);
}

double expm1(double x)
{
    bool sign;
    const fpu_examine_t exam = fxam(&sign, x);
    if (exam == FPU_EXAMINE_NAN)
    {
        return x;
    }
    if (exam == FPU_EXAMINE_ZERO)
    {
        return x; // Note that x is zero.
    }
    if (exam == FPU_EXAMINE_INFINITY)
    {
        if (sign)
        {
            // (e ^ -inf) - 1 = -1
            return fchs(1);
        }
        return x;
    }
    // e^x = 2^(x* log2(e))
    const double xlog2e = x * fldl2e();
    const double i = ftrunc(xlog2e);
    const double m = xlog2e - i;
    if (i == 0)
    {
        return f2xm1(m);
    }
    return fscale(1, i) * (f2xm1(m) + 1) - 1;
}

double cbrt(double x)
{
    bool xsign;
    const fpu_examine_t xexam = fxam(&xsign, x);
    if (xexam == FPU_EXAMINE_NAN)
    {
        return x;
    }
    if (xexam == FPU_EXAMINE_ZERO)
    {
        return x; // Note that x is zero.
    }
    if (xexam == FPU_EXAMINE_INFINITY)
    {
        return x;
    }
    const double xabs = fabs(x);
    // x^(1/3) = 2^(log2(x)/3)
    const double three = 3;
    const double ylog2x = fyl2x(1 / three, xabs);
    const double i = frndintany(ylog2x); // 整数部分
    const double m = ylog2x - i;         // 小数部分
    double ret = fscale(1, i) * (f2xm1(m) + 1);
    if (xsign)
    {
        ret = fchs(ret);
    }
    return ret;
}

double pow(double x, double y)
{
    // IEC 60559
    // 1. pow(+0, y), where y is a negative odd integer, returns +∞ and raises FE_DIVBYZERO
    // 2. pow(-0, y), where y is a negative odd integer, returns -∞ and raises FE_DIVBYZERO
    // 3. pow(±0, y), where y is negative, finite, and is an even integer or a non-integer, returns +∞ and raises FE_DIVBYZERO
    // 4. pow(±0, -∞) returns +∞ and may raise FE_DIVBYZERO
    // 5. pow(+0, y), where y is a positive odd integer, returns +0
    // 6. pow(-0, y), where y is a positive odd integer, returns -0
    // 7. pow(±0, y), where y is positive non-integer or a positive even integer, returns +0
    // 8. pow(-1, ±∞) returns 1
    // 9. pow(+1, y) returns 1 for any y, even when y is NaN
    // 10. pow(x, ±0) returns 1 for any x, even when x is NaN
    // 11. pow(x, y) returns NaN and raises FE_INVALID if x is finite and negative and y is finite and non-integer.
    // 12. pow(x, -∞) returns +∞ for any |x|<1
    // 13. pow(x, -∞) returns +0 for any |x|>1
    // 14. pow(x, +∞) returns +0 for any |x|<1
    // 15. pow(x, +∞) returns +∞ for any |x|>1
    // 16. pow(-∞, y) returns -0 if y is a negative odd integer
    // 17. pow(-∞, y) returns +0 if y is a negative non-integer or negative even integer
    // 18. pow(-∞, y) returns -∞ if y is a positive odd integer
    // 19. pow(-∞, y) returns +∞ if y is a positive non-integer or positive even integer
    // 20. pow(+∞, y) returns +0 for any negative y
    // 21. pow(+∞, y) returns +∞ for any positive y
    // 22. except where specified above, if any argument is NaN, NaN is returned

    if (x == 1)
    {
        return 1; // Case 9.
    }
    bool ysign;
    const fpu_examine_t yexam = fxam(&ysign, y);
    if (yexam == FPU_EXAMINE_ZERO)
    {
        return 1; // Case 10.
    }
    bool xsign;
    const fpu_examine_t xexam = fxam(&xsign, x);
    if (xexam == FPU_EXAMINE_NAN)
    {
        return x; // Case 22.
    }
    if (yexam == FPU_EXAMINE_NAN)
    {
        return y; // Case 22.
    }
    if (xexam == FPU_EXAMINE_ZERO)
    {
        if (ysign)
        {
            if (yexam == FPU_EXAMINE_INFINITY)
            {
                return 1 / fldz(); // Case 4. Raises the exception.
            }
            bool bits[3];
            if (ffmod(&bits, y, 2) == -1)
            {
                double sign_unit = 1;
                if (xsign)
                {
                    sign_unit = fchs(sign_unit);
                }
                return sign_unit / fldz(); // Case 1. Case 2.
            }
            return 1 / fldz(); // Case 3.
        }
        bool bits[3];
        if (ffmod(&bits, y, 2) == 1)
        {
            return x; // Case 5. Case 6. Note that x is zero.
        }
        return fldz(); // Case 7.
    }
    if (xexam == FPU_EXAMINE_INFINITY)
    {
        if (xsign)
        {
            if (ysign)
            {
                bool bits[3];
                if (ffmod(&bits, y, 2) == -1)
                {
                    return fchs(fldz()); // Case 16.
                }
                return fldz(); // Case 17.
            }
            bool bits[3];
            if (ffmod(&bits, y, 2) == 1)
            {
                return x; // Case 18. Note that x is -∞.
            }
            return fchs(x); // Case 19. Note that x is -∞.
        }
        if (ysign)
        {
            return fldz(); // Case 20.
        }
        return x; // Case 21. Note that x is +∞.
    }
    const double xabs = fabs(x);
    if (yexam == FPU_EXAMINE_INFINITY)
    {
        if (xabs == 1)
        {
            return 1; // Case 8. Note that x cannot be 1.
        }
        if (ysign)
        {
            if (xabs < 1)
            {
                return fchs(y); // Case 12. Note that y is -∞.
            }
            return fldz(); // Case 13.
        }
        if (xabs < 1)
        {
            return fldz(); // Case 14.
        }
        return y; // Case 15. Note that y is +∞.
    }
    bool rsign = false;
    if (xsign)
    {
        if (frndintany(y) != y)
        {
            return __builtin_nans() + fldz(); // Case 11.
        }
        bool bits[3];
        rsign = (ffmod(&bits, y, 2) != 0);
    }
    // x^y = 2^(y*log2(x))
    const double ylog2x = fyl2x(y, xabs);
    const double i = frndintany(ylog2x), m = ylog2x - i;
    double ret = fscale(1, i) * (f2xm1(m) + 1);
    if (rsign)
    {
        ret = fchs(ret);
    }
    return ret;
}

double copysign(double x, double y)
{
    double ret = fabs(x);
    bool sign;
    fxam(&sign, y);
    if (sign)
    {
        ret = fchs(ret);
    }
    return ret;
}

double fdim(double x, double y)
{
    return (x > y) ? (x - y) : +0.0;
}

double ldexp(double x, int n)
{
    return fscale(x, n);
}

double floor(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    return ffloor(x);
}

double ceil(double x)
{
    FPU_EXAMINE_NAN_RETURN(x, x)
    return fceil(x);
}

double fmod(double x, double y)
{
    // IEC 60559
    // 1. If x is ±0 and y is not zero, ±0 is returned
    // 2. If x is ±∞ and y is not NaN, NaN is returned and FE_INVALID is raised
    // 3. If y is ±0 and x is not NaN, NaN is returned and FE_INVALID is raised
    // 4. If y is ±∞ and x is finite, x is returned
    // 5. If either argument is NaN, NaN is returned

    FPU_EXAMINE_NAN_RETURN(x, x) // Case 5.
    FPU_EXAMINE_NAN_RETURN(y, y) // Case 5.

    if (xexam == FPU_EXAMINE_INFINITY)
    {
        return __builtin_nans() + fldz(); // Case 2.
    }
    if (yexam == FPU_EXAMINE_ZERO)
    {
        return __builtin_nans() + fldz(); // Case 3.
    }
    if (xexam == FPU_EXAMINE_ZERO)
    {
        return x; // Case 1.
    }
    if (yexam == FPU_EXAMINE_INFINITY)
    {
        return x; // Case 4.
    }
    bool bits[3];
    return ffmod(&bits, x, y);
}

bool isnan(double x)
{
    bool xsign;
    const fpu_examine_t xexam = fxam(&xsign, x);
    return xexam == FPU_EXAMINE_NAN;
}

bool isfinite(double x)
{
    bool xsign;
    const fpu_examine_t xexam = fxam(&xsign, x);
    return xexam == FPU_EXAMINE_NORMAL || xexam == FPU_EXAMINE_ZERO;
}

bool isinf(double x)
{
    bool xsign;
    const fpu_examine_t xexam = fxam(&xsign, x);
    return xexam == FPU_EXAMINE_INFINITY;
}

bool isnormal(double x)
{
    bool xsign;
    const fpu_examine_t xexam = fxam(&xsign, x);
    return xexam == FPU_EXAMINE_NORMAL;
}