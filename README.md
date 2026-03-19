# HIP Learning Notes

AMD ROCm HIP GPU 编程学习项目，包含教学示例、调试技巧和性能分析。

## 环境要求

- AMD ROCm 7.2+
- HIP 编译器 (hipcc)
- AMD GPU (RDNA3/RDNA4 或 CDNA 系列)

## 项目结构

```
hip_learning/
├── 01_vector_add/           # 基础示例：向量加法
│   ├── vector_add.cpp           # HIP 入门代码
│   ├── 1_dump_asm.sh            # 导出 GPU 汇编
│   └── 2_recompile.sh           # 重新编译修改后的 ISA
│
├── 02_debug_example/        # 调试技巧示例
│   ├── debug_demo.cpp           # 竞态条件和原子操作
│   └── assert_debug.cpp         # GPU assert 运行时检查
│
├── 03_profiling_and_perf/   # 性能分析
│   ├── 01_timing_basics/        # hipEvent 计时 API
│   ├── 02_rocprof_basics/       # rocprofv3 性能分析
│   ├── 03_memory_bandwidth/     # 内存带宽和合并访问
│   └── 04_occupancy/            # Wave 占用率计算
│
└── Doc/                     # 学习文档
    ├── LEARNING_PATH.md         # 完整学习路径
    ├── LEARNING_NOTES.md        # 核心笔记总结
    ├── ROCGDB_CHEATSHEET.md     # rocgdb 命令速查表
    ├── MODIFY_KERNEL_ISA.md     # 修改 GPU ISA 指南
    └── ROCPROFV3_HELP_CN.md     # rocprofv3 中文参考
```

## 快速开始

### 编译和运行

```bash
# 向量加法示例
cd 01_vector_add
make run

# 调试示例
cd 02_debug_example
hipcc -g -O0 -o debug_demo debug_demo.cpp
./debug_demo

# 性能测试
cd 03_profiling_and_perf/03_memory_bandwidth
make
./bandwidth_test
```

### GPU 调试

```bash
# 编译带调试符号
hipcc -g -O0 -o program program.cpp

# 使用 rocgdb 调试
rocgdb ./program
(gdb) break kernel_name
(gdb) run
(gdb) info threads      # 查看 GPU wavefronts
(gdb) info lanes        # 查看 lane 详情
(gdb) x/20i $pc         # 查看汇编指令
```

### 性能分析

```bash
# 使用 rocprofv3 追踪
rocprofv3 --runtime-trace -f csv -- ./program

# 查看 kernel 执行时间
cat */kernel_trace.csv
```

## 核心概念

### HIP 编程模型

```cpp
// 1. 分配 GPU 内存
float *d_A;
hipMalloc(&d_A, size);

// 2. 数据传输
hipMemcpy(d_A, h_A, size, hipMemcpyHostToDevice);

// 3. 启动 kernel
myKernel<<<blocks, threads>>>(d_A, N);

// 4. 同步等待
hipDeviceSynchronize();

// 5. 释放内存
hipFree(d_A);
```

### 线程索引计算

```cpp
__global__ void kernel(float* data, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < N) {
        data[idx] = ...;
    }
}
```

## 学习路径

1. **入门** - 运行 `01_vector_add`，理解基本工作流程
2. **调试** - 学习 `02_debug_example`，掌握 rocgdb 使用
3. **性能** - 完成 `03_profiling_and_perf` 各模块
4. **进阶** - 修改 GPU ISA，深入理解底层执行

详细笔记：[Doc/LEARNING_NOTES.md](Doc/LEARNING_NOTES.md)

## 关键发现

- **合并访问很重要** - 随机访问可导致带宽下降 95%
- **占用率影响性能** - LDS 使用过多会降低并行度
- **-O0 vs -O3 差异巨大** - 调试版本指令数可能是优化版的 5 倍
- **rocgdb thread ≠ lane** - GPU thread 显示的是 wavefront，用 `lane N` 切换

## 参考资源

- [ROCm 官方文档](https://rocm.docs.amd.com/)
- [HIP 编程指南](https://rocm.docs.amd.com/projects/HIP/en/latest/)
- [AMD GPU ISA 参考](https://gpuopen.com/amd-isa-documentation/)

## License

MIT
