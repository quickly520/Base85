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
#define VERSION "1.3"
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
    printf("  -z                强制压缩(仅配合 -e 使用)\n");
    printf("  --raw             输出纯Base85文本, 不带B85Z二进制头(兼容v1.2)\n");
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
    printf("  %s -z -e -f input.bin -o output.85\n", prog);
    printf("  %s --raw -e -f input.bin\n", prog);
    printf("  %s -q -e \"Hello\"\n", prog);
    printf("  %s -A -e \"Hello\"\n", prog);
    printf("  %s -P -e \"Hello\"\n", prog);
    printf("  %s -e file1 file2 file3  批量处理\n", prog);
    printf("  %s -V\n", prog);
    printf("\n无参数时进入REPL交互模式\n");
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
static const char *b85_error_string(int err)
{
    switch (err)
    {
        case B85_ERR_MEMORY: return "内存分配失败";
        case B85_ERR_DECODE: return "解码失败：包含非法字符";
        case B85_ERR_DECOMPRESS: return "解压失败（数据可能已损坏或不完整，指令越界）";
        case B85_ERR_HEADER_VERSION: return "B85Z文件头版本不支持";
        case B85_ERR_HEADER_VARIANT: return "B85Z文件头变体类型不支持";
        case B85_ERR_COMPRESS: return "压缩失败";
        default: return "未知错误";
    }
}

static int process_one(const char *input_str, const char *infile, const char *outfile,
                       base85_variant_t variant, int opt_encode, int opt_decode,
                       int wrap_mode, int add_wrapper, int opt_quiet, int opt_arm,
                       int opt_stats, int force_compress, int raw_output,
                       const char *prog_name) {
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
        size_t cap = 4096;
        size_t pos = 0;
        input_data = (uint8_t*)malloc(cap);
        if (!input_data) {
            print_error("内存分配失败");
            return 1;
        }
        while (1) {
            if (pos + 1 > cap) {
                cap *= 2;
                uint8_t *tmp = (uint8_t*)realloc(input_data, cap);
                if (!tmp) {
                    free(input_data);
                    print_error("内存分配失败");
                    return 1;
                }
                input_data = tmp;
            }
            size_t n = fread(input_data + pos, 1, cap - pos, stdin);
            pos += n;
            if (n == 0) break;
        }
        input_len = pos;
        input_allocated = 1;
        if (input_len == 0) {
            free(input_data);
            print_error("请提供输入数据");
            return 1;
        }
    }
    size_t out_len = 0;
    uint8_t *out_data = NULL;
    base85_stats_t stats;
    if (opt_stats) {
        base85_stats_init(&stats);
        base85_stats_start(&stats);
    }
    int ret = 0;
    if (opt_encode) {
        ret = base85_encode_pipeline(input_data, input_len, &out_data, &out_len,
                                      variant, force_compress, raw_output);
    } else {
        ret = base85_decode_pipeline(input_data, input_len, &out_data, &out_len,
                                      variant);
    }
    if (ret != B85_OK) {
        print_error(b85_error_string(ret));
        if (out_data) free(out_data);
        if (input_allocated) free(input_data);
        return 1;
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
static void repl_mode(base85_variant_t variant, int opt_arm)
{
    (void)opt_arm;
    printf("Base85 v%s REPL 交互模式 (输入 exit 退出)\n", VERSION);
    printf("默认编码; 输入 -d <内容> 解码\n");

    char line[4096];
    while (1)
    {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin))
            break;

        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (len == 0) continue;
        if (strcmp(line, "exit") == 0) break;

        int do_decode = 0;
        const char *content = line;
        if (strncmp(line, "-d ", 3) == 0)
        {
            do_decode = 1;
            content = line + 3;
        }

        uint8_t *out_data = NULL;
        size_t out_len = 0;
        int ret;

        if (do_decode)
        {
            ret = base85_decode_pipeline((const uint8_t *)content, strlen(content),
                                          &out_data, &out_len, variant);
        }
        else
        {
            ret = base85_encode_pipeline((const uint8_t *)content, strlen(content),
                                          &out_data, &out_len, variant, 0, 1);
        }

        if (ret != B85_OK)
        {
            fprintf(stderr, COLOR_RED "错误: %s" COLOR_RESET "\n", b85_error_string(ret));
        }
        else
        {
            fwrite(out_data, 1, out_len, stdout);
            printf("\n");
        }

        if (out_data) free(out_data);
    }
    printf("\n再见\n");
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
    int opt_force_compress = 0;
    int opt_raw = 0;
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
        } else if (strcmp(argv[i], "-z") == 0) {
            opt_force_compress = 1;
        } else if (strcmp(argv[i], "--raw") == 0) {
            opt_raw = 1;
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
    if (argc == 1) {
        base85_variant_t variant = parse_variant(opt_variant);
        if (opt_arm) {
            if (!base85_arm_supported()) {
                print_error("当前平台不支持 ARM 极限优化");
                return 1;
            }
            base85_use_arm = 1;
        }
        repl_mode(variant, opt_arm);
        return 0;
    }
    if (opt_force_compress && !opt_encode) {
        if (opt_decode) {
            print_error("-z 仅允许配合 -e 编码使用，不能配合 -d 解码");
        } else {
            print_error("-z 不能单独使用，必须配合 -e 编码");
        }
        return 1;
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
    int is_pipe = !isatty(STDIN_FILENO);
    if (multi_file_start != -1) {
        int file_count = argc - multi_file_start;
        if (opt_outfile && file_count > 1) {
            size_t total_size = 0;
            uint8_t **file_datas = (uint8_t **)malloc(sizeof(uint8_t *) * (size_t)file_count);
            size_t *file_lens = (size_t *)malloc(sizeof(size_t) * (size_t)file_count);
            if (!file_datas || !file_lens) {
                print_error("内存分配失败");
                if (file_datas) free(file_datas);
                if (file_lens) free(file_lens);
                return 1;
            }
            int ok = 1;
            for (int i = 0; i < file_count; i++) {
                file_datas[i] = NULL;
                file_lens[i] = read_file(argv[multi_file_start + i], &file_datas[i]);
                if (file_lens[i] == 0 || !file_datas[i]) {
                    ok = 0;
                    break;
                }
                total_size += file_lens[i];
            }
            if (ok) {
                uint8_t *combined = (uint8_t *)malloc(total_size + 1);
                if (!combined) {
                    print_error("内存分配失败");
                    ok = 0;
                } else {
                    size_t pos = 0;
                    for (int i = 0; i < file_count; i++) {
                        memcpy(combined + pos, file_datas[i], file_lens[i]);
                        pos += file_lens[i];
                    }
                    printf("批量拼接: %d 个文件 -> %s (%zu 字节)\n",
                           file_count, opt_outfile, total_size);

                    uint8_t *out_data = NULL;
                    size_t out_len = 0;
                    int ret;
                    if (opt_encode) {
                        ret = base85_encode_pipeline(combined, total_size, &out_data, &out_len,
                                                      variant, opt_force_compress, opt_raw);
                    } else {
                        ret = base85_decode_pipeline(combined, total_size, &out_data, &out_len,
                                                      variant);
                    }
                    if (ret != B85_OK) {
                        print_error(b85_error_string(ret));
                        ok = 0;
                    } else {
                        if (!write_file(opt_outfile, out_data, out_len)) {
                            ok = 0;
                        }
                    }
                    if (out_data) free(out_data);
                    free(combined);
                }
            }
            for (int i = 0; i < file_count; i++) {
                if (file_datas[i]) free(file_datas[i]);
            }
            free(file_datas);
            free(file_lens);
            return ok ? 0 : 1;
        }
        for (int i = multi_file_start; i < argc; i++) {
            const char *fname = argv[i];
            char outname[256];
            snprintf(outname, sizeof(outname), "%s.85", fname);
            printf("处理文件: %s -> %s\n", fname, outname);
            int ret = process_one(NULL, fname, outname, variant, opt_encode, opt_decode,
                                  wrap_mode, add_wrapper, opt_quiet, opt_arm, opt_stats,
                                  opt_force_compress, opt_raw, argv[0]);
            if (ret != 0) {
                char errmsg[256];
                snprintf(errmsg, sizeof(errmsg), "处理文件 %s 失败", fname);
                print_error(errmsg);
                return 1;
            }
        }
        return 0;
    }
    (void)is_pipe;
    return process_one(opt_str, opt_infile, opt_outfile, variant, opt_encode, opt_decode,
                       wrap_mode, add_wrapper, opt_quiet, opt_arm, opt_stats,
                       opt_force_compress, opt_raw, argv[0]);
}
