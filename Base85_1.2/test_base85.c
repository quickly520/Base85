#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "base85.h"
void base85_run_self_tests(void) {
    struct { const char *data; size_t len; } tests[] = {
        { "", 0 },
        { "Hello", 5 },
        { "Hello, World!", 13 },
        { "A\0B\0C", 5 },
        { "The quick brown fox jumps over the lazy dog", 43 }
    };
    int total = (int)(sizeof(tests)/sizeof(tests[0]));
    int passed = 0, failed = 0;
    for (int t = 0; t < total; t++) {
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
            printf("FAIL: \"%s\" (enc_len=%zu, dec_len=%zu, expected=%zu)\n",
                   test_data, enc_len, dec_len, len);
            failed++;
        } else {
            printf("PASS: \"%s\"\n", test_data);
            passed++;
        }
        free(enc);
        free(dec);
    }
    printf("\n%d/%d 全部通过\n", passed, total);
}
