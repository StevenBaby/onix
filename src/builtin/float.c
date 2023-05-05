#include <onix/stdio.h>
#include <onix/syscall.h>
#include <onix/math.h>

int main(int argc, char const *argv[])
{
    printf("sin(3.0) = %f\n", sin(3.0));     // sin(3.0) = 0.141120
    printf("cos(3.0) = %f\n", cos(3.0));     // cos(3.0) = -0.989992
    printf("tan(3.0) = %f\n", tan(3.0));     // tan(3.0) = -.0142546
    printf("asin(1.0) = %f\n", asin(1.0));   // asin(1.0) = 1.570796
    printf("acos(-1.0) = %f\n", acos(-1.0)); // acos(-1.0) = 3.141592 = π
    printf("atan(3.0) = %f\n", atan(3.0));   // atan(3.0) = 1.249045

    printf("sqrt(3.0) = %f\n", sqrt(3.0)); // sqrt(3.0) = 1.732050

    printf("fabs(-3.0) = %f\n", fabs(-3.0)); // fabs(-3.0) = 3.0

    printf("fmax(3.0, -3.0) = %f\n", fmax(3.0, -3.0)); // fmax(3.0, -3.0) = 3.0
    printf("fmin(3.0, -3.0) = %f\n", fmin(3.0, -3.0)); // fmin(3.0, -3.0) = -3.0

    double x = 1024.0;
    int e;
    double fra = frexp(x, &e);
    printf("x = %f = %f * 2^%d\n", x, fra, e); // x = 1024.00 = 0.50 * 2^11

    printf("log(2.700000) = %f\n", log(2.700000)); // log(2.700000) = 0.993252

    printf("log2(%f) = %f\n", x, log2(x));

    printf("log10(10000.000000) = %f\n", log10(10000.000000)); // log10(10000.000000) = 4.0

    printf("trunc(-2.8) = %f\n", trunc(-2.8));         // trunc(-2.8) = -2.0
    printf("trunc(-3.1) = %f\n", trunc(-3.1));         // trunc(-3.1) = -3.0
    printf("trunc(4.999999) = %f\n", trunc(4.999999)); // trunc(4.999999) = 4.0

    printf("round(+2.3) = %f\n", round(2.3));  // round(+2.3) = +2.0
    printf("round(-2.3) = %f\n", round(-2.3)); // round(-2.3) = -2.0
    printf("round(+2.7) = %f\n", round(2.7));  // round(+2.7) = +3.0
    printf("round(-2.7) = %f\n", round(-2.7)); // round(-2.7) = -3.0
    printf("round(-0.0) = %f\n", round(-0.0)); // round(-0.0) = -0.0

    printf("remainder(7.5, 2.1) = %f\n", remainder(7.5, 2.1));   // remainder(7.5, 2.1) = -0.900000
    printf("remainder(17.5, 2.0) = %f\n", remainder(17.5, 2.0)); // remainder(17.5, 2.0) = 0.500000
    printf("remainder(17.5, 0.0) = %f\n", remainder(17.5, 0.0)); // remainder(17.5, 0.0) = 0.0

    printf("exp(1.0) = %f\n", exp(1.0));     // exp(1.0) = 2.718281
    printf("exp(1.0) = %f\n", exp(1.0));     // exp(1.0) = 2.718281
    printf("exp(2.0) = %f\n", exp(2.0));     // exp(2.0) = 7.389056
    printf("exp2(8.0) = %f\n", exp2(8.0));   // exp2(8.0) = 256.0
    printf("expm1(1.0) = %f\n", expm1(1.0)); // expm1(1.0) = 1.718281

    printf("crbt(-1000.0) = %f\n", cbrt(-1000.0)); // crbt(-1000.0) = -10.0

    printf("pow(8.0, 3.0) = %f\n", pow(8.0, 3));       // pow(8.0, 3.0) = 512.0
    printf("pow(3.05, 1.98) = %f\n", pow(3.05, 1.98)); // pow(3.05, 1.98) = 9.97323

    printf("pow(8.0, 3.0) = %f\n", pow(8.0, 3));
    printf("pow(3.05, 1.98) = %f\n", pow(3.05, 1.98));

    printf("copysign(34.15,-13) = %f\n", copysign(34.15, -13)); // copysign(34.15,-13) = -34.149999

    printf("fdim(8.0, 2.0) = %f\n", fdim(8.0, 2.0)); // fdim(8.0, 2.0) = 6.00
    printf("fdim(2.0, 8.0) = %f\n", fdim(2.0, 8.0)); // fdim(2.0, 8.0) = 0.00

    printf("0.65 * 2^3 = %f\n", ldexp(0.65, 3)); // 0.65 * 2^3 = 5.200000

    printf("ceil(1.6) = %f\n", ceil(1.6)); // ceil(1.6) = 2.0
    printf("ceil(1.2) = %f\n", ceil(1.2)); // ceil(1.2) = 2.0
    printf("ceil(2.8) = %f\n", ceil(2.8)); // ceil(2.8) = 3.0
    printf("ceil(2.3) = %f\n", ceil(2.3)); // ceil(2.3) = 3.0

    printf("floor(1.6) = %f\n", floor(1.6)); // floor(1.6) = 1.0
    printf("floor(1.2) = %f\n", floor(1.2)); // floor(1.2) = 1.0
    printf("floor(2.8) = %f\n", floor(2.8)); // floor(2.8) = 2.0
    printf("floor(2.3) = %f\n", floor(2.3)); // floor(2.3) = 2.0

    printf("fmod(9.2, 2) = %f\n", fmod(9.2, 2));     // fmod(9.2, 2) = 1.199999
    printf("fmod(9.2, 3.7) = %f\n", fmod(9.2, 3.7)); // fmod(9.2, 3.7) = 1.799999

    printf("nan = %f\n", __builtin_nan());        // nan = nan
    printf("nansf = %f\n", __builtin_nansf());    // nansf = nan
    printf("nans = %f\n", __builtin_nans());      // TODO: fix me
    printf("inf = %f\n", __builtin_infinity());   // inf = inf
    printf("-inf = %f\n", -__builtin_infinity()); // -inf = -inf

    printf("isfinite(nan) = %d\n", isfinite(__builtin_nan()));        // isfinite(nan) = 0
    printf("isfinite(inf) = %d\n", isfinite(__builtin_infinity()));   // isfinite(inf) = 0
    printf("isfinite(-inf) = %d\n", isfinite(-__builtin_infinity())); // isfinite(-inf) = 0
    printf("isfinite(0) = %d\n", isfinite(0));                        // isfinite(0) = 1

    printf("isinf(nan) = %d\n", isinf(__builtin_nan()));        // isinf(nan) = 0
    printf("isinf(inf) = %d\n", isinf(__builtin_infinity()));   // isinf(inf) = 1
    printf("isinf(-inf) = %d\n", isinf(-__builtin_infinity())); // isinf(-inf) = 1
    printf("isinf(0) = %d\n", isinf(0));                        // isinf(0) = 0

    return 0;
}
