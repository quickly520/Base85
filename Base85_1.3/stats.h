#ifndef STATS_H
#define STATS_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint64_t elapsed_ms;
    size_t input_bytes;
    size_t output_bytes;
    uint64_t throughput_kb_s;
} base85_stats_t;
typedef struct {
    size_t total;
    size_t current;
    int enabled;
    int last_percent;
} base85_progress_t;
void base85_stats_init(base85_stats_t *stats);
void base85_stats_start(base85_stats_t *stats);
void base85_stats_stop(base85_stats_t *stats, size_t in_bytes, size_t out_bytes);
void base85_stats_print(const base85_stats_t *stats, FILE *out);
void base85_progress_init(base85_progress_t *prog, size_t total, int is_tty);
void base85_progress_update(base85_progress_t *prog, size_t processed, FILE *out);
void base85_progress_finish(base85_progress_t *prog, FILE *out);
#ifdef __cplusplus
}
#endif
#endif
