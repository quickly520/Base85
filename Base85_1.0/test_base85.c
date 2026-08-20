#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "base85.h"
static void print_usage(const char *prog) {
    printf("用法: %s [选项] [参数]\n", prog);
    printf("  -e <字符串>     编码字符串\n");
    printf("  -d <字符串>     解码字符串\n");
    printf("  -ef <文件>      编码文件\n");
    printf("  -df <文件>      解码文件\n");
    printf("  -t              运行自测\n");
    printf("  -h              显示帮助\n");
    printf("  -v              增加调试级别（可重复使用）\n");
}
static void encode_string(const char *str) {
    if (!str) return;
    size_t len = strlen(str);
    size_t enc_size = base85_encode_size(len);
    char *enc = malloc(enc_size);
    if (!enc) return;
    size_t out_len = base85_encode((const uint8_t*)str, len, enc);
    enc[out_len] = '\0';
    printf("%s\n", enc);
    free(enc);
}
static void decode_string(const char *str) {
    if (!str) return;
    size_t len = strlen(str);
    size_t dec_size = base85_decode_size(len);
    uint8_t *dec = malloc(dec_size);
    if (!dec) return;
    size_t out_len = base85_decode(str, len, dec, BASE85_DECODE_IGNORE_WHITESPACE);
    if (out_len == (size_t)-1) {
        fprintf(stderr, "解码失败：包含非法字符\n");
        free(dec);
        return;
    }
    fwrite(dec, 1, out_len, stdout);
    printf("\n");
    free(dec);
}
static size_t get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (size_t)st.st_size;
}
static void encode_file(const char *path) {
    if (!path) return;
    FILE *fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "无法打开文件: %s\n", path); return; }
    size_t len = get_file_size(path);
    if (len == 0) { fclose(fp); return; }
    uint8_t *data = (uint8_t*)malloc(len);
    if (!data) { fclose(fp); return; }
    size_t nread = fread(data, 1, len, fp);
    if (nread != len) {
        fprintf(stderr, "文件读取失败\n");
        free(data);
        fclose(fp);
        return;
    }
    fclose(fp);
    size_t enc_size = base85_encode_size(len);
    char *enc = malloc(enc_size);
    if (!enc) { free(data); return; }
    size_t out_len = base85_encode(data, len, enc);
    enc[out_len] = '\0';
    fwrite(enc, 1, out_len, stdout);
    printf("\n");
    free(data);
    free(enc);
}
static void decode_file(const char *path) {
    if (!path) return;
    FILE *fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "无法打开文件: %s\n", path); return; }
    size_t len = get_file_size(path);
    if (len == 0) { fclose(fp); return; }
    char *data = (char*)malloc(len + 1);
    if (!data) { fclose(fp); return; }
    size_t nread = fread(data, 1, len, fp);
    if (nread != len) {
        fprintf(stderr, "文件读取失败\n");
        free(data);
        fclose(fp);
        return;
    }
    data[len] = '\0';
    fclose(fp);
    size_t dec_size = base85_decode_size(len);
    uint8_t *dec = malloc(dec_size);
    if (!dec) { free(data); return; }
    size_t out_len = base85_decode(data, len, dec, BASE85_DECODE_IGNORE_WHITESPACE);
    if (out_len == (size_t)-1) {
        fprintf(stderr, "解码失败：包含非法字符\n");
        free(data);
        free(dec);
        return;
    }
    fwrite(dec, 1, out_len, stdout);
    free(data);
    free(dec);
}
static void self_test(void) {
    struct { const char *data; size_t len; } tests[] = {
        { "", 0 },
        { "Hello", 5 },
        { "Hello, World!", 13 },
        { "A\0B\0C", 5 },
        { "The quick brown fox jumps over the lazy dog", 43 }
    };
    int passed = 0, failed = 0;
    for (size_t t = 0; t < sizeof(tests)/sizeof(tests[0]); t++) {
        const char *test_data = tests[t].data;
        size_t len = tests[t].len;
        size_t enc_size = base85_encode_size(len);
        char *enc = malloc(enc_size);
        if (!enc) { fprintf(stderr, "内存分配失败\n"); return; }
        size_t enc_len = base85_encode((const uint8_t*)test_data, len, enc);
        enc[enc_len] = '\0';
        size_t dec_size = base85_decode_size(enc_len);
        uint8_t *dec = malloc(dec_size);
        if (!dec) { free(enc); return; }
        size_t dec_len = base85_decode(enc, enc_len, dec, 0);
        if (dec_len == (size_t)-1 || dec_len != len || memcmp(test_data, dec, len) != 0) {
            printf("❌ 测试失败: \"%s\"\n", test_data);
            failed++;
        } else {
            printf("✅ 测试通过: \"%s\"\n", test_data);
            passed++;
        }
        free(enc);
        free(dec);
    }
    printf("\n总计: %d 通过, %d 失败\n", passed, failed);
}
int main(int argc, char *argv[]) {
    int debug_level = 0;
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-v") == 0) {
            debug_level++;
            for (int j = i; j < argc - 1; j++) argv[j] = argv[j+1];
            argc--;
        } else {
            i++;
        }
    }
    if (debug_level > 0) {
        base85_set_debug_level(debug_level);
        fprintf(stderr, "调试级别: %d\n", debug_level);
    }
    if (argc < 2) { print_usage(argv[0]); return 1; }
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) { print_usage(argv[0]); return 0; }
    if (strcmp(argv[1], "-t") == 0) { self_test(); return 0; }
    if (argc < 3) { print_usage(argv[0]); return 1; }
    if (strcmp(argv[1], "-e") == 0) encode_string(argv[2]);
    else if (strcmp(argv[1], "-d") == 0) decode_string(argv[2]);
    else if (strcmp(argv[1], "-ef") == 0) encode_file(argv[2]);
    else if (strcmp(argv[1], "-df") == 0) decode_file(argv[2]);
    else { print_usage(argv[0]); return 1; }
    return 0;
}
