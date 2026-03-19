# 03_memory_bandwidth - GPU 内存带宽测试

## 📚 学习目标

理解 GPU 内存层次结构，学习如何测量和优化内存带宽性能。

## 🎯 核心概念

### 1. GPU 内存层次结构

```
┌─────────────────────────────────────────────────────────────┐
│                        GPU                                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Compute Unit (CU)                                    │  │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐      │  │
│  │  │  Registers │  │  Registers │  │  Registers │ ...  │  │
│  │  │  (最快)     │  │            │  │            │      │  │
│  │  └────────────┘  └────────────┘  └────────────┘      │  │
│  │           ↓                                           │  │
│  │  ┌────────────────────────────────────────────┐      │  │
│  │  │          LDS (Local Data Share)             │      │  │
│  │  │          共享内存，64KB/CU，~20 TB/s         │      │  │
│  │  └────────────────────────────────────────────┘      │  │
│  └──────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │                 L2 Cache (共享)                       │  │
│  │              几 MB，~4-8 TB/s                         │  │
│  └──────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Global Memory (VRAM/HBM)                 │  │
│  │        几 GB ~ 几十 GB，~500-3000 GB/s               │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                          ↓  PCIe/Infinity Fabric
┌─────────────────────────────────────────────────────────────┐
│                    Host Memory (RAM)                         │
│                  ~30-50 GB/s (DDR5)                         │
└─────────────────────────────────────────────────────────────┘
```

### 2. 内存类型对比

| 内存类型 | 大小 | 带宽 | 延迟 | 访问范围 |
|---------|------|------|------|---------|
| **Registers** | ~256KB/CU | ~100 TB/s | 0 cycles | 单线程 |
| **LDS (共享内存)** | 64KB/CU | ~20 TB/s | ~20 cycles | Workgroup |
| **L2 Cache** | 几 MB | ~4-8 TB/s | ~100 cycles | 全局 |
| **Global Memory** | 几 GB | ~500-3000 GB/s | ~500 cycles | 全局 |
| **Host Memory** | 几十 GB | ~15 GB/s (PCIe) | ~10000 cycles | CPU↔GPU |

### 3. 内存访问模式

**合并访问 (Coalesced Access)** - 最优：
```cpp
// 连续访问 - GPU 可以合并成一次内存事务
data[threadIdx.x]      // 线程 0 访问 data[0]
data[threadIdx.x + 1]  // 线程 1 访问 data[1] ...
```

**跨步访问 (Strided Access)** - 较差：
```cpp
// 跨步访问 - 需要多次内存事务
data[threadIdx.x * stride]  // 如果 stride > 1，性能下降
```

**随机访问 (Random Access)** - 最差：
```cpp
// 随机访问 - 每个线程可能需要单独的内存事务
data[random_index[threadIdx.x]]
```

## 🔧 示例程序

### bandwidth_test.cpp

测试不同内存操作的带宽：
1. **Host-to-Device (H2D)** - 从 CPU 内存复制到 GPU
2. **Device-to-Host (D2H)** - 从 GPU 复制到 CPU 内存
3. **Device-to-Device (D2D)** - GPU 内部复制
4. **Kernel Read** - Kernel 读取全局内存
5. **Kernel Write** - Kernel 写入全局内存
6. **Kernel Read+Write** - Kernel 读写（复制）

### coalescing_test.cpp

对比不同访问模式的性能：
1. **合并访问** - 最优模式
2. **跨步访问** - stride = 2, 4, 8, ...
3. **随机访问** - 最差模式

## 📊 编译和运行

```bash
cd 03_memory_bandwidth

# 使用 Makefile
make        # 编译全部
make run    # 编译并运行
make clean  # 清理

# 或手动编译
hipcc -O3 -o bandwidth_test bandwidth_test.cpp
hipcc -O3 -o coalescing_test coalescing_test.cpp
```

## 📈 实测结果 (RX 9070 XT)

### bandwidth_test 输出

```
========================================
    GPU 内存带宽测试
========================================
GPU: AMD Radeon RX 9070 XT
显存大小: 15.92 GB
内存时钟: 1258 MHz
内存位宽: 256-bit
========================================

=== 测试 1: Host-to-Device (H2D) 带宽 ===
  数据大小: 256.00 MB
  平均时间: 9.404 ms
  带宽: 28.54 GB/s

=== 测试 2: Device-to-Host (D2H) 带宽 ===
  数据大小: 256.00 MB
  平均时间: 9.598 ms
  带宽: 27.97 GB/s

=== 测试 3: Device-to-Device (D2D) 带宽 ===
  数据大小: 256.00 MB
  平均时间: 1.073 ms
  有效带宽: 500.43 GB/s (读 + 写)

=== 测试 4: Kernel 读取带宽 ===
  数据大小: 256.00 MB
  平均时间: 0.490 ms
  读取带宽: 548.31 GB/s

=== 测试 5: Kernel 写入带宽 ===
  数据大小: 256.00 MB
  平均时间: 0.478 ms
  写入带宽: 562.15 GB/s

=== 测试 6: Kernel 复制带宽 (读 + 写) ===
  数据大小: 256.00 MB
  平均时间: 0.996 ms
  有效带宽: 538.84 GB/s (读 + 写)

=== 测试 7: 向量化复制带宽 (float4) ===
  数据大小: 256.00 MB
  平均时间: 0.984 ms
  有效带宽: 545.43 GB/s (读 + 写)
```

### 带宽结果分析

| 操作类型 | 实测带宽 | 说明 |
|---------|---------|------|
| **H2D** | 28.5 GB/s | 受 PCIe 4.0 x16 限制 (~32 GB/s 理论) |
| **D2H** | 28.0 GB/s | PCIe 双向带宽类似 |
| **D2D** | 500 GB/s | GPU 内部显存带宽 |
| **Kernel Read** | 548 GB/s | 全局内存读取 |
| **Kernel Write** | 562 GB/s | 全局内存写入 |
| **Kernel Copy** | 539 GB/s | 读+写，~85% 理论带宽利用率 |

### coalescing_test 输出

```
========================================
    内存合并访问测试
========================================
GPU: AMD Radeon RX 9070 XT
数组大小: 256 MB
========================================

=== 测试 1: 合并访问 (Coalesced) ===
  访问模式: 连续 (stride = 1)
  平均时间: 1.026 ms
  有效带宽: 523.31 GB/s

=== 测试: 跨步访问 (Stride = 2) ===
  有效带宽: 139.05 GB/s

=== 测试: 跨步访问 (Stride = 4) ===
  有效带宽: 72.81 GB/s

=== 测试: 跨步访问 (Stride = 8) ===
  有效带宽: 38.54 GB/s

=== 测试: 跨步访问 (Stride = 16) ===
  有效带宽: 39.20 GB/s

=== 测试: 跨步访问 (Stride = 32) ===
  有效带宽: 24.01 GB/s

=== 测试: 随机访问 (Random) ===
  有效带宽: 23.61 GB/s
```

### 访问模式对带宽的影响

| 访问模式 | 有效带宽 | 相对性能 |
|---------|---------|---------|
| **合并访问 (stride=1)** | 523 GB/s | 100% ✅ |
| stride=2 | 139 GB/s | 27% |
| stride=4 | 73 GB/s | 14% |
| stride=8 | 39 GB/s | 7% |
| stride=32 | 24 GB/s | 5% |
| **随机访问** | 24 GB/s | **4.5%** ❌ |

**⚠️ 关键结论**: 跨步或随机访问可导致带宽下降 **95%**！这是 GPU 优化中最重要的概念之一。

## 📊 理论带宽 vs 实际带宽

### 常见 GPU 理论带宽

| GPU 型号 | 显存类型 | 理论带宽 |
|---------|---------|---------|
| RX 7600 | GDDR6 | 288 GB/s |
| RX 7900 XTX | GDDR6 | 960 GB/s |
| RX 9070 XT | GDDR6 | ~650 GB/s |
| MI210 | HBM2e | 1.6 TB/s |
| MI300X | HBM3 | 5.3 TB/s |

### 实际带宽

实际带宽通常为理论值的 **70-90%**，取决于：
- 访问模式（合并 vs 随机）
- 数据对齐
- Cache 命中率
- 同时运行的线程数

## 💡 优化建议

### 1. 确保合并访问
```cpp
// ✅ 好：连续访问
int idx = blockIdx.x * blockDim.x + threadIdx.x;
output[idx] = input[idx];

// ❌ 差：跨步访问
output[idx * 2] = input[idx * 2];
```

### 2. 使用向量类型提高吞吐量
```cpp
// 使用 float4 一次读取 16 字节
float4 val = reinterpret_cast<float4*>(input)[idx];
reinterpret_cast<float4*>(output)[idx] = val;
```

### 3. 利用 LDS 减少全局内存访问
```cpp
__shared__ float lds[256];  // 每个 workgroup 共享
lds[threadIdx.x] = global_data[idx];
__syncthreads();
// 多次使用 lds[...] 而不是重复读 global_data
```

### 4. 数据对齐
```cpp
// 确保数据按 128 字节（cache line）对齐
hipMalloc(&ptr, size);  // HIP 默认 256 字节对齐
```

## 🔗 DirectX 对比

| HIP 概念 | DirectX 等价物 |
|---------|---------------|
| Global Memory | Default Heap (D3D12_HEAP_TYPE_DEFAULT) |
| LDS (共享内存) | Group Shared Memory (groupshared) |
| hipMemcpy H2D | UpdateSubresource / CopyTextureRegion |
| hipMemcpy D2H | ReadbackHeap + Map |
| 合并访问 | 顺序纹理采样 / 结构化缓冲区访问 |

## 🎓 练习建议

1. **运行 bandwidth_test** - 记录你 GPU 的实际带宽
2. **对比理论值** - 计算利用率百分比
3. **修改 coalescing_test** - 尝试不同的 stride 值
4. **使用 rocprofv3 分析** - 查看内存事务数量

```bash
# 使用 rocprofv3 分析内存性能
rocprofv3 --runtime-trace -f csv -- ./bandwidth_test
```

## 🔗 下一步

学习 GPU 占用率优化 → [04_occupancy](../04_occupancy/)
