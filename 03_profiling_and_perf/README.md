# 03_profiling_and_perf - GPU 性能分析与优化

## 📚 学习目标

掌握 GPU 性能分析工具和优化技术，学习如何测量、分析和优化 HIP kernel 的性能。

## 🎯 学习路径

### ✅ 01_timing_basics - 计时基础
**状态**: 已完成，可以开始学习

学习内容：
- 使用 hipEvent 测量 kernel 执行时间
- CPU 时间 vs GPU 时间
- 计算带宽和 GFLOPS
- 性能指标基础

**开始学习**：
```bash
cd 01_timing_basics
hipcc -O3 -o timing_demo timing_demo.cpp
./timing_demo
```

### 🎯 02_rocprof_basics - rocprof 性能分析
**状态**: 计划中

学习内容：
- rocprof 基础用法
- 收集硬件计数器
- API 追踪
- 性能瓶颈分析

### 🎯 03_memory_bandwidth - 内存带宽测试
**状态**: 计划中

学习内容：
- 测量理论 vs 实际带宽
- Coalesced vs Strided 访问
- 内存访问模式优化

### 🎯 04_occupancy - 占用率优化
**状态**: 计划中

学习内容：
- 什么是 occupancy
- 寄存器和共享内存的影响
- 优化 block size

## 🔧 推荐学习顺序

```
01_timing_basics (基础)
    ↓
02_rocprof_basics (工具)
    ↓
03_memory_bandwidth (优化)
    ↓
04_occupancy (高级优化)
```

## 📊 性能分析工具概览

| 工具 | 用途 | 难度 | ROCm 可用性 |
|------|------|------|------------|
| **hipEvent** | Kernel 计时 | ⭐ 简单 | ✅ 完全支持 |
| **rocprofv3** | 性能分析 | ⭐⭐ 中等 | ✅ 完全支持 |
| **roctracer** | API 追踪 | ⭐⭐ 中等 | ✅ 完全支持 |
| **Omniperf** | 详细分析 | ⭐⭐⭐ 高级 | ✅ 需要安装 |

## 💡 性能优化关键概念

### 1. 内存带宽
GPU 性能的主要瓶颈通常是内存访问。

**理论带宽**（AMD RX 7600 示例）：
```
理论带宽 ≈ 288 GB/s
```

**实际测量**：
```bash
cd 03_memory_bandwidth
./bandwidth_test
```

### 2. Occupancy（占用率）
同时在 GPU 上活跃的 wavefronts 比例。

**影响因素**：
- Block size
- 寄存器使用
- 共享内存使用

**查看方法**：
```bash
hipcc --ptxas-options=-v kernel.cpp
```

### 3. Compute Throughput
实际计算吞吐量 vs 理论峰值。

**测量方法**：
- 使用 rocprofv3 收集计数器
- 计算 GFLOPS
- 与理论峰值对比

## 🎓 性能优化流程

```
1. 基准测试（Baseline）
   ↓
2. 使用 rocprofv3 分析
   ↓
3. 识别瓶颈
   - 内存带宽受限？
   - 计算受限？
   - Occupancy 低？
   ↓
4. 针对性优化
   ↓
5. 验证提升
   ↓
6. 重复 2-5
```

## 📖 推荐阅读

### AMD 官方文档
- [ROCm Performance Guidelines](https://rocm.docs.amd.com/projects/HIP/en/latest/how-to/performance_guidelines.html)
- [rocprofv3 用户指南](https://rocm.docs.amd.com/projects/rocprofiler/en/latest/)

### 性能优化技术（通用）
- 合并内存访问（Coalesced Access）
- 共享内存使用
- Occupancy 优化
- 指令级并行（ILP）

## 🚀 快速开始

```bash
# 1. 从计时基础开始
cd 01_timing_basics
hipcc -O3 -o timing_demo timing_demo.cpp
./timing_demo

# 2. 查看结果，理解性能指标
# 3. 阅读 README.md 学习概念
# 4. 修改代码尝试优化
# 5. 继续下一个主题
```

## 💬 与 DirectX 性能分析对比

您有 DirectX 驱动经验，这个对比可能有帮助：

| DirectX | HIP/ROCm |
|---------|----------|
| **PIX** | rocprofv3 / Omniperf |
| **GPU View** | roctracer |
| **Profiler** | hipEvent |
| **Nsight** | rocm-smi |

核心概念类似，工具不同！

## 📝 注意事项

1. **使用 -O3 编译** - 性能测试时必须优化
2. **Warmup run** - 第一次运行可能包含初始化开销
3. **多次测量** - 取平均值减少误差
4. **关闭其他 GPU 程序** - 避免干扰测量

---

开始学习：`cd 01_timing_basics && cat README.md` 🚀
