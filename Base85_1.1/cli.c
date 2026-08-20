#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "base85.h"
#define VERSION "1.1"

static void print_usage(const char *prog) {
    printf("用法: %s [选项] [参数]\n", prog);
    printf("\n");
    printf("选项:\n");
    printf("  -e <字符串>      编码字符串\n");
    printf("  -d <字符串>      解码字符串\n");
    printf("  -t               运行自测\n");
    printf("  -s <变体>        选择变体: ascii85 / z85 / rfc1924 (默认 ascii85)\n");
    printf("  -w <模式>        包裹符/换行: wrap / 数字(换行宽度) / 0(不换行)\n");
    printf("  -f <文件>        输入文件\n");
    printf("  -o <文件>        输出文件\n");
    printf("  -q               静默模式(只输出纯数据)\n");
    printf("  -V               显示版本号\n");
    printf("  -h               显示帮助\n");
    printf("\n");
    printf("示例:\n");
    printf("  %s -e \"Hello\"\n", prog);
    printf("  %s -d \"87cURDZ\"\n", prog);
    printf("  %s -s z85 -e \"Hello\"\n", prog);
    printf("  %s -w wrap -e \"Hello\"\n", prog);
    printf("  %s -d \"<~87cURDZ~>\"\n", prog);
    printf("  %s -w 80 -e -f input.bin\n", prog);
    printf("  %s -e -f input.bin -o output.txt\n", prog);
    printf("  %s -q -e \"Hello\"\n", prog);
    printf("  %s -V\n", prog);
    printf("\n");
    printf("变体说明:\n");
    printf("  ascii85   : Adobe 标准，字符集 !~u\n");
    printf("  z85       : ZeroMQ 变体\n");
    printf("  rfc1924   : RFC 1924 标准\n");
    printf("\n");
    printf("解码时自动检测变体，包裹符自动去除 (< > 和 <~ ~>)。\n");
}

static base85_variant_t parse_variant(const char *s) {
    if (!s) return BASE85_VARIANT_ASCII85;
    if (strcmp(s, "z85") == 0) return BASE85_VARIANT_Z85;
    if (strcmp(s, "rfc1924") == 0) return BASE85_VARIANT_RFC1924;
    if (strcmp(s, "ascii85") == 0) return BASE85_VARIANT_ASCII85;
    return BASE85_VARIANT_ASCII85;
}

static size_t read_file(const char *path, uint8_t **data) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "无法打开文件: %s\n", path);
        return 0;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        fclose(fp);
        return 0;
    }
    size_t len = (size_t)st.st_size;
    *data = (uint8_t*)malloc(len);
    if (!*data) {
        fclose(fp);
        return 0;
    }
    size_t nread = fread(*data, 1, len, fp);
    fclose(fp);
    if (nread != len) {
        free(*data);
        *data = NULL;
        return 0;
    }
    return len;
}

static int write_file(const char *path, const uint8_t *data, size_t len) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "无法写入文件: %s\n", path);
        return 0;
    }
    size_t nwrite = fwrite(data, 1, len, fp);
    fclose(fp);
    if (nwrite != len) {
        fprintf(stderr, "写入文件失败\n");
        return 0;
    }
    return 1;
}

static void output_data(const uint8_t *data, size_t len, int wrap_mode,
                        const char *prefix, const char *suffix, FILE *fp) {
    if (prefix) fprintf(fp, "%s", prefix);
    if (wrap_mode > 0) {
        size_t pos = 0;
        while (pos < len) {
            size_t chunk = (len - pos < (size_t)wrap_mode) ? (len - pos) : (size_t)wrap_mode;
            fwrite(data + pos, 1, chunk, fp);
            pos += chunk;
            if (pos < len) fprintf(fp, "\n");
        }
    } else {
        fwrite(data, 1, len, fp);
    }
    if (suffix) fprintf(fp, "%s", suffix);
    fprintf(fp, "\n");
}

int main(int argc, char *argv[]) {
    const char *opt_str = NULL;
    const char *opt_infile = NULL;
    const char *opt_outfile = NULL;
    const char *opt_variant = "ascii85";
    const char *opt_wrap = NULL;
    int opt_encode = 0;
    int opt_decode = 0;
    int opt_quiet = 0;
    int opt_version = 0;
    int opt_help = 0;
    int opt_test = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0) {
            opt_test = 1;
        } else if (strcmp(argv[i], "-e") == 0) {
            opt_encode = 1;
            if (i+1 < argc && argv[i+1][0] != '-') {
                opt_str = argv[++i];
            }
        } else if (strcmp(argv[i], "-d") == 0) {
            opt_decode = 1;
            if (i+1 < argc && argv[i+1][0] != '-') {
                opt_str = argv[++i];
            }
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            opt_variant = argv[++i];
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            opt_wrap = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            opt_infile = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            opt_outfile = argv[++i];
        } else if (strcmp(argv[i], "-q") == 0) {
            opt_quiet = 1;
        } else if (strcmp(argv[i], "-V") == 0) {
            opt_version = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            opt_help = 1;
        } else {
            fprintf(stderr, "未知参数: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (opt_test) {
        self_test();
        return 0;
    }
    if (opt_version) {
        printf("base85 %s\n", VERSION);
        return 0;
    }
    if (opt_help) {
        print_usage(argv[0]);
        return 0;
    }
    if (!opt_encode && !opt_decode) {
        fprintf(stderr, "请指定 -e 或 -d\n");
        print_usage(argv[0]);
        return 1;
    }
    if (opt_encode && opt_decode) {
        fprintf(stderr, "不能同时使用 -e 和 -d\n");
        return 1;
    }

    base85_variant_t variant = parse_variant(opt_variant);
    int wrap_mode = 0;
    int add_wrapper = 0;
    if (opt_wrap) {
        if (strcmp(opt_wrap, "wrap") == 0) {
            add_wrapper = 1;
            wrap_mode = 0;
        } else {
            wrap_mode = atoi(opt_wrap);
            if (wrap_mode < 0) wrap_mode = 0;
        }
    }

    uint8_t *input_data = NULL;
    size_t input_len = 0;
    int input_allocated = 0;
    if (opt_infile) {
        input_len = read_file(opt_infile, &input_data);
        if (input_len == 0 || !input_data) {
            fprintf(stderr, "读取文件失败: %s\n", opt_infile);
            return 1;
        }
        input_allocated = 1;
    } else if (opt_str) {
        input_len = strlen(opt_str);
        input_data = (uint8_t*)malloc(input_len + 1);
        if (!input_data) {
            fprintf(stderr, "内存分配失败\n");
            return 1;
        }
        memcpy(input_data, opt_str, input_len);
        input_data[input_len] = '\0';
        input_allocated = 1;
    } else {
        fprintf(stderr, "请提供输入数据（字符串或 -f 文件）\n");
        return 1;
    }

    size_t out_len = 0;
    uint8_t *out_data = NULL;
    if (opt_encode) {
        size_t enc_size = base85_encode_size(input_len);
        char *out_buf = (char*)malloc(enc_size + 8);
        if (!out_buf) {
            fprintf(stderr, "内存分配失败\n");
            if (input_allocated) free(input_data);
            return 1;
        }
        out_len = base85_encode_ex(variant, input_data, input_len, out_buf, 0);
        if (out_len == 0) {
            fprintf(stderr, "编码失败\n");
            free(out_buf);
            if (input_allocated) free(input_data);
            return 1;
        }
        out_data = (uint8_t*)out_buf;
    } else {
        size_t dec_size = base85_decode_size(input_len);
        uint8_t *out_buf = (uint8_t*)malloc(dec_size + 8);
        if (!out_buf) {
            fprintf(stderr, "内存分配失败\n");
            if (input_allocated) free(input_data);
            return 1;
        }
        int flags = BASE85_DECODE_STRIP_WRAPPER;
        base85_variant_t dec_variant = variant;
        if (dec_variant == BASE85_VARIANT_AUTO) {
            dec_variant = base85_detect_variant((const char*)input_data, input_len);
        }
        out_len = base85_decode_ex(dec_variant, (const char*)input_data, input_len, out_buf, flags);
        if (out_len == (size_t)-1) {
            fprintf(stderr, "解码失败：包含非法字符\n");
            free(out_buf);
            if (input_allocated) free(input_data);
            return 1;
        }
        out_data = out_buf;
    }

    if (opt_outfile) {
        if (!write_file(opt_outfile, out_data, out_len)) {
            free(out_data);
            if (input_allocated) free(input_data);
            return 1;
        }
    } else {
        if (!opt_quiet) {
            output_data(out_data, out_len, wrap_mode,
                        add_wrapper ? "<~" : NULL,
                        add_wrapper ? "~>" : NULL,
                        stdout);
        } else {
            fwrite(out_data, 1, out_len, stdout);
        }
    }

    free(out_data);
    if (input_allocated) free(input_data);
    return 0;
}
