#include "base85.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

static const uint8_t B85_CHARS[85] = {
'!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
'0','1','2','3','4','5','6','7','8','9',
':',';','<','=','>','?','@',
'A','B','C','D','E','F','G','H','I','J','K','L','M',
'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
'[','\\',']','^','_','`',
'a','b','c','d','e','f','g','h','i','j','k','l','m',
'n','o','p','q','r','s','t','u'
};

INLINE void encode_block(const uint8_t *RESTRICT in, uint8_t *RESTRICT out) {
    uint32_t n = ((uint32_t)(unsigned char)in[0] << 24) |
                 ((uint32_t)(unsigned char)in[1] << 16) |
                 ((uint32_t)(unsigned char)in[2] << 8) |
                 (uint32_t)(unsigned char)in[3];
    out[0] = B85_CHARS[n / 52200625u];
    n %= 52200625u;
    out[1] = B85_CHARS[n / 614125u];
    n %= 614125u;
    out[2] = B85_CHARS[n / 7225u];
    n %= 7225u;
    out[3] = B85_CHARS[n / 85u];
    out[4] = B85_CHARS[n % 85u];
    DEBUG_PRINT(2, "encode_block: in=%02x %02x %02x %02x -> out=%c%c%c%c%c",
                in[0], in[1], in[2], in[3],
                out[0], out[1], out[2], out[3], out[4]);
}

INLINE void encode_8_blocks(const uint8_t *RESTRICT in, uint8_t *RESTRICT out) {
    encode_block(in + 0, out + 0);
    encode_block(in + 4, out + 5);
    encode_block(in + 8, out + 10);
    encode_block(in + 12, out + 15);
    encode_block(in + 16, out + 20);
    encode_block(in + 20, out + 25);
    encode_block(in + 24, out + 30);
    encode_block(in + 28, out + 35);
}

INLINE int decode_block(const uint8_t *RESTRICT in, uint8_t *RESTRICT out) {
    uint64_t n = 0;
    for (int i = 0; i < 5; i++) {
        unsigned char c = in[i];
        if (UNLIKELY(c < '!' || c > 'u')) {
            return -1;
        }
        uint8_t v = (uint8_t)(c - '!');
        n = n * 85u + v;
    }
    out[0] = (uint8_t)(n >> 24);
    out[1] = (uint8_t)(n >> 16);
    out[2] = (uint8_t)(n >> 8);
    out[3] = (uint8_t)n;
    DEBUG_PRINT(2, "decode_block: in=%c%c%c%c%c -> n=%llu -> out=%02x%02x%02x%02x",
                in[0], in[1], in[2], in[3], in[4],
                (unsigned long long)n,
                out[0], out[1], out[2], out[3]);
    return 0;
}

INLINE int decode_8_blocks(const uint8_t *RESTRICT in, uint8_t *RESTRICT out) {
    if (decode_block(in + 0, out + 0) < 0) return -1;
    if (decode_block(in + 5, out + 4) < 0) return -1;
    if (decode_block(in + 10, out + 8) < 0) return -1;
    if (decode_block(in + 15, out + 12) < 0) return -1;
    if (decode_block(in + 20, out + 16) < 0) return -1;
    if (decode_block(in + 25, out + 20) < 0) return -1;
    if (decode_block(in + 30, out + 24) < 0) return -1;
    if (decode_block(in + 35, out + 28) < 0) return -1;
    return 0;
}

void base85_enc_init(base85_enc_ctx_t *ctx) {
    if (!ctx) return;
    ctx->len = 0;
    DEBUG_PRINT(1, "enc_init");
}

void base85_enc_reset(base85_enc_ctx_t *ctx) {
    if (!ctx) return;
    ctx->len = 0;
    DEBUG_PRINT(1, "enc_reset");
}

size_t base85_enc_update(base85_enc_ctx_t *ctx, const uint8_t *RESTRICT in,
                         size_t in_len, uint8_t *RESTRICT out) {
    if (!ctx || !in || !out) return 0;
    if (in_len == 0) return 0;
    size_t out_used = 0;
    size_t i = 0;
    DEBUG_PRINT(1, "enc_update: in_len=%zu, ctx->len=%d", in_len, ctx->len);
    if (ctx->len > 0) {
        while (ctx->len < 4 && i < in_len) {
            ctx->buf[ctx->len++] = in[i++];
        }
        if (ctx->len == 4) {
            encode_block(ctx->buf, out);
            out_used += 5;
            ctx->len = 0;
            DEBUG_PRINT(1, "  flushed buffer, out_used=%zu", out_used);
        }
    }
    size_t remaining = in_len - i;
    while (LIKELY(remaining >= 32)) {
        encode_8_blocks(in + i, out + out_used);
        out_used += 40;
        i += 32;
        remaining -= 32;
    }
    while (remaining >= 4) {
        encode_block(in + i, out + out_used);
        out_used += 5;
        i += 4;
        remaining -= 4;
    }
    while (i < in_len) {
        ctx->buf[ctx->len++] = in[i++];
    }
    DEBUG_PRINT(1, "enc_update: out_used=%zu, ctx->len=%d", out_used, ctx->len);
    return out_used;
}

size_t base85_enc_final(base85_enc_ctx_t *ctx, uint8_t *RESTRICT out) {
    if (!ctx || !out) return 0;
    if (ctx->len == 0) return 0;
    DEBUG_PRINT(1, "enc_final: ctx->len=%d", ctx->len);
    uint8_t pad[4] = {0};
    memcpy(pad, ctx->buf, ctx->len);
    uint8_t tmp[5];
    encode_block(pad, tmp);
    size_t out_len = ctx->len + 1;
    memcpy(out, tmp, out_len);
    ctx->len = 0;
    return out_len;
}

void base85_dec_init(base85_dec_ctx_t *ctx) {
    if (!ctx) return;
    ctx->len = 0;
    DEBUG_PRINT(1, "dec_init");
}

void base85_dec_reset(base85_dec_ctx_t *ctx) {
    if (!ctx) return;
    ctx->len = 0;
    DEBUG_PRINT(1, "dec_reset");
}

size_t base85_dec_update(base85_dec_ctx_t *ctx, const uint8_t *RESTRICT in,
                         size_t in_len, uint8_t *RESTRICT out) {
    if (!ctx || !in || !out) return 0;
    if (in_len == 0) return 0;
    size_t out_used = 0;
    size_t i = 0;
    DEBUG_PRINT(1, "dec_update: in_len=%zu, ctx->len=%d", in_len, ctx->len);
    if (ctx->len > 0) {
        while (ctx->len < 5 && i < in_len) {
            ctx->buf[ctx->len++] = in[i++];
        }
        if (ctx->len == 5) {
            if (UNLIKELY(decode_block(ctx->buf, out) < 0)) {
                ctx->len = 0;
                return (size_t)-1;
            }
            out_used += 4;
            ctx->len = 0;
            DEBUG_PRINT(1, "  flushed buffer, out_used=%zu", out_used);
        }
    }
    size_t remaining = in_len - i;
    while (LIKELY(remaining >= 40)) {
        if (UNLIKELY(decode_8_blocks(in + i, out + out_used) < 0)) {
            ctx->len = 0;
            return (size_t)-1;
        }
        out_used += 32;
        i += 40;
        remaining -= 40;
    }
    while (remaining >= 5) {
        if (UNLIKELY(decode_block(in + i, out + out_used) < 0)) {
            ctx->len = 0;
            return (size_t)-1;
        }
        out_used += 4;
        i += 5;
        remaining -= 5;
    }
    while (i < in_len) {
        ctx->buf[ctx->len++] = in[i++];
    }
    DEBUG_PRINT(1, "dec_update: out_used=%zu, ctx->len=%d", out_used, ctx->len);
    return out_used;
}

size_t base85_dec_final(base85_dec_ctx_t *ctx, uint8_t *RESTRICT out) {
    if (!ctx || !out) return 0;
    if (ctx->len == 0) return 0;
    if (UNLIKELY(ctx->len < 2)) {
        ctx->len = 0;
        return (size_t)-1;
    }
    uint8_t pad[5] = {0};
    memcpy(pad, ctx->buf, ctx->len);
    for (int i = ctx->len; i < 5; i++) pad[i] = 'u';
    uint8_t tmp[4];
    if (UNLIKELY(decode_block(pad, tmp) < 0)) {
        ctx->len = 0;
        return (size_t)-1;
    }
    size_t out_len = ctx->len - 1;
    memcpy(out, tmp, out_len);
    ctx->len = 0;
    return out_len;
}

size_t base85_encode(const uint8_t *in, size_t in_len, char *out) {
    if (!in || !out) return 0;
    base85_enc_ctx_t ctx;
    base85_enc_init(&ctx);
    size_t out_len = 0;
    out_len += base85_enc_update(&ctx, in, in_len, (uint8_t*)out);
    out_len += base85_enc_final(&ctx, (uint8_t*)(out + out_len));
    out[out_len] = '\0';
    return out_len;
}

size_t base85_decode(const char *in_str, size_t in_len, uint8_t *out, int flags) {
    if (!in_str || !out) return 0;
    if (in_len == 0) in_len = strlen(in_str);
    if (in_len == 0) return 0;
    base85_dec_ctx_t ctx;
    base85_dec_init(&ctx);
    size_t out_used = 0;
    int ignore_ws = (flags & BASE85_DECODE_IGNORE_WHITESPACE) != 0;
    for (size_t i = 0; i < in_len; i++) {
        unsigned char c = (unsigned char)in_str[i];
        if (ignore_ws && (c == ' ' || c == '\t' || c == '\n' ||
                          c == '\r' || c == '\f' || c == '\v'))
            continue;
        if (UNLIKELY(c < '!' || c > 'u'))
            return (size_t)-1;
        ctx.buf[ctx.len++] = c;
        if (ctx.len == 5) {
            if (UNLIKELY(decode_block(ctx.buf, out + out_used) < 0)) {
                ctx.len = 0;
                return (size_t)-1;
            }
            out_used += 4;
            ctx.len = 0;
        }
    }
    if (ctx.len > 0) {
        if (UNLIKELY(ctx.len < 2))
            return (size_t)-1;
        uint8_t pad[5];
        memcpy(pad, ctx.buf, ctx.len);
        for (int i = ctx.len; i < 5; i++) pad[i] = 'u';
        uint8_t tmp[4];
        if (UNLIKELY(decode_block(pad, tmp) < 0))
            return (size_t)-1;
        size_t bytes = ctx.len - 1;
        memcpy(out + out_used, tmp, bytes);
        out_used += bytes;
    }
    return out_used;
}

int base85_is_valid(const char *in, size_t in_len, int flags) {
    if (!in) return 0;
    if (in_len == 0) in_len = strlen(in);
    if (in_len == 0) return 1;
    int ignore_ws = (flags & BASE85_DECODE_IGNORE_WHITESPACE) != 0;
    size_t valid_len = 0;
    for (size_t i = 0; i < in_len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (ignore_ws && (c == ' ' || c == '\t' || c == '\n' ||
                          c == '\r' || c == '\f' || c == '\v'))
            continue;
        if (c < '!' || c > 'u') return 0;
        valid_len++;
    }
    if (valid_len == 0) return 1;
    if (valid_len % 5 == 1) return 0;
    return 1;
}

static int debug_level = 0;
int base85_debug_level(void) {
    return debug_level;
}
void base85_set_debug_level(int level) {
    debug_level = level;
}

#ifdef BASE85_DEBUG
void base85_enc_dump(const base85_enc_ctx_t *ctx) {
    if (!ctx) return;
    fprintf(stderr, "[DUMP] enc: len=%d buf=", ctx->len);
    for (int i=0; i<ctx->len; i++) fprintf(stderr, "%02x ", ctx->buf[i]);
    fprintf(stderr, "\n");
}
void base85_dec_dump(const base85_dec_ctx_t *ctx) {
    if (!ctx) return;
    fprintf(stderr, "[DUMP] dec: len=%d buf=", ctx->len);
    for (int i=0; i<ctx->len; i++) fprintf(stderr, "%02x ", ctx->buf[i]);
    fprintf(stderr, "\n");
}
#endif
