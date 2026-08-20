#ifndef BASE85_H
#define BASE85_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    BASE85_VARIANT_AUTO,
    BASE85_VARIANT_ASCII85,
    BASE85_VARIANT_Z85,
    BASE85_VARIANT_RFC1924
} base85_variant_t;
typedef struct {
    uint8_t buf[4];
    unsigned char len;
    int use_arm;
} base85_enc_ctx_t;
typedef struct {
    uint8_t buf[5];
    unsigned char len;
    int use_arm;
} base85_dec_ctx_t;
#define BASE85_DECODE_IGNORE_WHITESPACE  0x01
#define BASE85_DECODE_STRIP_WRAPPER      0x02
#define BASE85_ENCODE_ADD_WRAPPER        0x04

#define B85_MAGIC "B85Z"
#define B85_VERSION 0x01
#define B85_HEADER_SIZE 7
#define B85_FLAG_UNCOMPRESSED 0x00
#define B85_FLAG_COMPRESSED 0x01
#define B85_VARIANT_ASCII85 0x00
#define B85_VARIANT_Z85 0x01
#define B85_VARIANT_RFC1924 0x02

#define B85_OK 0
#define B85_ERR_MEMORY 1
#define B85_ERR_DECODE 2
#define B85_ERR_DECOMPRESS 3
#define B85_ERR_HEADER_VERSION 4
#define B85_ERR_HEADER_VARIANT 5
#define B85_ERR_COMPRESS 6

#define B85_ENTROPY_THRESHOLD 6.0
#define B85_ENTROPY_SAMPLE_LIMIT (1024U * 1024U)
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
size_t base85_encode_ex(base85_variant_t variant, const uint8_t *in, size_t in_len, char *out, int flags);
size_t base85_decode_ex(base85_variant_t variant, const char *in_str, size_t in_len, uint8_t *out, int flags);
base85_variant_t base85_detect_variant(const char *in, size_t in_len);
int base85_strip_wrapper(const char *in, size_t in_len, const char **out_ptr, size_t *out_len);
int base85_debug_level(void);
void base85_set_debug_level(int level);
void base85_run_self_tests(void);

int b85z_write_header(uint8_t *out, uint8_t flag, base85_variant_t variant);
int b85z_detect_header(const uint8_t *data, size_t len, uint8_t *flag,
                        base85_variant_t *variant, size_t *header_skip);
int base85_encode_pipeline(const uint8_t *in, size_t in_len,
                           uint8_t **out, size_t *out_len,
                           base85_variant_t variant,
                           int force_compress, int raw_output);
int base85_decode_pipeline(const uint8_t *in, size_t in_len,
                           uint8_t **out, size_t *out_len,
                           base85_variant_t default_variant);

extern int base85_use_arm;
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
#ifdef __cplusplus
}
#endif
#endif
