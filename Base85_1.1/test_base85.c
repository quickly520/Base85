#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "base85.h"

void self_test(void) {
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
        if (!enc) {
            fprintf(stderr, "内存分配失败\n");
            return;
        }
        size_t enc_len = base85_encode((const uint8_t*)test_data, len, enc);
        enc[enc_len] = '\0';
        size_t dec_size = base85_decode_size(enc_len);
        uint8_t *dec = malloc(dec_size);
        if (!dec) {
            free(enc);
            fprintf(stderr, "内存分配失败\n");
            return;
        }
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
