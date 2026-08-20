#include "base85.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "arm_optimize.h"
#include "variants.h"
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
int base85_use_arm = 0;
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
INLINE void encode_block_c(const uint8_t *RESTRICT in, uint8_t *RESTRICT out) {
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
}
INLINE void encode_8_blocks_c(const uint8_t *RESTRICT in, uint8_t *RESTRICT out) {
    encode_block_c(in + 0,  out + 0);
    encode_block_c(in + 4,  out + 5);
    encode_block_c(in + 8,  out + 10);
    encode_block_c(in + 12, out + 15);
    encode_block_c(in + 16, out + 20);
    encode_block_c(in + 20, out + 25);
    encode_block_c(in + 24, out + 30);
    encode_block_c(in + 28, out + 35);
}
INLINE int decode_block_c(const uint8_t *RESTRICT in, uint8_t *RESTRICT out) {
    uint64_t n = 0;
    for (int i = 0; i < 5; i++) {
        unsigned char c = in[i];
        if (UNLIKELY(c < '!' || c > 'u')) return -1;
        n = n * 85u + (uint8_t)(c - '!');
    }
    out[0] = (uint8_t)(n >> 24);
    out[1] = (uint8_t)(n >> 16);
    out[2] = (uint8_t)(n >> 8);
    out[3] = (uint8_t)n;
    return 0;
}
INLINE int decode_8_blocks_c(const uint8_t *RESTRICT in, uint8_t *RESTRICT out) {
    if (decode_block_c(in + 0,  out + 0)  < 0) return -1;
    if (decode_block_c(in + 5,  out + 4)  < 0) return -1;
    if (decode_block_c(in + 10, out + 8)  < 0) return -1;
    if (decode_block_c(in + 15, out + 12) < 0) return -1;
    if (decode_block_c(in + 20, out + 16) < 0) return -1;
    if (decode_block_c(in + 25, out + 20) < 0) return -1;
    if (decode_block_c(in + 30, out + 24) < 0) return -1;
    if (decode_block_c(in + 35, out + 28) < 0) return -1;
    return 0;
}
void base85_enc_init(base85_enc_ctx_t *ctx) {
    if (!ctx) return;
    ctx->len = 0;
    ctx->use_arm = base85_use_arm;
}
void base85_enc_reset(base85_enc_ctx_t *ctx) {
    if (!ctx) return;
    ctx->len = 0;
    ctx->use_arm = base85_use_arm;
}
size_t base85_enc_update(base85_enc_ctx_t *ctx, const uint8_t *RESTRICT in,
                         size_t in_len, uint8_t *RESTRICT out) {
    if (!ctx || !in || !out) return 0;
    if (in_len == 0) return 0;
    size_t out_used = 0;
    size_t i = 0;
    if (ctx->len > 0) {
        while (ctx->len < 4 && i < in_len) {
            ctx->buf[ctx->len++] = in[i++];
        }
        if (ctx->len == 4) {
            if (ctx->use_arm && base85_arm_supported()) {
                out_used += base85_encode_block_arm(ctx->buf, out);
            } else {
                encode_block_c(ctx->buf, out);
                out_used += 5;
            }
            ctx->len = 0;
        }
    }
    size_t remaining = in_len - i;
    while (LIKELY(remaining >= 32)) {
        if (ctx->use_arm && base85_arm_supported()) {
            out_used += base85_encode_8_blocks_arm(in + i, out + out_used);
        } else {
            encode_8_blocks_c(in + i, out + out_used);
            out_used += 40;
        }
        i += 32;
        remaining -= 32;
    }
    while (remaining >= 4) {
        if (ctx->use_arm && base85_arm_supported()) {
            out_used += base85_encode_block_arm(in + i, out + out_used);
        } else {
            encode_block_c(in + i, out + out_used);
            out_used += 5;
        }
        i += 4;
        remaining -= 4;
    }
    while (i < in_len) {
        ctx->buf[ctx->len++] = in[i++];
    }
    return out_used;
}
size_t base85_enc_final(base85_enc_ctx_t *ctx, uint8_t *RESTRICT out) {
    if (!ctx || !out) return 0;
    if (ctx->len == 0) return 0;
    uint8_t pad[4] = {0};
    memcpy(pad, ctx->buf, ctx->len);
    uint8_t tmp[5];
    if (ctx->use_arm && base85_arm_supported()) {
        base85_encode_block_arm(pad, tmp);
    } else {
        encode_block_c(pad, tmp);
    }
    size_t out_len = ctx->len + 1;
    memcpy(out, tmp, out_len);
    ctx->len = 0;
    return out_len;
}
void base85_dec_init(base85_dec_ctx_t *ctx) {
    if (!ctx) return;
    ctx->len = 0;
    ctx->use_arm = base85_use_arm;
}
void base85_dec_reset(base85_dec_ctx_t *ctx) {
    if (!ctx) return;
    ctx->len = 0;
    ctx->use_arm = base85_use_arm;
}
size_t base85_dec_update(base85_dec_ctx_t *ctx, const uint8_t *RESTRICT in,
                         size_t in_len, uint8_t *RESTRICT out) {
    if (!ctx || !in || !out) return 0;
    if (in_len == 0) return 0;
    size_t out_used = 0;
    size_t i = 0;
    if (ctx->len > 0) {
        while (ctx->len < 5 && i < in_len) {
            ctx->buf[ctx->len++] = in[i++];
        }
        if (ctx->len == 5) {
            int r;
            if (ctx->use_arm && base85_arm_supported()) {
                r = base85_decode_block_arm(ctx->buf, out);
            } else {
                r = decode_block_c(ctx->buf, out);
            }
            if (UNLIKELY(r < 0)) {
                ctx->len = 0;
                return (size_t)-1;
            }
            out_used += 4;
            ctx->len = 0;
        }
    }
    size_t remaining = in_len - i;
    while (LIKELY(remaining >= 40)) {
        int r;
        if (ctx->use_arm && base85_arm_supported()) {
            r = base85_decode_8_blocks_arm(in + i, out + out_used);
        } else {
            r = decode_8_blocks_c(in + i, out + out_used);
        }
        if (UNLIKELY(r < 0)) {
            ctx->len = 0;
            return (size_t)-1;
        }
        out_used += 32;
        i += 40;
        remaining -= 40;
    }
    while (remaining >= 5) {
        int r;
        if (ctx->use_arm && base85_arm_supported()) {
            r = base85_decode_block_arm(in + i, out + out_used);
        } else {
            r = decode_block_c(in + i, out + out_used);
        }
        if (UNLIKELY(r < 0)) {
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
    int r;
    if (ctx->use_arm && base85_arm_supported()) {
        r = base85_decode_block_arm(pad, tmp);
    } else {
        r = decode_block_c(pad, tmp);
    }
    if (UNLIKELY(r < 0)) {
        ctx->len = 0;
        return (size_t)-1;
    }
    size_t out_len = ctx->len - 1;
    memcpy(out, tmp, out_len);
    ctx->len = 0;
    return out_len;
}
size_t base85_encode(const uint8_t *in, size_t in_len, char *out) {
    return base85_encode_ex(BASE85_VARIANT_ASCII85, in, in_len, out, 0);
}
size_t base85_decode(const char *in, size_t in_len, uint8_t *out, int flags) {
    return base85_decode_ex(BASE85_VARIANT_ASCII85, in, in_len, out, flags);
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

/* ===== 扩展API实现 ===== */
int base85_strip_wrapper(const char *in, size_t in_len,
                         const char **out_ptr, size_t *out_len) {
    if (!in || !out_ptr || !out_len) return 0;
    if (in_len >= 4 && in[0] == '<' && in[1] == '~' &&
        in[in_len - 2] == '~' && in[in_len - 1] == '>') {
        *out_ptr = in + 2;
        *out_len = in_len - 4;
        return 1;
    }
    *out_ptr = in;
    *out_len = in_len;
    return 0;
}

base85_variant_t base85_detect_variant(const char *in, size_t in_len) {
    if (!in || in_len == 0) return BASE85_VARIANT_ASCII85;
    const char *p = in;
    size_t n = in_len;
    if (n >= 4 && p[0] == '<' && p[1] == '~' && p[n - 2] == '~' && p[n - 1] == '>') {
        p += 2; n -= 4;
    }
    int has_z85_only = 0, has_rfc1924_only = 0, has_non_ascii85 = 0;
    int first_alpha_upper = -1;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c >= 'A' && c <= 'Z') { if (first_alpha_upper == -1) first_alpha_upper = 1; }
        else if (c >= 'a' && c <= 'z') { if (first_alpha_upper == -1) first_alpha_upper = 0; }
        if (c < '!' || c > 'u') has_non_ascii85 = 1;
        if (c == '.' || c == ':' || c == '/' || c == '[' || c == ']') has_z85_only = 1;
        if (c == ';' || c == '`' || c == '|' || c == '~') has_rfc1924_only = 1;
    }
    if (has_non_ascii85) {
        if (has_z85_only && !has_rfc1924_only) return BASE85_VARIANT_Z85;
        if (has_rfc1924_only && !has_z85_only) return BASE85_VARIANT_RFC1924;
        if (first_alpha_upper == 1) return BASE85_VARIANT_RFC1924;
        if (first_alpha_upper == 0) return BASE85_VARIANT_Z85;
        return BASE85_VARIANT_Z85;
    }
    return BASE85_VARIANT_ASCII85;
}

size_t base85_encode_ex(base85_variant_t variant, const uint8_t *in,
                        size_t in_len, char *out, int flags) {
    if (!out) return 0;
    if (in_len == 0) { out[0] = '\0'; return 0; }
    size_t out_pos = 0;
    if (flags & BASE85_ENCODE_ADD_WRAPPER) {
        out[out_pos++] = '<'; out[out_pos++] = '~';
    }
    if (variant == BASE85_VARIANT_ASCII85 || variant == BASE85_VARIANT_AUTO) {
        base85_enc_ctx_t ctx;
        base85_enc_init(&ctx);
        size_t n = base85_enc_update(&ctx, in, in_len, (uint8_t *)(out + out_pos));
        out_pos += n;
        n = base85_enc_final(&ctx, (uint8_t *)(out + out_pos));
        out_pos += n;
    } else {
        size_t i = 0;
        while (i + 4 <= in_len) {
            uint8_t tmp[5];
            base85_encode_block_var(variant, in + i, tmp);
            memcpy(out + out_pos, tmp, 5); out_pos += 5; i += 4;
        }
        if (i < in_len) {
            uint8_t pad[4] = {0};
            memcpy(pad, in + i, in_len - i);
            uint8_t tmp[5];
            base85_encode_block_var(variant, pad, tmp);
            size_t keep = in_len - i + 1;
            memcpy(out + out_pos, tmp, keep); out_pos += keep;
        }
    }
    if (flags & BASE85_ENCODE_ADD_WRAPPER) {
        out[out_pos++] = '~'; out[out_pos++] = '>';
    }
    out[out_pos] = '\0';
    return out_pos;
}

size_t base85_decode_ex(base85_variant_t variant, const char *in_str,
                        size_t in_len, uint8_t *out, int flags) {
    if (!out || !in_str) return (size_t)-1;
    if (in_len == 0) return 0;
    const char *data = in_str;
    size_t data_len = in_len;
    int allocated_data = 0;
    if (flags & BASE85_DECODE_STRIP_WRAPPER)
        base85_strip_wrapper(in_str, in_len, &data, &data_len);
    if (flags & BASE85_DECODE_IGNORE_WHITESPACE) {
        char *filtered = (char *)malloc(data_len + 1);
        if (!filtered) return (size_t)-1;
        size_t fi = 0;
        for (size_t i = 0; i < data_len; i++) {
            unsigned char c = (unsigned char)data[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') continue;
            filtered[fi++] = data[i];
        }
        data = filtered; data_len = fi; allocated_data = 1;
    }
    if (data_len == 0) {
        if (allocated_data) free((void *)data);
        return 0;
    }
    size_t out_pos = 0;
    if (variant == BASE85_VARIANT_ASCII85) {
        base85_dec_ctx_t ctx;
        base85_dec_init(&ctx);
        size_t n = base85_dec_update(&ctx, (const uint8_t *)data, data_len, out + out_pos);
        if (n == (size_t)-1) { if (allocated_data) free((void *)data); return (size_t)-1; }
        out_pos += n;
        n = base85_dec_final(&ctx, out + out_pos);
        if (n == (size_t)-1) { if (allocated_data) free((void *)data); return (size_t)-1; }
        out_pos += n;
    } else {
        size_t i = 0;
        while (i + 5 <= data_len) {
            uint8_t tmp[4];
            if (base85_decode_block_var(variant, (const uint8_t *)data + i, tmp) < 0) {
                if (allocated_data) free((void *)data); return (size_t)-1;
            }
            memcpy(out + out_pos, tmp, 4); out_pos += 4; i += 5;
        }
        if (i < data_len) {
            if (data_len - i < 2) { if (allocated_data) free((void *)data); return (size_t)-1; }
            uint8_t pad[5];
            memcpy(pad, data + i, data_len - i);
            const uint8_t *cs = base85_get_charset(variant);
            for (size_t k = data_len - i; k < 5; k++) pad[k] = cs[84];
            uint8_t tmp[4];
            if (base85_decode_block_var(variant, pad, tmp) < 0) {
                if (allocated_data) free((void *)data); return (size_t)-1;
            }
            size_t keep = data_len - i - 1;
            memcpy(out + out_pos, tmp, keep); out_pos += keep;
        }
    }
    if (allocated_data) free((void *)data);
    return out_pos;
}
