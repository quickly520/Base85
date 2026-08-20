#ifndef ENTROPY_H
#define ENTROPY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

double calculate_entropy(const uint8_t *data, size_t len);
double sample_entropy(const uint8_t *data, size_t len, size_t sample_limit);

#ifdef __cplusplus
}
#endif

#endif
