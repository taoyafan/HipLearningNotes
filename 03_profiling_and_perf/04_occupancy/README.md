# 04_occupancy - GPU 占用率优化

## 📚 学习目标

理解 GPU 占用率（Occupancy）的概念，学习如何分析和优化 kernel 的占用率。

## 🎯 核心概念

### 1. 什么是占用率 (Occupancy)？

**占用率** = 实际活跃的 wavefronts / CU 能支持的最大 wavefronts

```
Occupancy = Active Wavefronts per CU / Max Wavefronts per CU
```

例如：如果一个 CU 最多支持 32 个 wavefronts，但你的 kernel 只能运行 16 个：
- 占用率 = 16/32 = **50%**

### 2. 为什么占用率重要？

GPU 通过 **延迟隐藏 (Latency Hiding)** 来保持高性能：

```
┌─────────────────────────────────────────────────────────────┐
│  Wavefront 0: [计算] [等待内存] [计算] [等待内存] ...       │
│  Wavefront 1:        [计算]    [等待]  [计算]    ...       │
│  Wavefront 2:               [计算]   [等待]  [计算] ...    │
│  ...                                                        │
│  当一个 wavefront 等待内存时，GPU 切换到另一个 wavefront    │
└─────────────────────────────────────────────────────────────┘
```

- **高占用率** → 更多 wavefronts 可切换 → 更好的延迟隐藏 → 更高性能
- **低占用率** → GPU 闲置等待 → 性能浪费

### 3. 限制占用率的三大因素

| 资源 | 说明 | 限制方式 |
|-----|------|---------|
| **寄存器 (VGPR/SGPR)** | 每个线程使用的寄存器数 | 寄存器用太多 → 能运行的 wavefronts 减少 |
| **共享内存 (LDS)** | 每个 workgroup 使用的共享内存 | LDS 用太多 → 能运行的 workgroups 减少 |
| **Workgroup 大小** | block 中的线程数 | 太大或太小都可能降低占用率 |

### 4. AMD GPU 资源限制（以 RDNA 架构为例）

| 资源 | 每 CU 限制 | 说明 |
|-----|-----------|------|
| **Max Wavefronts** | 32 | 最多同时运行的 wavefronts |
| **Max Workgroups** | 16-32 | 取决于具体 GPU |
| **VGPR** | 1536 (RDNA3/4) | 向量寄存器总数 |
| **SGPR** | 800 | 标量寄存器 |
| **LDS** | 64 KB | 共享内存 |
| **Wavefront Size** | 32 (RDNA) / 64 (CDNA) | 每个 wavefront 的线程数 |

### 5. 占用率计算示例

假设 RDNA4 GPU，wavefront size = 32：

**情况 1：寄存器限制**
```
Kernel 使用 96 VGPRs/线程
每 CU 有 1536 VGPRs
每个 wavefront 需要: 96 × 32 = 3072 VGPRs ❌ 超过限制！

实际分配: 每 wavefront 最多用 48 VGPRs (1536/32)
→ Kernel 需要更多寄存器，可能 spill 到内存
```

**情况 2：LDS 限制**
```
Workgroup 使用 32 KB LDS
每 CU 有 64 KB LDS
最多支持: 64KB / 32KB = 2 个 workgroups
每个 workgroup 有 256 线程 = 8 wavefronts
→ 最多 16 个 wavefronts → 占用率 = 16/32 = 50%
```

## 🔧 示例程序

### occupancy_test.cpp

演示不同配置对占用率的影响：
1. **不同 block size** - 64, 128, 256, 512, 1024
2. **不同寄存器使用量** - 通过代码复杂度控制
3. **不同 LDS 使用量** - 通过 `__shared__` 数组大小控制

### occupancy_calculator.cpp

使用 HIP API 计算理论占用率：
- `hipOccupancyMaxActiveBlocksPerMultiprocessor()`
- `hipOccupancyMaxPotentialBlockSize()`

## 📊 编译和运行

```bash
cd 04_occupancy

# 使用 Makefile
make        # 编译全部
make run    # 编译并运行
make clean  # 清理

# 或手动编译
hipcc -O3 -o occupancy_test occupancy_test.cpp
```

## 📈 实测结果 (RX 9070 XT)

### GPU 资源信息

```
GPU: AMD Radeon RX 9070 XT
Compute Units: 32
Max Threads/Block: 1024
Max Threads/CU: 2048
Registers/Block: 196608
Shared Memory/Block: 64 KB
Warp Size: 32
```

### 测试 1: Block Size 对性能的影响

| Block Size | 时间 (ms) | 带宽 (GB/s) |
|-----------|----------|-------------|
| 64 | 1.051 | 510.7 |
| **128** | **0.909** | **590.9** ✅ |
| 256 | 0.924 | 581.1 |
| 512 | 0.945 | 568.4 |
| 1024 | 0.943 | 569.4 |

**发现**: Block Size = 128 性能最佳，并非越大越好！

### 测试 2: 寄存器使用量对性能的影响

| Kernel | 时间 (ms) | 带宽 (GB/s) |
|--------|----------|-------------|
| Low Registers | 0.908 | 591.2 |
| Medium Registers | 0.935 | 574.5 |
| High Registers | 0.947 | 566.7 |

**发现**: 寄存器使用增加，性能略有下降（约 4%）

### 测试 3: LDS 使用量对性能的影响 ⚠️

| LDS 大小 | 时间 (ms) | 带宽 (GB/s) | 相对性能 |
|---------|----------|-------------|---------|
| 0 KB | 0.900 | 596.4 | 100% |
| 1 KB | 0.946 | 567.5 | 95% |
| 8 KB | 1.337 | 401.6 | 67% |
| **32 KB** | **3.849** | **139.5** | **23%** ❌ |

**关键发现**: LDS 使用 32KB 时，占用率大幅下降！
- 每 CU 64KB LDS → 最多 2 个 workgroups
- 性能下降 77%

### 测试 4: HIP Occupancy API 与占用率计算

```
kernel_low_registers:
  Block Size=256 时，每 CU 最多 8 blocks

kernel_large_lds (32KB LDS):
  Block Size=256, LDS=32KB 时，每 CU 最多 1 block  ← 占用率大幅降低
```

### 如何计算占用率

**公式**:
```
Occupancy = (Active Wavefronts per CU) / (Max Wavefronts per CU)

Active Wavefronts = Blocks_per_CU × Threads_per_Block / Wavefront_Size
```

**RX 9070 XT 计算示例**:
```
Max Threads/CU = 2048
Wavefront Size = 32
Max Wavefronts/CU = 2048 / 32 = 64

kernel_low_registers (Block=256, 8 blocks/CU):
  Active Wavefronts = 8 × 256 / 32 = 64
  Occupancy = 64 / 64 = 100% ✅

kernel_large_lds (Block=256, 1 block/CU):
  Active Wavefronts = 1 × 256 / 32 = 8
  Occupancy = 8 / 64 = 12.5% ❌
```

**使用 HIP API 获取占用率**:
```cpp
int numBlocks;
hipOccupancyMaxActiveBlocksPerMultiprocessor(
    &numBlocks,           // 输出：每 CU 最多多少 blocks
    myKernel,             // kernel 函数
    blockSize,            // block 大小
    dynamicSharedMem);    // 动态共享内存大小

// 计算占用率
int wavefrontsPerBlock = blockSize / prop.warpSize;
int activeWavefronts = numBlocks * wavefrontsPerBlock;
int maxWavefronts = prop.maxThreadsPerMultiProcessor / prop.warpSize;
float occupancy = (float)activeWavefronts / maxWavefronts * 100.0f;
```

## 📉 使用 rocprofv3 分析占用率

```bash
# 查看 kernel 的资源使用
rocprofv3 --kernel-trace -f csv -- ./occupancy_test

# kernel_trace.csv 中的关键字段:
# - VGPR_Count: 向量寄存器数
# - SGPR_Count: 标量寄存器数
# - LDS_Size: 共享内存大小
# - Workgroup_Size: block 大小
```

## 💡 优化策略

### 1. 调整 Block Size

```cpp
// 使用 HIP API 自动选择最佳 block size
int blockSize;
int minGridSize;
hipOccupancyMaxPotentialBlockSize(&minGridSize, &blockSize,
                                   myKernel, 0, 0);
```

### 2. 减少寄存器使用

```cpp
// 方法 1：使用 __launch_bounds__ 提示编译器
__global__ __launch_bounds__(256, 4)  // 最多 256 线程/block, 至少 4 blocks/CU
void myKernel(...) { ... }

// 方法 2：减少局部变量
// 方法 3：使用共享内存代替私有数组
```

### 3. 减少 LDS 使用

```cpp
// 方法 1：减小共享内存数组大小
__shared__ float cache[128];  // 而不是 [1024]

// 方法 2：分块处理，复用同一块 LDS
for (int tile = 0; tile < numTiles; tile++) {
    // 加载 tile 到 LDS
    // 处理
    __syncthreads();
}
```

### 4. 占用率 vs 性能的权衡

⚠️ **高占用率 ≠ 一定高性能**

| 场景 | 最佳占用率 | 原因 |
|-----|-----------|------|
| **Memory Bound** | 较高（50-100%） | 需要更多 wavefronts 隐藏内存延迟 |
| **Compute Bound** | 可以较低（25-50%） | 计算密集，不需要太多切换 |
| **使用大量 LDS** | 可以较低 | LDS 带宽高，低占用率也够用 |

## 🔗 DirectX 对比

| HIP/CUDA 概念 | DirectX 等价物 |
|--------------|---------------|
| Occupancy | Wave Occupancy (PIX) |
| VGPR/SGPR | GPR Usage |
| LDS | Group Shared Memory |
| `__launch_bounds__` | `[numthreads(X,Y,Z)]` 影响寄存器分配 |
| Wavefront | Wave |

## 🎓 练习建议

1. **运行 occupancy_test** - 观察不同配置的性能差异
2. **使用 rocprofv3 分析** - 查看 VGPR/SGPR/LDS 使用量
3. **尝试 `__launch_bounds__`** - 观察对寄存器分配的影响
4. **修改 LDS 大小** - 观察对占用率和性能的影响

## 🔗 下一步

学习 GPGPU 特有算法 → [04_compute_algorithms](../../04_compute_algorithms/)
