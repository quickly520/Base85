#ifndef ARM_OPTIMIZE_H
#define ARM_OPTIMIZE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int base85_arm_supported(void);

size_t base85_encode_block_arm(const uint8_t *in, uint8_t *out);
size_t base85_encode_8_blocks_arm(const uint8_t *in, uint8_t *out);

int base85_decode_block_arm(const uint8_t *in, uint8_t *out);
int base85_decode_8_blocks_arm(const uint8_t *in, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif
