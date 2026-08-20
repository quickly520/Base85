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

static const uint8_t B85_ASCII85[85] = {
    '!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
    '0','1','2','3','4','5','6','7','8','9',
    ':',';','<','=','>','?','@',
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '[','\\',']','^','_','`',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u'
};

static const uint8_t B85_Z85[85] = {
    '0','1','2','3','4','5','6','7','8','9',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '.','-',':','+','=','^','!','/','*','?','&','<','>','(',')',
    '[',']','{','}','@','%','$','#'
};

static const uint8_t B85_RFC1924[85] = {
    '0','1','2','3','4','5','6','7','8','9',
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '!','#','$','%','&','\'','(',')','*','+','-','.','/',
    ':',';','<','=','>','?','@','[',']','^','_','`','{','|','}','~','_'
};

static const uint8_t *get_charset(base85_variant_t variant) {
    switch (variant) {
        case BASE85_VARIANT_Z85: return B85_Z85;
        case BASE85_VARIANT_RFC1924: return B85_RFC1924;
        default: return B85_ASCII85;
    }
}

static uint8_t get_pad_char(base85_variant_t variant) {
    const uint8_t *cs = get_charset(variant);
    return cs[84];
}

static int is_valid_char(base85_variant_t variant, unsigned char c) {
    const uint8_t *charset = get_charset(variant);
    for (int i = 0; i < 85; i++) {
        if (charset[i] == c) return 1;
    }
    return 0;
}

static int char_to_value(base85_variant_t variant, unsigned char c) {
    const uint8_t *charset = get_charset(variant);
    for (int i = 0; i < 85; i++) {
        if (charset[i] == c) return i;
    }
    return -1;
}

INLINE void encode_block_ex(const uint8_t *RESTRICT in, uint8_t *RESTRICT out,
                            base85_variant_t variant) {
    uint32_t n = ((uint32_t)(unsigned char)in[0] << 24) |
                 ((uint32_t)(unsigned char)in[1] << 16) |
                 ((uint32_t)(unsigned char)in[2] << 8) |
                 (uint32_t)(unsigned char)in[3];
    const uint8_t *charset = get_charset(variant);
    out[0] = charset[n / 52200625u];
    n %= 52200625u;
    out[1] = charset[n / 614125u];
    n %= 614125u;
    out[2] = charset[n / 7225u];
    n %= 7225u;
    out[3] = charset[n / 85u];
    out[4] = charset[n % 85u];
}

INLINE int decode_block_ex(const uint8_t *RESTRICT in, uint8_t *RESTRICT out,
                           base85_variant_t variant) {
    uint64_t n = 0;
    for (int i = 0; i < 5; i++) {
        int v = char_to_value(variant, in[i]);
        if (UNLIKELY(v < 0)) return -1;
        n = n * 85u + (uint8_t)v;
    }
    out[0] = (uint8_t)(n >> 24);
    out[1] = (uint8_t)(n >> 16);
    out[2] = (uint8_t)(n >> 8);
    out[3] = (uint8_t)n;
    return 0;
}

INLINE void encode_8_blocks_ex(const uint8_t *RESTRICT in, uint8_t *RESTRICT out,
                               base85_variant_t variant) {
    encode_block_ex(in + 0, out + 0, variant);
    encode_block_ex(in + 4, out + 5, variant);
    encode_block_ex(in + 8, out + 10, variant);
    encode_block_ex(in + 12, out + 15, variant);
    encode_block_ex(in + 16, out + 20, variant);
    encode_block_ex(in + 20, out + 25, variant);
    encode_block_ex(in + 24, out + 30, variant);
    encode_block_ex(in + 28, out + 35, variant);
}

INLINE int decode_8_blocks_ex(const uint8_t *RESTRICT in, uint8_t *RESTRICT out,
                              base85_variant_t variant) {
    if (decode_block_ex(in + 0, out + 0, variant) < 0) return -1;
    if (decode_block_ex(in + 5, out + 4, variant) < 0) return -1;
    if (decode_block_ex(in + 10, out + 8, variant) < 0) return -1;
    if (decode_block_ex(in + 15, out + 12, variant) < 0) return -1;
    if (decode_block_ex(in + 20, out + 16, variant) < 0) return -1;
    if (decode_block_ex(in + 25, out + 20, variant) < 0) return -1;
    if (decode_block_ex(in + 30, out + 24, variant) < 0) return -1;
    if (decode_block_ex(in + 35, out + 28, variant) < 0) return -1;
    return 0;
}

typedef struct {
    base85_enc_ctx_t base;
    base85_variant_t variant;
} base85_enc_ex_ctx_t;

typedef struct {
    base85_dec_ctx_t base;
    base85_variant_t variant;
} base85_dec_ex_ctx_t;

void base85_enc_ex_init(base85_enc_ex_ctx_t *ctx, base85_variant_t variant) {
    if (!ctx) return;
    ctx->base.len = 0;
    ctx->variant = variant;
}

void base85_dec_ex_init(base85_dec_ex_ctx_t *ctx, base85_variant_t variant) {
    if (!ctx) return;
    ctx->base.len = 0;
    ctx->variant = variant;
}

size_t base85_enc_ex_update(base85_enc_ex_ctx_t *ctx, const uint8_t *RESTRICT in,
                            size_t in_len, uint8_t *RESTRICT out) {
    if (!ctx || !in || !out) return 0;
    if (in_len == 0) return 0;
    size_t out_used = 0;
    size_t i = 0;
    if (ctx->base.len > 0) {
        while (ctx->base.len < 4 && i < in_len) {
            ctx->base.buf[ctx->base.len++] = in[i++];
        }
        if (ctx->base.len == 4) {
            encode_block_ex(ctx->base.buf, out, ctx->variant);
            out_used += 5;
            ctx->base.len = 0;
        }
    }
    size_t remaining = in_len - i;
    while (LIKELY(remaining >= 32)) {
        encode_8_blocks_ex(in + i, out + out_used, ctx->variant);
        out_used += 40;
        i += 32;
        remaining -= 32;
    }
    while (remaining >= 4) {
        encode_block_ex(in + i, out + out_used, ctx->variant);
        out_used += 5;
        i += 4;
        remaining -= 4;
    }
    while (i < in_len) {
        ctx->base.buf[ctx->base.len++] = in[i++];
    }
    return out_used;
}

size_t base85_enc_ex_final(base85_enc_ex_ctx_t *ctx, uint8_t *RESTRICT out) {
    if (!ctx || !out) return 0;
    if (ctx->base.len == 0) return 0;
    uint8_t pad[4] = {0};
    memcpy(pad, ctx->base.buf, ctx->base.len);
    uint8_t tmp[5];
    encode_block_ex(pad, tmp, ctx->variant);
    size_t out_len = ctx->base.len + 1;
    memcpy(out, tmp, out_len);
    ctx->base.len = 0;
    return out_len;
}

size_t base85_dec_ex_update(base85_dec_ex_ctx_t *ctx, const uint8_t *RESTRICT in,
                            size_t in_len, uint8_t *RESTRICT out) {
    if (!ctx || !in || !out) return 0;
    if (in_len == 0) return 0;
    size_t out_used = 0;
    size_t i = 0;
    if (ctx->base.len > 0) {
        while (ctx->base.len < 5 && i < in_len) {
            ctx->base.buf[ctx->base.len++] = in[i++];
        }
        if (ctx->base.len == 5) {
            if (UNLIKELY(decode_block_ex(ctx->base.buf, out, ctx->variant) < 0)) {
                ctx->base.len = 0;
                return (size_t)-1;
            }
            out_used += 4;
            ctx->base.len = 0;
        }
    }
    size_t remaining = in_len - i;
    while (LIKELY(remaining >= 40)) {
        if (UNLIKELY(decode_8_blocks_ex(in + i, out + out_used, ctx->variant) < 0)) {
            ctx->base.len = 0;
            return (size_t)-1;
        }
        out_used += 32;
        i += 40;
        remaining -= 40;
    }
    while (remaining >= 5) {
        if (UNLIKELY(decode_block_ex(in + i, out + out_used, ctx->variant) < 0)) {
            ctx->base.len = 0;
            return (size_t)-1;
        }
        out_used += 4;
        i += 5;
        remaining -= 5;
    }
    while (i < in_len) {
        ctx->base.buf[ctx->base.len++] = in[i++];
    }
    return out_used;
}

size_t base85_dec_ex_final(base85_dec_ex_ctx_t *ctx, uint8_t *RESTRICT out) {
    if (!ctx || !out) return 0;
    if (ctx->base.len == 0) return 0;
    if (UNLIKELY(ctx->base.len < 2)) {
        ctx->base.len = 0;
        return (size_t)-1;
    }
    uint8_t pad[5] = {0};
    memcpy(pad, ctx->base.buf, ctx->base.len);
    uint8_t pc = get_pad_char(ctx->variant);
    for (int i = ctx->base.len; i < 5; i++) {
        pad[i] = pc;
    }
    uint8_t tmp[4];
    if (UNLIKELY(decode_block_ex(pad, tmp, ctx->variant) < 0)) {
        ctx->base.len = 0;
        return (size_t)-1;
    }
    size_t out_len = ctx->base.len - 1;
    memcpy(out, tmp, out_len);
    ctx->base.len = 0;
    return out_len;
}

size_t base85_encode_ex(base85_variant_t variant, const uint8_t *in,
                        size_t in_len, char *out, int flags) {
    (void)flags;
    if (!in || !out) return 0;
    base85_enc_ex_ctx_t ctx;
    base85_enc_ex_init(&ctx, variant);
    size_t out_len = 0;
    out_len += base85_enc_ex_update(&ctx, in, in_len, (uint8_t*)out);
    out_len += base85_enc_ex_final(&ctx, (uint8_t*)(out + out_len));
    out[out_len] = '\0';
    return out_len;
}

size_t base85_decode_ex(base85_variant_t variant, const char *in_str,
                        size_t in_len, uint8_t *out, int flags) {
    if (!in_str || !out) return 0;
    if (in_len == 0) in_len = strlen(in_str);
    if (in_len == 0) return 0;
    const char *data = in_str;
    size_t data_len = in_len;
    if (flags & BASE85_DECODE_STRIP_WRAPPER) {
        const char *stripped = NULL;
        size_t stripped_len = 0;
        if (base85_strip_wrapper(in_str, in_len, &stripped, &stripped_len)) {
            data = stripped;
            data_len = stripped_len;
        }
    }
    base85_dec_ex_ctx_t ctx;
    base85_dec_ex_init(&ctx, variant);
    size_t out_used = 0;
    int ignore_ws = (flags & BASE85_DECODE_IGNORE_WHITESPACE) != 0;
    for (size_t i = 0; i < data_len; i++) {
        unsigned char c = (unsigned char)data[i];
        if (ignore_ws && (c == ' ' || c == '\t' || c == '\n' ||
                          c == '\r' || c == '\f' || c == '\v'))
            continue;
        if (UNLIKELY(!is_valid_char(variant, c))) return (size_t)-1;
        ctx.base.buf[ctx.base.len++] = c;
        if (ctx.base.len == 5) {
            if (UNLIKELY(decode_block_ex(ctx.base.buf, out + out_used, variant) < 0)) {
                ctx.base.len = 0;
                return (size_t)-1;
            }
            out_used += 4;
            ctx.base.len = 0;
        }
    }
    if (ctx.base.len > 0) {
        if (UNLIKELY(ctx.base.len < 2)) return (size_t)-1;
        uint8_t pad[5];
        memcpy(pad, ctx.base.buf, ctx.base.len);
        uint8_t pc = get_pad_char(variant);
        for (int i = ctx.base.len; i < 5; i++) pad[i] = pc;
        uint8_t tmp[4];
        if (UNLIKELY(decode_block_ex(pad, tmp, variant) < 0)) return (size_t)-1;
        size_t bytes = ctx.base.len - 1;
        memcpy(out + out_used, tmp, bytes);
        out_used += bytes;
    }
    return out_used;
}

base85_variant_t base85_detect_variant(const char *in, size_t in_len) {
    if (!in || in_len == 0) return BASE85_VARIANT_ASCII85;
    const char *data = in;
    size_t data_len = in_len;
    const char *stripped = NULL;
    size_t stripped_len = 0;
    if (base85_strip_wrapper(in, in_len, &stripped, &stripped_len)) {
        data = stripped;
        data_len = stripped_len;
    }
    int has_ascii85_only = 0;
    int has_rfc1924_only = 0;
    for (size_t i = 0; i < data_len && i < 256; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == '"' || c == '\'' || c == '`' || c == '\\') {
            has_ascii85_only = 1;
        }
        if (c == '|' || c == '~') {
            has_rfc1924_only = 1;
        }
    }
    if (has_ascii85_only) return BASE85_VARIANT_ASCII85;
    if (has_rfc1924_only) return BASE85_VARIANT_RFC1924;
    return BASE85_VARIANT_ASCII85;
}

int base85_strip_wrapper(const char *in, size_t in_len,
                         const char **out_ptr, size_t *out_len) {
    if (!in || !out_ptr || !out_len || in_len < 2) return 0;
    int start = 0;
    int end = (int)in_len - 1;
    if (in[0] == '<') {
        if (in_len >= 4 && in[1] == '~' && in[end] == '>' && in[end-1] == '~') {
            start = 2;
            end -= 2;
        } else if (in[end] == '>') {
            start = 1;
            end -= 1;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
    if (start >= end) return 0;
    *out_ptr = in + start;
    *out_len = (size_t)(end - start + 1);
    return 1;
}
