#include "arm_optimize.h"
#include <stdint.h>
#include <string.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define HAVE_NEON 1
#else
#define HAVE_NEON 0
#endif

int base85_arm_supported(void)
{
#if HAVE_NEON
    return 1;
#else
    return 0;
#endif
}

#if HAVE_NEON

static const uint8_t B85_CHARS_ARM[85] = {
    '!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
    '0','1','2','3','4','5','6','7','8','9',
    ':',';','<','=','>','?','@',
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '[','\\',']','^','_','`',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u'
};

static inline uint32_t pack4(const uint8_t *in)
{
    uint32_t v;
    memcpy(&v, in, sizeof(v));
    return v;
}

static inline void arm_prefetch(const void *p)
{
    __asm__ volatile (
        "prfm pldl1keep, [%0]"
        :
        : "r"(p)
        : "memory"
    );
}

static inline void encode_asm(uint32_t n, uint8_t *out)
{
    uint32_t t0, t1, t2, t3, t4, t5;
    __asm__ volatile (
        "movz %w3, #0xA711\n"
        "movk %w3, #0x310, lsl #16\n"
        "udiv %w4, %w0, %w3\n"
        "msub %w0, %w4, %w3, %w0\n"

        "movz %w3, #0x5F6D\n"
        "movk %w3, #0x9, lsl #16\n"
        "udiv %w5, %w0, %w3\n"
        "msub %w0, %w5, %w3, %w0\n"

        "movz %w3, #0x1C39\n"
        "udiv %w6, %w0, %w3\n"
        "msub %w0, %w6, %w3, %w0\n"

        "mov %w3, #85\n"
        "udiv %w1, %w0, %w3\n"
        "msub %w2, %w0, %w3, %w0\n"

        "add %w4, %w4, #33\n"
        "add %w5, %w5, #33\n"
        "add %w6, %w6, #33\n"
        "add %w1, %w1, #33\n"
        "add %w2, %w2, #33\n"

        "strb %w4, [%7]\n"
        "strb %w5, [%7, #1]\n"
        "strb %w6, [%7, #2]\n"
        "strb %w1, [%7, #3]\n"
        "strb %w2, [%7, #4]\n"

        : "+r"(n), "=&r"(t0), "=&r"(t1), "=&r"(t2), "=&r"(t3), "=&r"(t4), "=&r"(t5)
        : "r"(out)
        : "memory"
    );
}

static inline int decode_asm(const uint8_t *in, uint32_t *n_out)
{
    int res;
    uint32_t t0,t1,t2,t3,t4,t5,t6,t7;
    __asm__ volatile (
        "ldrb %w2, [%10]\n"
        "ldrb %w3, [%10, #1]\n"
        "ldrb %w4, [%10, #2]\n"
        "ldrb %w5, [%10, #3]\n"
        "ldrb %w6, [%10, #4]\n"

        "sub %w7, %w2, #33\n"
        "cmp %w7, #84\n"
        "cset %w8, hi\n"

        "sub %w7, %w3, #33\n"
        "cmp %w7, #84\n"
        "cset %w9, hi\n"
        "orr %w8, %w8, %w9\n"

        "sub %w7, %w4, #33\n"
        "cmp %w7, #84\n"
        "cset %w9, hi\n"
        "orr %w8, %w8, %w9\n"

        "sub %w7, %w5, #33\n"
        "cmp %w7, #84\n"
        "cset %w9, hi\n"
        "orr %w8, %w8, %w9\n"

        "sub %w7, %w6, #33\n"
        "cmp %w7, #84\n"
        "cset %w9, hi\n"
        "orr %w8, %w8, %w9\n"

        "cbnz %w8, 1f\n"

        "sub %w2, %w2, #33\n"
        "sub %w3, %w3, #33\n"
        "sub %w4, %w4, #33\n"
        "sub %w5, %w5, #33\n"
        "sub %w6, %w6, #33\n"

        "mov %w0, %w2\n"
        "mov %w1, #0\n"

        "mov %w7, #85\n"
        "mul %w0, %w0, %w7\n"
        "add %w0, %w0, %w3\n"
        "mul %w0, %w0, %w7\n"
        "add %w0, %w0, %w4\n"
        "mul %w0, %w0, %w7\n"
        "add %w0, %w0, %w5\n"
        "mul %w0, %w0, %w7\n"
        "add %w0, %w0, %w6\n"

        "b 2f\n"
    "1:\n"
        "mov %w1, #-1\n"
    "2:\n"

        : "=r"(*n_out), "=r"(res),
          "=&r"(t0),"=&r"(t1),"=&r"(t2),"=&r"(t3),
          "=&r"(t4),"=&r"(t5),"=&r"(t6),"=&r"(t7)
        : "r"(in)
        : "memory"
    );
    return res;
}

size_t base85_encode_block_arm(const uint8_t *in, uint8_t *out)
{
    arm_prefetch(in);
    uint32_t n = pack4(in);
    encode_asm(n, out);
    return 5;
}

size_t base85_encode_8_blocks_arm(const uint8_t *in, uint8_t *out)
{
    arm_prefetch(in + 32);
    __asm__ volatile (
        "prfm pldl1keep, [%0, #16]\n"
        :
        : "r"(in)
        : "memory"
    );
    for (int i = 0; i < 8; i++)
    {
        uint32_t n;
        memcpy(&n, in + i*4, sizeof(n));
        encode_asm(n, out + i * 5);
    }
    return 40;
}

int base85_decode_block_arm(const uint8_t *in, uint8_t *out)
{
    arm_prefetch(in);
    uint32_t n;
    int ret = decode_asm(in, &n);
    if (ret == 0)
    {
        out[0] = (uint8_t)((n >> 24) & 0xFFU);
        out[1] = (uint8_t)((n >> 16) & 0xFFU);
        out[2] = (uint8_t)((n >> 8) & 0xFFU);
        out[3] = (uint8_t)(n & 0xFFU);
    }
    return ret;
}

int base85_decode_8_blocks_arm(const uint8_t *in, uint8_t *out)
{
    arm_prefetch(in + 40);
    for (int i = 0; i < 8; i++)
    {
        if (base85_decode_block_arm(in + i * 5, out + i * 4) < 0)
        {
            return -1;
        }
    }
    return 0;
}

#else
size_t base85_encode_block_arm(const uint8_t *in, uint8_t *out) { (void)in; (void)out; return 0; }
size_t base85_encode_8_blocks_arm(const uint8_t *in, uint8_t *out) { (void)in; (void)out; return 0; }
int base85_decode_block_arm(const uint8_t *in, uint8_t *out) { (void)in; (void)out; return -1; }
int base85_decode_8_blocks_arm(const uint8_t *in, uint8_t *out) { (void)in; (void)out; return -1; }
#endif
