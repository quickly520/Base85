#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "base85.h"
#include "arm_optimize.h"
#include "stats.h"
#include "variants.h"
#define VERSION "1.2"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RESET   "\033[0m"
static void print_usage(const char *prog) {
    printf("用法: %s [选项] [参数]\n", prog);
    printf("\n选项:\n");
    printf("  -e <字符串>       编码字符串\n");
    printf("  -d <字符串>       解码字符串\n");
    printf("  -s <变体>         选择变体: ascii85 / z85 / rfc1924 (默认 ascii85)\n");
    printf("  -w <模式>         包裹符/换行: wrap / 数字(换行宽度) / 0(不换行)\n");
    printf("  -f <文件>         输入文件\n");
    printf("  -o <文件>         输出文件\n");
    printf("  -q                静默模式(只输出纯数据)\n");
    printf("  -A / --arm        启用 ARM 极限优化\n");
    printf("  -P / --stats      显示性能统计\n");
    printf("  -t                运行自测\n");
    printf("  -V                显示版本号\n");
    printf("  -h                显示帮助\n");
    printf("\n示例:\n");
    printf("  %s -e \"Hello\"\n", prog);
    printf("  %s -d \"87cURDZ\"\n", prog);
    printf("  %s -s z85 -e \"Hello\"\n", prog);
    printf("  %s -w wrap -e \"Hello\"\n", prog);
    printf("  %s -d \"<~87cURDZ>\"\n", prog);
    printf("  %s -w 80 -e -f input.bin\n", prog);
    printf("  %s -e -f input.bin -o output.txt\n", prog);
    printf("  %s -q -e \"Hello\"\n", prog);
    printf("  %s -A -e \"Hello\"\n", prog);
    printf("  %s -P -e \"Hello\"\n", prog);
    printf("  %s -e file1 file2 file3  批量处理\n", prog);
    printf("  %s -V\n", prog);
}
static void print_error(const char *msg) {
    fprintf(stderr, COLOR_RED "错误: %s" COLOR_RESET "\n", msg);
}
static void print_warning(const char *msg) {
    fprintf(stderr, COLOR_YELLOW "警告: %s" COLOR_RESET "\n", msg);
}
static void print_success(const char *msg) {
    printf(COLOR_GREEN "%s" COLOR_RESET "\n", msg);
}
static base85_variant_t parse_variant(const char *s) {
    if (!s) return BASE85_VARIANT_ASCII85;
    if (strcmp(s, "z85") == 0) return BASE85_VARIANT_Z85;
    if (strcmp(s, "rfc1924") == 0) return BASE85_VARIANT_RFC1924;
    return BASE85_VARIANT_ASCII85;
}
static size_t read_file(const char *path, uint8_t **data) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        char errmsg[256];
        snprintf(errmsg, sizeof(errmsg), "无法打开文件 '%s': %s", path, strerror(errno));
        print_error(errmsg);
        return 0;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        char errmsg[256];
        snprintf(errmsg, sizeof(errmsg), "获取文件状态失败 '%s': %s", path, strerror(errno));
        print_error(errmsg);
        fclose(fp);
        return 0;
    }
    size_t len = (size_t)st.st_size;
    *data = (uint8_t*)malloc(len);
    if (!*data) {
        print_error("内存分配失败");
        fclose(fp);
        return 0;
    }
    size_t nread = fread(*data, 1, len, fp);
    fclose(fp);
    if (nread != len) {
        char errmsg[256];
        snprintf(errmsg, sizeof(errmsg), "读取文件失败: 期望 %zu 字节, 实际 %zu", len, nread);
        print_error(errmsg);
        free(*data);
        *data = NULL;
        return 0;
    }
    return len;
}
static int write_file(const char *path, const uint8_t *data, size_t len) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        char errmsg[256];
        snprintf(errmsg, sizeof(errmsg), "无法写入文件 '%s': %s", path, strerror(errno));
        print_error(errmsg);
        return 0;
    }
    size_t nwrite = fwrite(data, 1, len, fp);
    fclose(fp);
    if (nwrite != len) {
        char errmsg[256];
        snprintf(errmsg, sizeof(errmsg), "写入文件失败: 期望 %zu 字节, 实际 %zu", len, nwrite);
        print_error(errmsg);
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
static int process_one(const char *input_str, const char *infile, const char *outfile,
                       base85_variant_t variant, int opt_encode, int opt_decode,
                       int wrap_mode, int add_wrapper, int opt_quiet, int opt_arm,
                       int opt_stats, const char *prog_name) {
    (void)prog_name;
    uint8_t *input_data = NULL;
    size_t input_len = 0;
    int input_allocated = 0;
    if (infile) {
        input_len = read_file(infile, &input_data);
        if (input_len == 0 || !input_data) {
            return 1;
        }
        input_allocated = 1;
    } else if (input_str) {
        input_len = strlen(input_str);
        input_data = (uint8_t*)malloc(input_len + 1);
        if (!input_data) {
            print_error("内存分配失败");
            return 1;
        }
        memcpy(input_data, input_str, input_len);
        input_data[input_len] = '\0';
        input_allocated = 1;
    } else {
        print_error("请提供输入数据");
        return 1;
    }
    size_t out_len = 0;
    uint8_t *out_data = NULL;
    base85_stats_t stats;
    if (opt_stats) {
        base85_stats_init(&stats);
        base85_stats_start(&stats);
    }
    if (opt_encode) {
        size_t enc_size = base85_encode_size(input_len);
        char *out_buf = (char*)malloc(enc_size + 8);
        if (!out_buf) {
            print_error("内存分配失败");
            if (input_allocated) free(input_data);
            return 1;
        }
        int flags = 0;
        if (add_wrapper) flags |= BASE85_ENCODE_ADD_WRAPPER;
        out_len = base85_encode_ex(variant, input_data, input_len, out_buf, flags);
        if (out_len == 0) {
            print_error("编码失败");
            free(out_buf);
            if (input_allocated) free(input_data);
            return 1;
        }
        out_data = (uint8_t*)out_buf;
    } else {
        size_t dec_size = base85_decode_size(input_len);
        uint8_t *out_buf = (uint8_t*)malloc(dec_size + 8);
        if (!out_buf) {
            print_error("内存分配失败");
            if (input_allocated) free(input_data);
            return 1;
        }
        int flags = 0;
        flags |= BASE85_DECODE_STRIP_WRAPPER;
        base85_variant_t dec_variant = variant;
        if (dec_variant == BASE85_VARIANT_AUTO) {
            dec_variant = base85_detect_variant((const char*)input_data, input_len);
            if (dec_variant == BASE85_VARIANT_AUTO) {
                dec_variant = BASE85_VARIANT_ASCII85;
            }
        }
        out_len = base85_decode_ex(dec_variant, (const char*)input_data, input_len, out_buf, flags);
        if (out_len == (size_t)-1) {
            print_error("解码失败：包含非法字符");
            free(out_buf);
            if (input_allocated) free(input_data);
            return 1;
        }
        out_data = out_buf;
    }
    if (opt_stats) {
        base85_stats_stop(&stats, input_len, out_len);
    }
    if (outfile) {
        if (!write_file(outfile, out_data, out_len)) {
            free(out_data);
            if (input_allocated) free(input_data);
            return 1;
        }
    } else {
        if(opt_encode) {
            if (!opt_quiet) {
                output_data(out_data, out_len, wrap_mode,
                            add_wrapper ? "<~" : NULL,
                            add_wrapper ? "~>" : NULL,
                            stdout);
            } else {
                fwrite(out_data, 1, out_len, stdout);
                printf("\n");
            }
        } else {
            fwrite(out_data,1,out_len,stdout);
        }
    }
    if (opt_stats) {
        base85_stats_print(&stats, stderr);
    }
    free(out_data);
    if (input_allocated) free(input_data);
    return 0;
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
    int opt_arm = 0;
    int opt_stats = 0;
    int opt_version = 0;
    int opt_help = 0;
    int opt_test = 0;
    int multi_file_start = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-e") == 0) {
            opt_encode = 1;
            if(i+1 < argc && argv[i+1][0]!='-') {
                opt_str = argv[++i];
            }
        } else if (strcmp(argv[i], "-d") == 0) {
            opt_decode = 1;
            if(i+1 < argc && argv[i+1][0]!='-') {
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
        } else if (strcmp(argv[i], "-A") == 0 || strcmp(argv[i], "--arm") == 0) {
            opt_arm = 1;
        } else if (strcmp(argv[i], "-P") == 0 || strcmp(argv[i], "--stats") == 0) {
            opt_stats = 1;
        } else if (strcmp(argv[i], "-t") ==0) {
            opt_test =1;
        } else if (strcmp(argv[i], "-V") == 0) {
            opt_version = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            opt_help = 1;
        } else {
            if (multi_file_start == -1 && opt_str == NULL && opt_infile == NULL) {
                multi_file_start = i;
            } else {
                fprintf(stderr, "未知参数: %s\n", argv[i]);
                print_usage(argv[0]);
                return 1;
            }
        }
    }
    if(opt_test) {
        base85_run_self_tests();
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
        print_error("请指定 -e 或 -d");
        print_usage(argv[0]);
        return 1;
    }
    if (opt_encode && opt_decode) {
        print_error("不能同时使用 -e 和 -d");
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
    if (opt_arm) {
        if (!base85_arm_supported()) {
            print_error("当前平台不支持 ARM 极限优化");
            return 1;
        }
        base85_use_arm = 1;
        print_success("ARM 极限优化已启用");
    }
    if (multi_file_start != -1) {
        for (int i = multi_file_start; i < argc; i++) {
            const char *fname = argv[i];
            char outname[256];
            snprintf(outname, sizeof(outname), "%s.85", fname);
            printf("处理文件: %s -> %s\n", fname, outname);
            int ret = process_one(NULL, fname, outname, variant, opt_encode, opt_decode,
                                  wrap_mode, add_wrapper, opt_quiet, opt_arm, opt_stats, argv[0]);
            if (ret != 0) {
                char errmsg[256];
                snprintf(errmsg, sizeof(errmsg), "处理文件 %s 失败", fname);
                print_error(errmsg);
                return 1;
            }
        }
        return 0;
    }
    return process_one(opt_str, opt_infile, opt_outfile, variant, opt_encode, opt_decode,
                       wrap_mode, add_wrapper, opt_quiet, opt_arm, opt_stats, argv[0]);
}
