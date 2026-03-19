# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

这是一个 AMD ROCm HIP 学习项目，包含多个教学示例，用于学习 GPU 并行编程。

**技术栈**: AMD ROCm HIP 7.2.26015 - 一个可在 AMD 和 NVIDIA GPU 之间移植的 C++ 运行时 API。

## 学习者背景

**用户背景**: DirectX 3D 驱动开发经验，熟悉 HLSL 和 GPU 工作流程

**学习目标**:
1. HIP 调试工具（rocgdb, compute-sanitizer）
2. 性能分析与优化（rocprof, roctracer, Omniperf）
3. 为 ROCm 开源项目贡献代码
4. GPGPU 特有算法（Reduction, Scan, 稀疏矩阵, 图算法）

**📖 详细学习路径**: 见 [Doc/LEARNING_PATH.md](Doc/LEARNING_PATH.md) - 包含 2 周速成计划和完整工具链指南

## 项目结构

```
hip_learning/
├── Doc/                     # 📁 项目文档
│   ├── LEARNING_PATH.md         # 完整学习路径和工具指南
│   ├── ROCGDB_CHEATSHEET.md     # rocgdb 命令速查表
│   ├── GPU_DISASSEMBLY.md       # GPU 汇编和反汇编对比
│   └── LEARNING_NOTES.md        # 学习笔记总结
├── 01_vector_add/           # ✅ 基础示例：向量加法
├── 02_debug_example/        # ✅ 调试技巧示例
│   ├── debug_demo.cpp           # 演示竞态条件和原子操作
│   ├── thread_debug.cpp         # 调试器断点演示
│   └── assert_debug.cpp         # GPU assert 运行时检查
├── 03_profiling_and_perf/   # 🚀 性能分析（学习中）
│   ├── 01_timing_basics/        # ✅ hipEvent 计时基础
│   ├── 02_rocprof_basics/       # 🎯 rocprof 性能分析（计划中）
│   ├── 03_memory_bandwidth/     # 🎯 内存带宽测试（计划中）
│   └── 04_occupancy/            # 🎯 占用率优化（计划中）
├── 04_compute_algorithms/   # 🎯 GPGPU 算法（计划中）
└── .vscode/                 # VS Code 配置（含调试配置）
```

## 构建和运行

### 01_vector_add (基础向量加法)

```bash
cd 01_vector_add
make          # 编译
make run      # 编译并运行
make clean    # 清理

# 手动编译
hipcc -o vector_add vector_add.cpp
```

### 02_debug_example (调试示例)

```bash
cd 02_debug_example

# 编译 debug_demo (演示竞态条件)
hipcc -o debug_demo debug_demo.cpp

# 编译 thread_debug (用于调试器)
hipcc -g -O0 -o thread_debug thread_debug.cpp

# 运行
./debug_demo
./thread_debug
```

**调试说明**:
- 使用 VS Code 的 `.vscode/launch.json` 配置进行 GPU kernel 调试
- `thread_debug.cpp` 专门设计用于练习设置断点和单步调试
- 使用 `rocgdb` 可以直接调试 GPU 代码

## HIP 编程核心模式

### 标准工作流程

1. **分配内存**:
   - 主机 (CPU): `malloc()` / `free()`
   - 设备 (GPU): `hipMalloc()` / `hipFree()`

2. **数据传输**:
   - 主机→设备: `hipMemcpy(dst, src, size, hipMemcpyHostToDevice)`
   - 设备→主机: `hipMemcpy(dst, src, size, hipMemcpyDeviceToHost)`

3. **启动 kernel**:
   ```cpp
   __global__ void kernel_name(args...) { ... }
   kernel_name<<<blocks, threads>>>(args...);
   ```

4. **同步**: `hipDeviceSynchronize()` - 等待 GPU 完成

### Kernel 编程要点

**线程索引**:
```cpp
int idx = blockIdx.x * blockDim.x + threadIdx.x;
```

**必须做边界检查**:
```cpp
if (idx < N) {
    // 处理数据
}
```

**执行配置**:
```cpp
int threadsPerBlock = 256;  // 常用值：128, 256, 512
int blocksPerGrid = (N + threadsPerBlock - 1) / threadsPerBlock;  // 向上取整
```

### 常见调试技巧

**1. Kernel 内打印**:
```cpp
if (threadIdx.x == 0) {
    printf("[DEBUG] Block %d, value = %d\n", blockIdx.x, value);
}
```

**2. 错误检查宏** (见 `02_debug_example/debug_demo.cpp`):
```cpp
#define HIP_CHECK(call) \
    do { \
        hipError_t err = call; \
        if (err != hipSuccess) { \
            printf("HIP Error: %s at %s:%d\n", \
                   hipGetErrorString(err), __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)
```

**3. 原子操作** (避免竞态条件):
```cpp
atomicMax(result, value);  // 多线程安全的最大值更新
atomicAdd(result, value);  // 多线程安全的加法
```

## 学习路径建议

1. **先运行 `01_vector_add`**: 理解基本的 GPU 工作流程
2. **查看 `debug_demo.cpp`**: 理解竞态条件问题和原子操作解决方案
3. **练习调试 `thread_debug.cpp`**: 学习使用调试器单步执行 kernel 代码

## 开发注意事项

- 代码包含中文注释用于教学
- 这些是学习示例，错误检查较少（生产代码应添加完整错误处理）
- 内存管理是手动的（学习项目不使用 RAII 封装）
- `02_debug_example` 故意包含有 bug 的代码用于演示调试技巧