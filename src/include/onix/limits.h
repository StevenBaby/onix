
#ifndef ONIX_LIMITS_H
#define ONIX_LIMITS_H

#include <onix/onix.h>

#define INT_MAX (2 ^ 31 - 1)
#define INT_MIN (-2 ^ 31)

// 下面都是表示不同类型的 NAN
#define NAN (0.0f / 0.0f)

static const unsigned int _QNAN_F = 0x7fc00000;
#define NANF (*((float *)(&_QNAN_F)))

static const unsigned int _SNAN_F = 0x7f855555;
#define NANSF (*((float *)(&_SNAN_F)))

static const unsigned int _SNAN_D[2] = {0x7ff55555, 0x55555555};
#define NANS (*((double *)(&_SNAN_D)))

// ISO C99 function nan.
#define __builtin_nan(__dummy) NAN
#define __builtin_nanf(__dummy) NANF
#define __builtin_nans(__dummy) NANS
#define __builtin_nansf(__dummy) NANSF

#define INF (1.0f / 0.0f)

#define __builtin_infinity(__dummy) INF

#endif