# 修改 GPU Kernel ISA 并调试 C++ 源码

本文档介绍如何手动修改 HIP kernel 的 GPU ISA（汇编指令），然后使用修改后的 kernel 代码调试原始 C++ 源码。

## 概述

```
┌─────────────────────────────────────────────────────────────────┐
│  C++ 源码 (vector_add.cpp)                                      │
│  ↓                                                              │
│  GPU 汇编 (.s)  ──修改──→  修改后汇编 (.s)                       │
│  ↓                              ↓                               │
│  目标文件 (.o)            修改后目标文件 (.o)                    │
│  ↓                              ↓                               │
│  共享对象 (.out)          修改后共享对象 (.out)                  │
│  ↓                              ↓                               │
│  Fatbin (.hipfb)          修改后 Fatbin (.hipfb)                │
│  ↓                              ↓                               │
│  可执行文件               修改后可执行文件                       │
│                           (C++ 源码 + 修改后 ISA)                │
└─────────────────────────────────────────────────────────────────┘
```

## 完整流程

### 步骤 1: 生成 GPU 汇编文件

```bash
cd 01_vector_add

# 生成带调试信息的汇编（用于调试）
hipcc -g -O0 --save-temps --offload-arch=gfx1201 -o vector_add_debug vector_add.cpp

# 或者生成优化后的汇编（用于性能分析）
hipcc -O3 --save-temps --offload-arch=gfx1201 -o vector_add vector_add.cpp
```

生成的文件：
```
vector_add-hip-amdgcn-amd-amdhsa-gfx1201.s   # GPU 汇编 ← 修改这个
vector_add-hip-amdgcn-amd-amdhsa-gfx1201.bc  # LLVM bitcode
vector_add-host-x86_64-unknown-linux-gnu.s   # Host 汇编
```

### 步骤 2: 查看和修改 ISA

查看关键指令：
```bash
# 查看 kernel 函数的汇编
grep -A50 "^_Z10vector_add" vector_add-hip-amdgcn-amd-amdhsa-gfx1201.s
```

示例输出（-O3 优化后）：
```asm
_Z10vector_addPKfS0_Pfi:
    ...
    global_load_b32 v2, v[2:3], off      # 加载 A[idx]
    global_load_b32 v3, v[4:5], off      # 加载 B[idx]
    s_wait_loadcnt 0x0                    # 等待加载完成
    v_add_f32_e32 v2, v2, v3             # C[idx] = A[idx] + B[idx] ← 关键指令
    global_store_b32 v[0:1], v2, off     # 存储结果
    s_endpgm
```

修改汇编：
```bash
# 复制原始文件
cp vector_add-hip-amdgcn-amd-amdhsa-gfx1201.s kernel_modified.s

# 修改指令：把加法改成乘法
sed -i 's/v_add_f32_e32 v2, v2, v3/v_mul_f32_e32 v2, v2, v3/' kernel_modified.s

# 如果是 -O0 编译的，指令格式可能是 e64：
sed -i 's/v_add_f32_e64 v2, v0, v1/v_mul_f32_e64 v2, v0, v1/' kernel_modified.s

# 验证修改
grep "v_mul_f32" kernel_modified.s
```

### 步骤 3: 汇编 .s → .o

```bash
/opt/rocm-7.2.0/lib/llvm/bin/llvm-mc \
  -triple=amdgcn-amd-amdhsa \
  -mcpu=gfx1201 \
  -filetype=obj \
  -g \
  -o kernel_modified.o \
  kernel_modified.s
```

### 步骤 4: 链接 .o → .out

```bash
/opt/rocm-7.2.0/lib/llvm/bin/lld \
  -flavor gnu \
  -m elf64_amdgpu \
  --no-undefined \
  -shared \
  -o kernel_modified.out \
  kernel_modified.o
```

### 步骤 5: 打包 .out → .hipfb

```bash
/opt/rocm-7.2.0/lib/llvm/bin/clang-offload-bundler \
  -type=o \
  -bundle-align=4096 \
  -targets=host-x86_64-unknown-linux-gnu,hipv4-amdgcn-amd-amdhsa--gfx1201 \
  -input=/dev/null \
  -input=kernel_modified.out \
  -output=kernel_modified.hipfb
```

### 步骤 6: 编译最终可执行文件

```bash
/opt/rocm-7.2.0/lib/llvm/bin/clang++ \
  -g -O0 \
  -x hip \
  --cuda-host-only \
  -Xclang -fcuda-include-gpubinary -Xclang kernel_modified.hipfb \
  -L/opt/rocm-7.2.0/lib -lamdhip64 \
  -o vector_add_debug_modified \
  vector_add.cpp
```

关键参数说明：
- `--cuda-host-only`: 只编译 host 代码，不重新编译 device 代码
- `-Xclang -fcuda-include-gpubinary`: 嵌入我们修改后的 kernel

## 调试方法

### 方法 1: 使用 rocgdb 命令行

```bash
rocgdb ./vector_add_debug_modified
```

```gdb
(gdb) set breakpoint pending on
(gdb) break vector_add.cpp:11          # 在 C++ 源码设断点
(gdb) run
(gdb) info threads                      # 查看 GPU 线程
(gdb) thread 7                          # 切换到 GPU wave
(gdb) print idx                         # 查看变量
(gdb) disassemble /r                    # 查看 ISA (会显示修改后的指令)
(gdb) next                              # 单步执行
```

### 方法 2: 使用 VSCode

1. 添加 launch.json 配置：
```json
{
    "name": "HIP Debug Modified Kernel",
    "type": "cppdbg",
    "request": "launch",
    "program": "${fileDirname}/vector_add_debug_modified",
    "cwd": "${fileDirname}",
    "environment": [
        {"name": "HSA_ENABLE_DEBUG", "value": "1"}
    ],
    "MIMode": "gdb",
    "miDebuggerPath": "/usr/bin/rocgdb",
    "setupCommands": [
        {"text": "set breakpoint pending on"}
    ]
}
```

2. 在 C++ 源码设置断点
3. 选择 "HIP Debug Modified Kernel" 配置
4. 按 F5 启动调试

## 常用 ISA 修改示例

### 算术运算修改

| 原始指令 | 修改后 | 效果 |
|----------|--------|------|
| `v_add_f32` | `v_mul_f32` | 加法 → 乘法 |
| `v_add_f32` | `v_sub_f32` | 加法 → 减法 |
| `v_add_f32` | `v_max_f32` | 加法 → 取最大值 |
| `v_mul_f32` | `v_fma_f32` | 乘法 → 乘加 |

### 指令格式

```asm
# e32 格式 (2 操作数，优化后常见)
v_add_f32_e32 v2, v2, v3       # v2 = v2 + v3

# e64 格式 (3 操作数，-O0 常见)
v_add_f32_e64 v2, v0, v1       # v2 = v0 + v1
```

### 内存操作

```asm
global_load_b32 v2, v[2:3], off    # 从全局内存加载 32 位
global_store_b32 v[0:1], v2, off   # 存储到全局内存
```

## 一键构建脚本

使用 `01_vector_add/build_modified_kernel.sh`:

```bash
# 默认：加法改乘法
./build_modified_kernel.sh

# 自定义修改
./build_modified_kernel.sh "s/v_add_f32_e64/v_sub_f32_e64/"
```

## 注意事项

1. **保持指令数量不变**: 如果修改改变了指令数量，调试信息的行号映射可能会偏移

2. **指定正确的 GPU 架构**: 使用 `rocminfo | grep gfx` 查看你的 GPU 架构

3. **-O0 vs -O3 指令格式不同**:
   - `-O0`: 通常使用 `v_xxx_e64` 格式
   - `-O3`: 通常使用 `v_xxx_e32` 格式，且有更多优化

4. **多 GPU 系统**: 使用 `--offload-arch=gfxXXXX` 只编译特定 GPU

## 常见 GPU 架构

| GPU 系列 | 架构 |
|----------|------|
| RX 7900 XTX/XT | gfx1100 |
| RX 7800/7700 | gfx1101 |
| RX 7600 | gfx1102 |
| Radeon 780M (集显) | gfx1103 |
| RX 9070 系列 | gfx1201 |
| RX 6900/6800 | gfx1030 |

## 参考资料

- [AMDGPU ISA 文档](https://llvm.org/docs/AMDGPUUsage.html)
- [ROCm 调试指南](https://rocm.docs.amd.com/projects/ROCgdb/en/latest/)
