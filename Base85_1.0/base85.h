#ifndef BASE85_H
#define BASE85_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint8_t buf[4];
    unsigned char len;
} base85_enc_ctx_t;
typedef struct {
    uint8_t buf[5];
    unsigned char len;
} base85_dec_ctx_t;
#define BASE85_DECODE_IGNORE_WHITESPACE 0x01
size_t base85_encode(const uint8_t *in, size_t in_len, char *out);
size_t base85_decode(const char *in, size_t in_len, uint8_t *out, int flags);
int base85_is_valid(const char *in, size_t in_len, int flags);
void base85_enc_init(base85_enc_ctx_t *ctx);
void base85_enc_reset(base85_enc_ctx_t *ctx);
size_t base85_enc_update(base85_enc_ctx_t *ctx, const uint8_t *in, size_t in_len, uint8_t *out);
size_t base85_enc_final(base85_enc_ctx_t *ctx, uint8_t *out);
void base85_dec_init(base85_dec_ctx_t *ctx);
void base85_dec_reset(base85_dec_ctx_t *ctx);
size_t base85_dec_update(base85_dec_ctx_t *ctx, const uint8_t *in, size_t in_len, uint8_t *out);
size_t base85_dec_final(base85_dec_ctx_t *ctx, uint8_t *out);
int base85_debug_level(void);
void base85_set_debug_level(int level);
static inline size_t base85_encode_size(size_t n) {
    if (n == 0) return 1;
    if (n > SIZE_MAX - 3) return 0;
    size_t blocks = (n + 3) / 4;
    if (blocks > (SIZE_MAX - 1) / 5) return 0;
    return blocks * 5 + 1;
}
static inline size_t base85_decode_size(size_t n) {
    if (n == 0) return 1;
    if (n > SIZE_MAX - 4) return 0;
    size_t blocks = (n + 4) / 5;
    if (blocks > (SIZE_MAX - 1) / 4) return 0;
    return blocks * 4 + 1;
}
#ifdef BASE85_DEBUG
#include <stdio.h>
#include <stdlib.h>
void base85_enc_dump(const base85_enc_ctx_t *ctx);
void base85_dec_dump(const base85_dec_ctx_t *ctx);
#define DEBUG_PRINT(level, ...) \
    do { if (base85_debug_level() >= (level)) { \
        fprintf(stderr, "[BASE85] " __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } } while(0)
#else
#define DEBUG_PRINT(level, ...) ((void)0)
static inline void base85_enc_dump(const base85_enc_ctx_t *ctx) { (void)ctx; }
static inline void base85_dec_dump(const base85_dec_ctx_t *ctx) { (void)ctx; }
#endif
#ifdef __cplusplus
}
#endif
#endif
