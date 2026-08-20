#include "compress.h"
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

#define LZ77_HASH_TABLE_SIZE 65536U
#define LZ77_WINDOW_SIZE (64U * 1024U)
#define LZ77_MIN_MATCH 4U
#define LZ77_MAX_MATCH 131U
#define LZ77_MAX_LITERAL 127U

static uint16_t g_hash_table[LZ77_HASH_TABLE_SIZE];

INLINE uint32_t lz77_hash(const uint8_t *p)
{
    uint32_t h = ((uint32_t)p[0] << 16) ^ ((uint32_t)p[1] << 8) ^ (uint32_t)p[2];
    return h & 0xFFFFU;
}

static int emit_literals(const uint8_t *in, size_t *literal_start_ptr, size_t ip,
                          uint8_t *out, size_t *op_ptr, size_t out_cap)
{
    size_t literal_start = *literal_start_ptr;
    size_t op = *op_ptr;
    size_t lit_len = ip - literal_start;

    while (lit_len > 0)
    {
        size_t chunk = (lit_len > LZ77_MAX_LITERAL) ? LZ77_MAX_LITERAL : lit_len;
        if (op + 1U + chunk > out_cap)
            return -1;
        out[op++] = (uint8_t)chunk;
        memcpy(&out[op], &in[literal_start], chunk);
        op += chunk;
        literal_start += chunk;
        lit_len -= chunk;
    }

    *literal_start_ptr = literal_start;
    *op_ptr = op;
    return 0;
}

size_t lz77_compress(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_cap)
{
    if (in_len == 0 || out_cap == 0 || !in || !out)
        return 0;

    size_t ip = 0;
    size_t op = 0;

    while (ip < in_len)
    {
        size_t block_start = ip;
        size_t block_end = ip + LZ77_WINDOW_SIZE;
        if (block_end > in_len)
            block_end = in_len;

        memset(g_hash_table, 0, sizeof(g_hash_table));

        size_t literal_start = ip;

        while (ip < block_end)
        {
            if (ip + LZ77_MIN_MATCH > block_end)
            {
                ip = block_end;
                break;
            }

            uint32_t h = lz77_hash(&in[ip]);
            uint16_t stored = g_hash_table[h];
            g_hash_table[h] = (uint16_t)((ip - block_start) + 1U);

            if (stored == 0)
            {
                ip++;
                continue;
            }

            size_t match_off_in_block = (size_t)(stored - 1U);
            size_t match_pos = block_start + match_off_in_block;

            if (match_pos >= ip)
            {
                ip++;
                continue;
            }

            size_t distance = ip - match_pos;
            if (distance == 0 || distance > 65535U)
            {
                ip++;
                continue;
            }

            size_t match_len = 0;
            size_t max_possible = block_end - ip;
            if (max_possible > LZ77_MAX_MATCH)
                max_possible = LZ77_MAX_MATCH;

            while (match_len < max_possible && in[ip + match_len] == in[match_pos + match_len])
                match_len++;

            if (match_len >= LZ77_MIN_MATCH)
            {
                if (emit_literals(in, &literal_start, ip, out, &op, out_cap) < 0)
                    return 0;

                if (op + 3U > out_cap)
                    return 0;

                out[op++] = (uint8_t)(0x80U | (uint8_t)(match_len - 4U));
                out[op++] = (uint8_t)((distance >> 8) & 0xFFU);
                out[op++] = (uint8_t)(distance & 0xFFU);

                ip += match_len;
                literal_start = ip;
            }
            else
            {
                ip++;
            }
        }

        if (emit_literals(in, &literal_start, ip, out, &op, out_cap) < 0)
            return 0;
    }

    return op;
}

size_t lz77_decompress(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_cap)
{
    if (in_len == 0 || out_cap == 0 || !in || !out)
        return 0;

    size_t ip = 0;
    size_t op = 0;

    while (ip < in_len)
    {
        uint8_t cmd = in[ip++];

        if ((cmd & 0x80U) == 0)
        {
            size_t lit_len = (size_t)cmd;
            if (ip + lit_len > in_len)
                return 0;
            if (op + lit_len > out_cap)
                return 0;
            memcpy(&out[op], &in[ip], lit_len);
            ip += lit_len;
            op += lit_len;
        }
        else
        {
            size_t match_len = (size_t)(cmd & 0x7FU) + 4U;
            if (ip + 2U > in_len)
                return 0;

            uint16_t distance = ((uint16_t)in[ip] << 8) | (uint16_t)in[ip + 1];
            ip += 2U;

            if (distance == 0)
                return 0;
            if ((size_t)distance > op)
                return 0;
            if (op + match_len > out_cap)
                return 0;

            size_t src = op - (size_t)distance;
            for (size_t i = 0; i < match_len; i++)
            {
                out[op + i] = out[src + i];
            }
            op += match_len;
        }
    }

    return op;
}
