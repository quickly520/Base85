#ifndef VARIANTS_H
#define VARIANTS_H

#include "base85.h"

#ifdef __cplusplus
extern "C" {
#endif

const uint8_t *base85_get_charset(base85_variant_t variant);
int base85_char_to_value(base85_variant_t variant, unsigned char c);
int base85_is_valid_char(base85_variant_t variant, unsigned char c);
int base85_encode_block_var(base85_variant_t variant, const uint8_t *in, uint8_t *out);
int base85_decode_block_var(base85_variant_t variant, const uint8_t *in, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif
