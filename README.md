# Base85

ARM NEON 优化的 Base85 编码/解码工具，支持 Ascii85 / Z85 / RFC1924 三种变体。

## 三个版本

- **v1.0**：最小编码实现，核心编码/解码功能
- **v1.1**：增加命令行界面 + 三种变体支持
- **v1.2**：ARM NEON 优化 + 性能统计（当前最新）

## 功能

- 三种变体全支持（Ascii85 / Z85 / RFC1924）
- ARM NEON 汇编优化（v1.2）
- 静态编译，单文件运行
- 流式处理大文件

## 编译

```bash
# 通用编译
gcc -O2 *.c -lm -o base85

# ARM64 NEON 优化（Android / Termux）
clang -static -O3 -march=armv8-a+fp+simd *.c -o base85_arm64
```

## 用法

```bash
# 编码字符串
./base85 -e "Hello, World!"

# 解码字符串
./base85 -d "87cURD_*#4DfTZ)+T"

# 指定变体（Z85）
./base85 -s z85 -e "Hello"

# 编码文件
./base85 -e -f input.bin -o output.85

# 运行自测
./base85 -t
```

## 版本历史

| 版本 | 新增功能 |
|------|----------|
| v1.0 | 核心编码/解码 |
| v1.1 | CLI + 三种变体 |
| v1.2 | ARM NEON + 性能统计 |

## 许可证

MIT License

## 作者

quickly520