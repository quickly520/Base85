#ifndef COMPRESS_H
#define COMPRESS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t lz77_compress(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_cap);
size_t lz77_decompress(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif
