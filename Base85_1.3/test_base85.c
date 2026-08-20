#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "base85.h"
#include "entropy.h"
#include "compress.h"

#define EPS_ENTROPY 1e-5

static int g_passed = 0;
static int g_failed = 0;

static void test_pass(const char *name)
{
    printf("PASS: %s\n", name);
    g_passed++;
}

static void test_fail(const char *name, const char *detail)
{
    printf("FAIL: %s - %s\n", name, detail ? detail : "");
    g_failed++;
}

static int double_eq(double a, double b, double eps)
{
    double d = a - b;
    if (d < 0) d = -d;
    return d < eps;
}

static void test_v12_roundtrip(void)
{
    struct { const char *data; size_t len; } tests[] = {
        { "", 0 },
        { "Hello", 5 },
        { "Hello, World!", 13 },
        { "A\0B\0C", 5 },
        { "The quick brown fox jumps over the lazy dog", 43 }
    };
    int total = (int)(sizeof(tests)/sizeof(tests[0]));
    for (int t = 0; t < total; t++) {
        const char *test_data = tests[t].data;
        size_t len = tests[t].len;
        size_t enc_size = base85_encode_size(len);
        char *enc = malloc(enc_size);
        if (!enc) { test_fail("v12_roundtrip", "malloc failed"); return; }
        size_t enc_len = base85_encode((const uint8_t*)test_data, len, enc);
        enc[enc_len] = '\0';
        size_t dec_size = base85_decode_size(enc_len);
        uint8_t *dec = malloc(dec_size);
        if (!dec) { free(enc); test_fail("v12_roundtrip", "malloc failed"); return; }
        size_t dec_len = base85_decode(enc, enc_len, dec, 0);
        if (dec_len == (size_t)-1 || dec_len != len || memcmp(test_data, dec, len) != 0) {
            char detail[256];
            snprintf(detail, sizeof(detail), "enc_len=%zu, dec_len=%zu, expected=%zu",
                     enc_len, dec_len, len);
            test_fail("v12_roundtrip", detail);
        } else {
            char name[128];
            snprintf(name, sizeof(name), "v12_roundtrip(\"%s\")", test_data);
            test_pass(name);
        }
        free(enc);
        free(dec);
    }
}

static void test_entropy(void)
{
    uint8_t all_zero[256];
    memset(all_zero, 0, sizeof(all_zero));
    double e0 = calculate_entropy(all_zero, sizeof(all_zero));
    if (double_eq(e0, 0.0, EPS_ENTROPY))
        test_pass("entropy_all_zero");
    else {
        char detail[64];
        snprintf(detail, sizeof(detail), "got %.6f, expected 0.0", e0);
        test_fail("entropy_all_zero", detail);
    }

    uint8_t uniform[256];
    for (int i = 0; i < 256; i++) uniform[i] = (uint8_t)i;
    double e1 = calculate_entropy(uniform, sizeof(uniform));
    if (double_eq(e1, 8.0, EPS_ENTROPY))
        test_pass("entropy_uniform_8bit");
    else {
        char detail[64];
        snprintf(detail, sizeof(detail), "got %.6f, expected 8.0", e1);
        test_fail("entropy_uniform_8bit", detail);
    }

    const char *text = "Hello World! Hello World! Hello World!";
    double e2 = calculate_entropy((const uint8_t *)text, strlen(text));
    if (e2 > 2.0 && e2 < 5.0)
        test_pass("entropy_text_range");
    else {
        char detail[64];
        snprintf(detail, sizeof(detail), "got %.6f, expected 2.0-5.0", e2);
        test_fail("entropy_text_range", detail);
    }

    double e3 = sample_entropy(uniform, sizeof(uniform), 128);
    if (e3 > 0)
        test_pass("entropy_sample");
    else
        test_fail("entropy_sample", "returned 0");

    double e4 = calculate_entropy(NULL, 0);
    if (double_eq(e4, 0.0, EPS_ENTROPY))
        test_pass("entropy_empty");
    else
        test_fail("entropy_empty", "non-zero for empty");
}

static void test_lz77_roundtrip(void)
{
    const char *repetitive = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                              "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
                              "ABCABCABCABCABCABCABCABCABCABCABCABCABCABCABC";
    size_t in_len = strlen(repetitive);
    uint8_t *comp = malloc(in_len + 256);
    uint8_t *decomp = malloc(in_len + 256);
    if (!comp || !decomp) {
        if (comp) free(comp);
        if (decomp) free(decomp);
        test_fail("lz77_roundtrip", "malloc failed");
        return;
    }
    size_t comp_len = lz77_compress((const uint8_t *)repetitive, in_len, comp, in_len + 256);
    if (comp_len == 0) {
        test_fail("lz77_roundtrip", "compress returned 0");
        free(comp); free(decomp);
        return;
    }
    size_t decomp_len = lz77_decompress(comp, comp_len, decomp, in_len + 256);
    if (decomp_len == in_len && memcmp(repetitive, decomp, in_len) == 0)
        test_pass("lz77_roundtrip_repetitive");
    else
        test_fail("lz77_roundtrip_repetitive", "decompress mismatch");
    free(comp); free(decomp);
}

static void test_lz77_random(void)
{
    uint8_t data[4096];
    srand(42);
    for (int i = 0; i < 4096; i++) data[i] = (uint8_t)(rand() & 0xFF);
    uint8_t *comp = malloc(8192);
    uint8_t *decomp = malloc(8192);
    if (!comp || !decomp) {
        if (comp) free(comp);
        if (decomp) free(decomp);
        test_fail("lz77_random", "malloc failed");
        return;
    }
    size_t comp_len = lz77_compress(data, 4096, comp, 8192);
    if (comp_len == 0) {
        test_fail("lz77_random", "compress returned 0");
        free(comp); free(decomp);
        return;
    }
    size_t decomp_len = lz77_decompress(comp, comp_len, decomp, 8192);
    if (decomp_len == 4096 && memcmp(data, decomp, 4096) == 0)
        test_pass("lz77_roundtrip_random");
    else
        test_fail("lz77_roundtrip_random", "decompress mismatch");
    free(comp); free(decomp);
}

static void test_lz77_malicious(void)
{
    uint8_t bad1[] = { 0x80, 0x00, 0x01 };
    uint8_t out[256];
    size_t r = lz77_decompress(bad1, sizeof(bad1), out, sizeof(out));
    if (r == 0) test_pass("lz77_malicious_distance_zero");
    else test_fail("lz77_malicious_distance_zero", "should fail");

    uint8_t bad2[] = { 0x80, 0xFF, 0xFF };
    r = lz77_decompress(bad2, sizeof(bad2), out, sizeof(out));
    if (r == 0) test_pass("lz77_malicious_distance_overflow");
    else test_fail("lz77_malicious_distance_overflow", "should fail");

    uint8_t bad3[] = { 0x05, 'H', 'e', 'l', 'l' };
    r = lz77_decompress(bad3, sizeof(bad3), out, sizeof(out));
    if (r == 0) test_pass("lz77_malicious_truncated_literal");
    else test_fail("lz77_malicious_truncated_literal", "should fail");

    uint8_t bad4[] = { 0x81 };
    r = lz77_decompress(bad4, sizeof(bad4), out, sizeof(out));
    if (r == 0) test_pass("lz77_malicious_truncated_match");
    else test_fail("lz77_malicious_truncated_match", "should fail");
}

static void test_b85z_header(void)
{
    uint8_t hdr[7];
    b85z_write_header(hdr, B85_FLAG_COMPRESSED, BASE85_VARIANT_Z85);
    if (hdr[0] == 'B' && hdr[1] == '8' && hdr[2] == '5' && hdr[3] == 'Z' &&
        hdr[4] == B85_VERSION && hdr[5] == B85_FLAG_COMPRESSED && hdr[6] == B85_VARIANT_Z85)
        test_pass("b85z_header_write");
    else
        test_fail("b85z_header_write", "header bytes incorrect");

    uint8_t flag = 0;
    base85_variant_t var = BASE85_VARIANT_ASCII85;
    size_t skip = 0;
    int r = b85z_detect_header(hdr, sizeof(hdr), &flag, &var, &skip);
    if (r == 1 && flag == B85_FLAG_COMPRESSED && var == BASE85_VARIANT_Z85 && skip == 7)
        test_pass("b85z_header_detect");
    else
        test_fail("b85z_header_detect", "detection failed");

    uint8_t bad_ver[] = { 'B','8','5','Z', 0x99, 0x00, 0x00 };
    r = b85z_detect_header(bad_ver, sizeof(bad_ver), &flag, &var, &skip);
    if (r == -B85_ERR_HEADER_VERSION)
        test_pass("b85z_header_bad_version");
    else
        test_fail("b85z_header_bad_version", "should return version error");

    uint8_t bad_var[] = { 'B','8','5','Z', 0x01, 0x00, 0x99 };
    r = b85z_detect_header(bad_var, sizeof(bad_var), &flag, &var, &skip);
    if (r == -B85_ERR_HEADER_VARIANT)
        test_pass("b85z_header_bad_variant");
    else
        test_fail("b85z_header_bad_variant", "should return variant error");

    const char *raw = "87cURDZ";
    r = b85z_detect_header((const uint8_t *)raw, strlen(raw), &flag, &var, &skip);
    if (r == 0 && skip == 0)
        test_pass("b85z_header_raw_fallback");
    else
        test_fail("b85z_header_raw_fallback", "should detect raw text");
}

static void test_pipeline_roundtrip(base85_variant_t variant, const char *vname)
{
    const char *data = "Base85 v1.3 pipeline test with some repetitive data "
                       "Base85 v1.3 pipeline test with some repetitive data "
                       "Base85 v1.3 pipeline test with some repetitive data";
    size_t data_len = strlen(data);

    uint8_t *enc = NULL;
    size_t enc_len = 0;
    int ret = base85_encode_pipeline((const uint8_t *)data, data_len, &enc, &enc_len,
                                      variant, 0, 0);
    if (ret != B85_OK || !enc || enc_len < 7) {
        char detail[128];
        snprintf(detail, sizeof(detail), "encode failed ret=%d", ret);
        test_fail(vname, detail);
        if (enc) free(enc);
        return;
    }
    if (memcmp(enc, "B85Z", 4) == 0)
        test_pass("pipeline_header_present");
    else {
        test_fail(vname, "missing B85Z header");
        free(enc);
        return;
    }

    uint8_t *dec = NULL;
    size_t dec_len = 0;
    ret = base85_decode_pipeline(enc, enc_len, &dec, &dec_len, variant);
    if (ret == B85_OK && dec_len == data_len && memcmp(data, dec, data_len) == 0)
        test_pass(vname);
    else {
        char detail[128];
        snprintf(detail, sizeof(detail), "decode failed ret=%d dec_len=%zu", ret, dec_len);
        test_fail(vname, detail);
    }
    free(enc);
    if (dec) free(dec);
}

static void test_pipeline_raw(void)
{
    const char *data = "Hello World!";
    size_t data_len = strlen(data);
    uint8_t *enc = NULL;
    size_t enc_len = 0;
    int ret = base85_encode_pipeline((const uint8_t *)data, data_len, &enc, &enc_len,
                                      BASE85_VARIANT_ASCII85, 0, 1);
    if (ret != B85_OK || !enc) {
        test_fail("pipeline_raw", "encode failed");
        return;
    }
    if (enc_len >= 4 && memcmp(enc, "B85Z", 4) == 0)
        test_fail("pipeline_raw", "should not have B85Z header");
    else
        test_pass("pipeline_raw_no_header");

    uint8_t *dec = NULL;
    size_t dec_len = 0;
    ret = base85_decode_pipeline(enc, enc_len, &dec, &dec_len, BASE85_VARIANT_ASCII85);
    if (ret == B85_OK && dec_len == data_len && memcmp(data, dec, data_len) == 0)
        test_pass("pipeline_raw_roundtrip");
    else
        test_fail("pipeline_raw_roundtrip", "decode mismatch");
    free(enc);
    if (dec) free(dec);
}

static void test_pipeline_force_compress(void)
{
    uint8_t data[8192];
    memset(data, 'A', sizeof(data));
    uint8_t *enc = NULL;
    size_t enc_len = 0;
    int ret = base85_encode_pipeline(data, sizeof(data), &enc, &enc_len,
                                      BASE85_VARIANT_ASCII85, 1, 0);
    if (ret != B85_OK || !enc) {
        test_fail("pipeline_force_compress", "encode failed");
        return;
    }
    if (enc[5] == B85_FLAG_COMPRESSED)
        test_pass("pipeline_force_compress_flag");
    else
        test_fail("pipeline_force_compress_flag", "compressed flag not set");

    uint8_t *dec = NULL;
    size_t dec_len = 0;
    ret = base85_decode_pipeline(enc, enc_len, &dec, &dec_len, BASE85_VARIANT_ASCII85);
    if (ret == B85_OK && dec_len == sizeof(data) && memcmp(data, dec, sizeof(data)) == 0)
        test_pass("pipeline_force_compress_roundtrip");
    else
        test_fail("pipeline_force_compress_roundtrip", "decode mismatch");
    free(enc);
    if (dec) free(dec);
}

static void test_pipeline_empty(void)
{
    uint8_t *enc = NULL;
    size_t enc_len = 0;
    int ret = base85_encode_pipeline(NULL, 0, &enc, &enc_len,
                                      BASE85_VARIANT_ASCII85, 0, 0);
    if (ret == B85_OK && enc_len == 0)
        test_pass("pipeline_empty_encode");
    else
        test_fail("pipeline_empty_encode", "should return 0 length");

    uint8_t *dec = NULL;
    size_t dec_len = 0;
    ret = base85_decode_pipeline(NULL, 0, &dec, &dec_len, BASE85_VARIANT_ASCII85);
    if (ret == B85_OK && dec_len == 0)
        test_pass("pipeline_empty_decode");
    else
        test_fail("pipeline_empty_decode", "should return 0 length");
}

static void test_arm_switch(void)
{
    int old = base85_use_arm;
    base85_use_arm = 0;
    const char *data = "ARM switch test";
    size_t enc_size = base85_encode_size(strlen(data));
    char *enc = malloc(enc_size);
    uint8_t *dec = malloc(enc_size);
    if (!enc || !dec) {
        if (enc) free(enc);
        if (dec) free(dec);
        test_fail("arm_switch", "malloc failed");
        base85_use_arm = old;
        return;
    }
    size_t enc_len = base85_encode((const uint8_t *)data, strlen(data), enc);
    size_t dec_len = base85_decode(enc, enc_len, dec, 0);
    int ok1 = (dec_len == strlen(data) && memcmp(data, dec, dec_len) == 0);
    free(enc); free(dec);

    base85_use_arm = 1;
    enc_size = base85_encode_size(strlen(data));
    enc = malloc(enc_size);
    dec = malloc(enc_size);
    size_t enc_len2 = base85_encode((const uint8_t *)data, strlen(data), enc);
    size_t dec_len2 = base85_decode(enc, enc_len2, dec, 0);
    int ok2 = (dec_len2 == strlen(data) && memcmp(data, dec, dec_len2) == 0);
    free(enc); free(dec);
    base85_use_arm = old;

    if (ok1 && ok2)
        test_pass("arm_switch_functional");
    else
        test_fail("arm_switch_functional", "switch affects correctness");
}

void base85_run_self_tests(void) {
    g_passed = 0;
    g_failed = 0;

    printf("=== Base85 v1.3 自测开始 ===\n\n");

    printf("[v1.2 兼容测试]\n");
    test_v12_roundtrip();

    printf("\n[熵计算测试]\n");
    test_entropy();

    printf("\n[LZ4 变体压缩测试]\n");
    test_lz77_roundtrip();
    test_lz77_random();
    test_lz77_malicious();

    printf("\n[B85Z 文件头测试]\n");
    test_b85z_header();

    printf("\n[Pipeline 数据流测试]\n");
    test_pipeline_roundtrip(BASE85_VARIANT_ASCII85, "pipeline_ascii85");
    test_pipeline_roundtrip(BASE85_VARIANT_Z85, "pipeline_z85");
    test_pipeline_roundtrip(BASE85_VARIANT_RFC1924, "pipeline_rfc1924");
    test_pipeline_raw();
    test_pipeline_force_compress();
    test_pipeline_empty();

    printf("\n[ARM 开关测试]\n");
    test_arm_switch();

    printf("\n=== 自测结果: %d 通过, %d 失败 ===\n", g_passed, g_failed);
    if (g_failed == 0)
        printf("全部测试通过!\n");
    else
        printf("存在失败测试项!\n");
}
