# 01_timing_basics - HIP Kernel 计时基础

## 📚 学习目标

学习如何测量 GPU kernel 的执行时间和性能指标。

## 🎯 核心概念

### 1. hipEvent 计时

```cpp
hipEvent_t start, stop;
hipEventCreate(&start);
hipEventCreate(&stop);

hipEventRecord(start, 0);        // 记录开始时间
kernel<<<...>>>(...);             // 执行 kernel
hipEventRecord(stop, 0);          // 记录结束时间

hipEventSynchronize(stop);        // 等待完成
float time_ms;
hipEventElapsedTime(&time_ms, start, stop);  // 获取耗时（毫秒）
```

### 2. CPU 时间 vs GPU 时间

| 计时方法 | 测量内容 | 适用场景 |
|---------|---------|---------|
| **CPU wall time** | Kernel 启动 + 执行 | 整体性能评估 |
| **hipEvent** | 只计算 GPU 执行时间 | Kernel 优化 |

### 3. 关键性能指标

**带宽（Bandwidth）**：
```
带宽 (GB/s) = 数据量 (GB) / 时间 (s)
```

**吞吐量（Throughput）**：
```
吞吐量 (GElements/s) = 元素数量 / 时间
```

**GFLOPS**：
```
GFLOPS = (浮点运算次数 / 10^9) / 时间 (s)
```

## 🔧 编译和运行

```bash
cd 01_timing_basics

# 编译（-O3 优化）
hipcc -O3 -o timing_demo timing_demo.cpp

# 运行
./timing_demo
```

## 📊 预期输出

```
========================================
    HIP Kernel 计时示例
========================================
使用 GPU: AMD Radeon RX 7600
计算能力: 11.0
全局内存: 8.00 GB

=== 示例 1: 使用 hipEvent 测量 GPU 时间 ===
Host to Device 传输时间: 5.234 ms
  数据量: 76.29 MB
  带宽: 14.58 GB/s

Kernel 执行时间: 0.823 ms
  处理元素: 10000000
  吞吐量: 12.15 GElements/s

Device to Host 传输时间: 4.912 ms
  带宽: 15.53 GB/s

总时间: 10.969 ms
结果验证: ✅ 正确

=== 示例 2: CPU 时间 vs GPU 时间 ===
CPU 计时（wall time）: 1.245 ms
GPU 计时（hipEvent）: 0.412 ms
差异: 0.833 ms

💡 CPU 时间包含 kernel 启动开销，GPU 时间只计算实际执行时间

=== 示例 3: 计算 GFLOPS ===
Kernel 执行时间: 12.456 ms
总浮点运算次数: 200000000 (0.20 billion)
性能: 16.06 GFLOPS

GPU: AMD Radeon RX 7600
理论峰值性能（参考）: 取决于具体 GPU 型号
```

## 💡 关键要点

1. **hipEvent 是标准方法** - 用于精确测量 GPU 时间
2. **记得 synchronize** - `hipEventSynchronize()` 确保测量完成
3. **CPU 时间 > GPU 时间** - 包含 kernel 启动开销
4. **多次测量取平均** - 减少误差（生产代码中）
5. **Warmup run** - 第一次运行可能较慢（JIT 编译等）

## 🎓 练习建议

1. **修改数据规模** - 观察时间如何变化
2. **调整 block size** - 看性能影响
3. **添加 warmup** - 运行 kernel 几次再测量
4. **计算实际带宽** - 与理论带宽对比
5. **测量不同 kernel** - 比较性能差异

## 🔗 下一步

学习使用 rocprof 进行更详细的性能分析 → [02_rocprof_basics](../02_rocprof_basics/)
