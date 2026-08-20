# Base85

ARM NEON 优化的 Base85 编码/解码工具，支持 Ascii85 / Z85 / RFC1924 三种变体。

## 功能

- 三种变体全支持（Ascii85 / Z85 / RFC1924）
- ARM NEON 汇编优化（v1.2+）
- 流式处理大文件
- LZ4 变体压缩（v1.3）
- 香农熵自动压缩决策（v1.3）
- 静态编译，单文件运行

## 编译

```bash
# 通用编译
gcc -O2 *.c -lm -o base85

# ARM64 NEON 优化（Android / Termux）
clang -static -O3 -march=armv8-a+fp+simd *.c -lm -o base85_arm64
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
| v1.3 | LZ4 压缩 + 智能决策 + 7字节文件头 |

## 许可证

MIT License

## 作者

quickly520