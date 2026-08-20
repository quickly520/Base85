#include "stats.h"
#include <stdio.h>
#include <time.h>
#if defined(__linux__) || defined(__unix__)
#include <sys/time.h>
#endif

static uint64_t get_time_ms(void)
{
#if defined(__linux__) || defined(__unix__)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000U + (uint64_t)tv.tv_usec / 1000U;
#else
    clock_t t = clock();
    return (uint64_t)t * 1000U / (uint64_t)CLOCKS_PER_SEC;
#endif
}

void base85_stats_init(base85_stats_t *stats)
{
    if (!stats) return;
    stats->elapsed_ms = 0U;
    stats->input_bytes = 0U;
    stats->output_bytes = 0U;
    stats->throughput_kb_s = 0U;
}

void base85_stats_start(base85_stats_t *stats)
{
    if (!stats) return;
    stats->elapsed_ms = get_time_ms();
    stats->input_bytes = 0U;
    stats->output_bytes = 0U;
    stats->throughput_kb_s = 0U;
}

void base85_stats_stop(base85_stats_t *stats, size_t in_bytes, size_t out_bytes)
{
    if (!stats) return;
    uint64_t now = get_time_ms();
    uint64_t delta_ms = now - stats->elapsed_ms;

    stats->elapsed_ms = delta_ms;
    stats->input_bytes = in_bytes;
    stats->output_bytes = out_bytes;

    if (delta_ms > 0U)
    {
        uint64_t total_bytes = (uint64_t)in_bytes;
        stats->throughput_kb_s = (total_bytes * 1000ULL) / (delta_ms * 1024ULL);
    }
    else
    {
        stats->throughput_kb_s = 0U;
    }
}

void base85_stats_print(const base85_stats_t *stats, FILE *out)
{
    if (!stats || !out) return;
    double mb_s = (double)stats->throughput_kb_s / 1024.0;
    fprintf(out,
        "输入: %zu 字节, 输出: %zu 字符, 耗时: %.3f ms, 吞吐量: %.2f MB/s\n",
        stats->input_bytes,
        stats->output_bytes,
        (double)stats->elapsed_ms,
        mb_s
    );
}

void base85_progress_init(base85_progress_t *prog, size_t total, int is_tty)
{
    if (!prog) return;
    prog->total = total;
    prog->current = 0;
    prog->enabled = is_tty && (total > 0);
    prog->last_percent = -1;
}

void base85_progress_update(base85_progress_t *prog, size_t processed, FILE *out)
{
    if (!prog || !out || !prog->enabled || prog->total == 0) return;
    prog->current = processed;
    if (prog->current > prog->total) prog->current = prog->total;
    int percent = (int)((prog->current * 100U) / prog->total);
    if (percent == prog->last_percent) return;
    prog->last_percent = percent;

    int bars = percent / 2;
    fprintf(out, "\r[");
    for (int i = 0; i < 50; i++)
    {
        if (i < bars) fputc('#', out);
        else fputc('-', out);
    }
    fprintf(out, "] %3d%% (%zu/%zu)", percent, prog->current, prog->total);
    fflush(out);
}

void base85_progress_finish(base85_progress_t *prog, FILE *out)
{
    if (!prog || !out || !prog->enabled) return;
    prog->current = prog->total;
    prog->last_percent = 100;
    fprintf(out, "\r[");
    for (int i = 0; i < 50; i++) fputc('#', out);
    fprintf(out, "] 100%% (%zu/%zu)\n", prog->total, prog->total);
    fflush(out);
}
