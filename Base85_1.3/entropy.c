#include "entropy.h"
#include <stdint.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define RESTRICT __restrict__
#define INLINE static inline __attribute__((always_inline))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define RESTRICT
#define INLINE static inline
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#define LOG2_E 1.4426950408889634

INLINE double chebyshev_ln_1to2(double x)
{
    const double t = x - 1.0;
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double t4 = t3 * t;
    const double t5 = t4 * t;
    const double t6 = t5 * t;

    return t
        - 0.50000000000 * t2
        + 0.33333333333 * t3
        - 0.25000000000 * t4
        + 0.20000000000 * t5
        - 0.16666666667 * t6;
}

INLINE double soft_log2(double x)
{
    if (x <= 0.0)
        return -1024.0;

    union {
        double d;
        uint64_t i;
    } u;
    u.d = x;

    uint64_t bits = u.i;
    int exp = (int)((bits >> 52U) & 0x7FFU) - 1023;

    u.i = (bits & UINT64_C(0x000FFFFFFFFFFFFF)) | UINT64_C(0x3FF0000000000000);
    double frac = u.d;

    double ln_frac = chebyshev_ln_1to2(frac);
    double log2_frac = ln_frac * LOG2_E;

    return (double)exp + log2_frac;
}

double calculate_entropy(const uint8_t *data, size_t len)
{
    if (len == 0)
        return 0.0;

    size_t count[256];
    memset(count, 0, sizeof(count));

    for (size_t i = 0; i < len; i++)
    {
        count[data[i]]++;
    }

    double H = 0.0;
    const double total = (double)len;

    for (int b = 0; b < 256; b++)
    {
        size_t cnt = count[b];
        if (cnt == 0)
            continue;

        double p = (double)cnt / total;
        H -= p * soft_log2(p);
    }
    return H;
}

double sample_entropy(const uint8_t *data, size_t len, size_t sample_limit)
{
    if (len == 0)
        return 0.0;
    size_t take = (len < sample_limit) ? len : sample_limit;
    return calculate_entropy(data, take);
}
