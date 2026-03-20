# HIP GPU 学习笔记总结

本文档是 HIP GPU 学习过程中的核心笔记和发现总结。

> **文档组织说明**：本笔记作为索引和关键发现总结，详细内容请参考各专题文档。

---

## 📚 文档索引

### 核心参考文档

| 文档 | 用途 | 适合场景 |
|------|------|---------|
| [ROCGDB_CHEATSHEET.md](ROCGDB_CHEATSHEET.md) | rocgdb 命令速查 | 调试时快速查阅 |
| [CheckDisam.md](CheckDisam.md) | GPU 汇编和反汇编对比 | 理解优化等级差异 |
| [MODIFY_KERNEL_ISA.md](MODIFY_KERNEL_ISA.md) | 修改 GPU ISA 并调试 | 底层指令修改和验证 |

### 示例代码

| 示例 | 位置 | 说明 |
|------|------|------|
| 向量加法 | [01_vector_add/](../01_vector_add/) | 基础工作流程 |
| 调试示例 | [02_debug_example/](../02_debug_example/) | rocgdb 实战练习 |

---

## 🎯 关键发现和经验

### 1. rocgdb 调试核心概念

#### Thread vs Wavefront vs Lane

> 详细说明：[ROCGDB_CHEATSHEET.md - 关键概念](ROCGDB_CHEATSHEET.md#关键概念)

| 概念 | 说明 | 调试命令 |
|------|------|---------|
| **CPU Thread** | 操作系统线程 | `info threads` (Thread 1-6) |
| **GPU Wavefront** | 硬件执行单元（32/64个线程组） | `info threads` (Thread 7+) |
| **Lane** | Wavefront 内的单个线程 | `info lanes`, `lane N` |

**关键理解**：
- `info threads` 显示的 Thread 7+ 是 GPU **Wavefronts**，不是单个线程
- 每个 Wavefront 包含 32 或 64 个 lanes（取决于 GPU 架构）
- 使用 `lane N` 在同一 wavefront 内切换线程

#### Wavefront 编号解析

```
AMDGPU Wave 1:1:1:29/0 (3,0,0)[128,0,0]
            │  │ │ │  │   │       │
            │  │ │ │  │   │       └─ threadIdx (128,0,0)
            │  │ │ │  │   └───────── blockIdx (3,0,0)
            │  │ │ │  └───────────── lane 编号（wavefront 内）
            │  │ │ └──────────────── wavefront ID
            │  │ └─────────────────── dispatch ID
            │  └────────────────────── queue ID
            └───────────────────────── agent ID (GPU 设备)
```

**计算全局索引**：
```
idx = blockIdx.x * blockDim.x + threadIdx.x
    = 3 * 256 + 128 = 896
```

---

### 2. GDB 的 `x` 命令（Examine Memory）

> 详细说明：[ROCGDB_CHEATSHEET.md - 查看内存](ROCGDB_CHEATSHEET.md#查看内存x-命令)

**基本格式**: `x/[数量][格式][大小] [地址]`

**最常用的组合**：
```gdb
x/20i $pc           # 查看 20 条汇编指令（最重要！）
x/10f d_A           # 查看 10 个浮点数
x/10xw addr         # 查看 10 个 word（十六进制）
```

**关键技巧**：
- 按回车重复上一个 `x` 命令（继续显示）
- 结合 `print` 使用：`print &A[100]` → `x/10f $1`
- 同一内存不同格式查看：`x/4xw` vs `x/4f` vs `x/16xb`

---

### 3. 局部变量 vs 数组的调试差异 ⚠️

> 详细说明：[ROCGDB_CHEATSHEET.md - 查看内存](ROCGDB_CHEATSHEET.md#查看内存x-命令)

**重要发现**：不能对局部变量使用 `@` 操作符！

```gdb
# ✗ 错误：尝试查看多个 lanes 的局部变量
(gdb) print idx@2
$7 = {898, -249560896}    # 第二个值是垃圾数据！

# ✓ 正确：只能查看单个 lane 的值
(gdb) lane 0
(gdb) print idx           # 896
(gdb) lane 1
(gdb) print idx           # 897
```

**原因**：
- 局部变量存储在**寄存器或私有栈**中
- 不同 lane 的局部变量**不在连续内存**
- `@` 操作符只能用于连续存储的数组

**内存访问对比**：

| 数据类型 | 存储位置 | 是否连续 | 能用 `@` 吗 |
|---------|---------|---------|-----------|
| **全局数组** | GPU VRAM | ✓ | ✓ `print A[0]@1024` |
| **局部变量** | 寄存器/私有栈 | ✗ | ✗ 只能单个查看 |
| **共享内存** | LDS | ✓ | ✓ `print shared[0]@256` |

---

### 4. GPU 汇编代码的优化等级差异 🔥

> 详细说明：[CheckDisam.md - 汇编和反汇编对比](CheckDisam.md)

**核心发现**：`rocgdb disassemble` ≈ `hipcc -S -O0` 生成的 .s 文件

| 版本 | 指令数 | 来源 |
|------|--------|------|
| **-O0** (调试) | ~150 条 | rocgdb disassemble / hipcc -S -O0 |
| **-O2/-O3** (优化) | ~31 条 | hipcc --save-temps (默认) |

**同一段代码的差异**：

```cpp
if (idx < N) {  // 简单的边界检查
```

- **-O0 版本**：18 条指令（包含内存加载、调试状态保存、等待指令）
- **-O2 版本**：3 条指令（直接比较和跳转）

**关键理解**：
1. `-O0` 将变量存在**内存**中 → 需要 `flat_load`
2. `-O2` 将变量存在**寄存器**中 → 无需加载
3. `-O0` 包含**调试代码**（保存状态、栈操作）
4. rocgdb 显示的是**实际执行的代码**（反映编译选项）

**使用建议**：
- **学习 GPU 指令** → 查看 `-O2` 的 .s 文件（代码简洁）
- **运行时调试** → 使用 rocgdb（查看实际执行代码）
- **性能分析** → 查看 `-O3` 的 .s 文件（最优化版本）

> 完整对比分析：[CheckDisam.md](CheckDisam.md)

---

### 5. GDB 值历史（Value History）技巧

**GDB 自动保存每次 print 的结果**：

```gdb
(gdb) print idx
$3 = 896           # 第 3 个值

(gdb) lane 1
(gdb) print idx
$4 = 897           # 第 4 个值

# 后续可以引用
(gdb) print $3     # 896
(gdb) print $4     # 897
(gdb) print $4-$3  # 1（验证连续性）
(gdb) print $      # 最后一个值（$4）
(gdb) print $$     # 倒数第二个值（$3）
```

**应用场景**：
- 对比不同 lanes 的值
- 计算差值和验证
- 避免重复输入复杂表达式

---

### 6. rocgdb 命令验证结果 ✅

**可用的 GPU 调试命令**（已验证）：

```gdb
info agents      # ✅ 查看 GPU 设备
info dispatches  # ✅ 查看 GPU 调度
info lanes       # ✅ 查看当前 lane 详情
info queues      # ✅ 查看命令队列
lane N           # ✅ 切换到第 N 个 lane
```

**不可用的命令**（不要使用）：

```gdb
info rocm threads         # ❌ Undefined info command
rocm thread (0,0):(5,0)   # ❌ 不存在
```

> 完整命令参考：[ROCGDB_CHEATSHEET.md](ROCGDB_CHEATSHEET.md)

---

## 💡 最佳实践

### 调试策略组合

| 场景 | 推荐工具 | 理由 |
|------|---------|------|
| **快速查看大量数据** | rocgdb + 数组查看 | `print A[0]@1024` 一次看所有 |
| **精确调试特定线程** | rocgdb + lane 切换 | `lane N` + `print idx` |
| **快速定位问题** | kernel 内 printf | 最直观，无需调试器 |
| **保存调试数据** | debug 数组 + rocgdb | 将变量保存到数组再查看 |

### Do's ✓

1. **使用 `disassemble /m`** - 混合源代码和汇编（最有用！）
2. **使用 `x/20i $pc`** - 查看即将执行的指令
3. **使用 `lane N` 切换线程** - 在同一 wavefront 内
4. **使用 `print array[0]@N`** - 查看连续数据
5. **结合 printf 和 rocgdb** - 互补使用

### Don'ts ✗

1. **不要对局部变量使用 `@`** - 只能用于数组
2. **不要混淆 thread 和 lane** - thread 切换 wavefront，lane 切换 lane
3. **不要期望 GPU 调试像 CPU** - rocgdb 功能有限
4. **不要忽略优化等级** - -O0 和 -O3 代码完全不同

---

## 🔧 实用工具链

### 编译和查看汇编

```bash
# 生成优化的汇编（学习用）
hipcc -S -O2 -o kernel.s kernel.cpp

# 生成调试版汇编（对比用）
hipcc -S -O0 -o kernel_O0.s kernel.cpp

# 生成所有中间文件
hipcc --save-temps -o program program.cpp
```

### rocgdb 快速启动

```bash
# 编译带调试符号
hipcc -g -O0 -o program program.cpp

# 启动调试
rocgdb ./program

# 常用命令序列
(gdb) break kernel_name
(gdb) run
(gdb) info threads
(gdb) info lanes
(gdb) x/20i $pc
(gdb) disassemble /m
```


---

## 7. GPU 性能测量基础（2026-03-11）

### hipEvent API（编程方式计时）

> 代码示例：[03_profiling_and_perf/01_timing_basics/timing_demo.cpp](../03_profiling_and_perf/01_timing_basics/timing_demo.cpp)

**核心 API**：

```cpp
hipEvent_t start, stop;
hipEventCreate(&start);
hipEventCreate(&stop);

// 在 GPU 命令流中插入时间戳
hipEventRecord(start, 0);        // 0 = 默认 stream
kernel<<<...>>>(...);
hipEventRecord(stop, 0);

// 等待 GPU 执行到这个时间点
hipEventSynchronize(stop);       // 阻塞 CPU 直到 GPU 完成

// 获取两个 event 之间的时间差（毫秒）
float time_ms;
hipEventElapsedTime(&time_ms, start, stop);
```

### Stream（流）概念

**`hipEventRecord(event, stream)` 中的 stream 参数**：

| 值 | 含义 | 用途 |
|----|------|------|
| `0` / `NULL` | 默认流（default stream） | 简单程序用这个就够了 |
| 自定义流 | `hipStreamCreate(&stream)` | 并发执行多个操作 |

**与 DirectX 12 类比**：

| HIP | DirectX 12 | 说明 |
|-----|-----------|------|
| **Stream** | Command Queue | 命令流水线 |
| `hipEventRecord(event, stream)` | `queue->Signal(fence, value)` | 插入标记 |
| `hipEventSynchronize(event)` | `fence->Wait(value)` | CPU 等待 GPU |
| 多 stream 并发 | 多 queue 并发（Copy/Compute） | 隐藏延迟 |

**Stream 的作用**：
1. **并发执行** - 不同 stream 可以同时运行不同的 kernel
2. **异步操作** - 数据传输和计算可以重叠
3. **性能优化** - 隐藏内存延迟（类似 DX12 的异步拷贝队列）

### hipEventSynchronize 的必要性

**为什么必须调用 `hipEventSynchronize(stop)`？**

1. **GPU 是异步的**：
   - `kernel<<<>>>()` 启动后 CPU **立即返回**，不等 GPU
   - GPU 在后台执行，CPU 继续运行

2. **Event 只是标记**：
   - `hipEventRecord(stop, 0)` 只在 GPU 命令流中**插入标记**
   - CPU 不知道 GPU 什么时候执行到这个标记

3. **必须等待才能读取时间**：
   - `hipEventElapsedTime()` 需要两个 event 都已被 GPU 执行
   - 如果 GPU 还没到达 `stop`，时间测量就不准确

**不同同步方式对比**：

```cpp
// 方式 1：等待整个设备所有工作完成（粗暴）
hipDeviceSynchronize();

// 方式 2：等待特定 stream 完成（中等）
hipStreamSynchronize(stream1);

// 方式 3：等待到达某个 event（最精确）⭐
hipEventSynchronize(event);  // 只等到这个标记点
```

**计时场景推荐**：使用 `hipEventSynchronize(event)` 最精确。

### hipEvent vs rocprof 工具对比 ⚠️

**重要区别**：`hipEvent` 和 `rocprof` 是**完全不同的东西**！

| | **hipEvent** | **rocprof** |
|---|-------------|-------------|
| **类型** | 编程 API（代码中调用） | 外部命令行工具 |
| **用法** | 在源代码中插入计时代码 | 在程序外部运行分析 |
| **需要修改代码** | ✅ 是 | ❌ 否 |
| **测量内容** | Kernel 执行时间、数据传输时间 | 硬件计数器、内存带宽、占用率、指令吞吐量 |
| **粒度** | 你手动选择测量什么 | 自动分析整个程序 |
| **性能开销** | 很小（微秒级） | 较大（收集详细数据） |
| **与 DirectX 类比** | `QueryPerformanceCounter`（代码中） | **PIX**（外部工具） |

**使用场景**：

```cpp
// hipEvent（编程方式）- 需要修改代码
hipEvent_t start, stop;
hipEventCreate(&start);
hipEventRecord(start, 0);
kernel<<<...>>>(...);
hipEventRecord(stop, 0);
hipEventSynchronize(stop);
float time_ms;
hipEventElapsedTime(&time_ms, start, stop);  // 得到时间
```

```bash
# rocprof（外部工具）- 不需要修改代码
rocprof --stats ./timing_demo
rocprof --hsa-trace ./timing_demo  # 收集硬件计数器
```

**学习路径**：
1. **现在（01_timing_basics）** - 学习 `hipEvent` 手动计时
2. **下一步（02_rocprof_basics）** - 学习 `rocprof` 工具深入分析

**实际工作流程**：
- 先用 `hipEvent` 快速测量 kernel 时间
- 发现性能问题时，用 `rocprof` 深入分析硬件瓶颈
- 类似 DirectX：先用 `QueryPerformanceCounter` 快速测试，遇到问题再开 **PIX** 分析

---

## 8. rocprofv3 性能分析工具（2026-03-13）

> 代码示例：[03_profiling_and_perf/02_rocprof_basics/](../03_profiling_and_perf/02_rocprof_basics/)
> 详细文档：[02_rocprof_basics/README.md](../03_profiling_and_perf/02_rocprof_basics/README.md)

### rocprof 版本选择

**rocprof = ROCm Profiler（性能分析器）**

| GPU 架构 | 使用工具 |
|---------|---------|
| **RDNA4 (gfx1201) / MI35x 及更新** | `rocprofv3` |
| RDNA3 / CDNA3 | `rocprofv2` |
| 更旧架构 | `rocprof` (v1) |

### 核心命令

**重要**：rocprofv3 使用 `--` 分隔工具选项和应用程序

```bash
# 运行时追踪（推荐入门）
rocprofv3 --runtime-trace -f csv -- ./app

# 系统级追踪（包含 HSA 底层 API）
rocprofv3 --sys-trace -f csv -- ./app

# 仅 kernel 追踪
rocprofv3 --kernel-trace -f csv -- ./app
```

### 输出文件说明

输出目录：`<hostname>/<pid>/`，文件名格式：`<pid>_<trace_type>.csv`

| 追踪选项 | 生成的文件 | 用途 |
|---------|-----------|------|
| `--runtime-trace` | `kernel_trace.csv` | Kernel 执行时间、VGPR/SGPR、Grid/Block 大小 |
| | `hip_api_trace.csv` | HIP API 调用及耗时 |
| | `memory_copy_trace.csv` | H2D/D2H 传输时间 |
| | `memory_allocation_trace.csv` | 内存分配/释放、Correlation_Id |
| | `agent_info.csv` | GPU 型号、CU 数、LDS 大小等 |
| `--sys-trace` | 以上所有 + `hsa_api_trace.csv` | HSA 底层 API（深度调试用） |

### HIP vs HSA 的关系

```
应用层:     你的代码
              │
              ▼
高层 API:   HIP API  (hipMalloc, hipMemcpy, hipLaunchKernel...)
              │         ↑ 你通常用这个
              ▼
底层 API:   HSA API  (hsa_memory_allocate, hsa_signal_create...)
              │         ↑ HIP 内部调用这个
              ▼
驱动层:     AMDGPU 内核驱动
```

### kernel_trace.csv 关键字段

| 字段 | 说明 |
|------|------|
| `Kernel_Name` | Kernel 函数名（含参数签名） |
| `Start_Timestamp` / `End_Timestamp` | 开始/结束时间戳（ns），相减得 Duration |
| `VGPR_Count` | 向量通用寄存器数量 |
| `SGPR_Count` | 标量通用寄存器数量 |
| `Workgroup_Size_X/Y/Z` | Block 大小 |
| `Grid_Size_X/Y/Z` | Grid 大小（总线程数） |

### memory_allocation_trace.csv 的 Correlation_Id

**Correlation_Id** 将底层内存操作关联到触发它的 HIP API 调用：

```
hip_api_trace.csv:
  Correlation_Id=1  →  hipMalloc(&d_A, size)
  Correlation_Id=4  →  hipMemcpy(d_A, h_A, ...)

memory_allocation_trace.csv:
  Correlation_Id=1  →  Agent 0: 112B (元数据) + Agent 1: 40MB (GPU内存)
  Correlation_Id=4  →  Agent 0: 4MB + 16MB (Staging buffers)
```

**关键发现**：
- `hipMalloc` 除了分配 GPU 内存，还在 CPU 端分配小量元数据
- `hipMemcpy` 会分配 **Staging Buffer**（Pinned Memory）用于 DMA 传输
- 使用双缓冲技术提高传输效率

### 与 DirectX 工具对比

| DirectX | ROCm | 功能 |
|---------|------|------|
| **PIX** | rocprofv3 / Omniperf | GPU 性能分析 |
| **GPU View** | roctracer | API 时间线追踪 |
| **QueryPerformanceCounter** | hipEvent | 代码中计时 |

### RDNA4 (gfx1201) 已知问题 ⚠️

截至 ROCm 7.2，硬件计数器功能（`--pmc`）在 RDNA4 上存在 bug：
```
terminate called after throwing an instance of 'std::out_of_range'
  what():  unordered_map::at
```

**可用功能**：追踪功能（`--runtime-trace`, `--kernel-trace` 等）正常工作
**不可用**：`rocprofv3 --pmc SQ_WAVES -- ./app` 会崩溃

### 快速参考

```bash
# 最常用：运行时追踪
rocprofv3 --runtime-trace -f csv -- ./app

# 指定输出目录
rocprofv3 --runtime-trace -f csv -d ./output -- ./app

# 查看可用计数器
rocprofv3 --list-avail

# 查看帮助
rocprofv3 --help
```

---

## 9. GPU 内存带宽优化（2026-03-16）

> 代码示例：[03_profiling_and_perf/03_memory_bandwidth/](../03_profiling_and_perf/03_memory_bandwidth/)
> 详细文档：[03_memory_bandwidth/README.md](../03_profiling_and_perf/03_memory_bandwidth/README.md)

### GPU 内存层次结构

```
┌─────────────────────────────────────┐
│  Registers（寄存器）~100 TB/s       │  ← 最快，单线程私有
├─────────────────────────────────────┤
│  LDS（共享内存）~20 TB/s, 64KB/CU   │  ← Workgroup 共享
├─────────────────────────────────────┤
│  L2 Cache ~4-8 TB/s                 │  ← 全局共享
├─────────────────────────────────────┤
│  Global Memory ~500-3000 GB/s       │  ← VRAM/HBM
├─────────────────────────────────────┤
│  Host Memory ~15-30 GB/s (PCIe)     │  ← CPU 内存
└─────────────────────────────────────┘
```

### 实测带宽结果 (RX 9070 XT)

| 操作类型 | 实测带宽 | 说明 |
|---------|---------|------|
| **H2D (hipMemcpy)** | 28.5 GB/s | 受 PCIe 4.0 x16 限制 |
| **D2H (hipMemcpy)** | 28.0 GB/s | PCIe 双向类似 |
| **D2D (hipMemcpy)** | 500 GB/s | GPU 内部显存带宽 |
| **Kernel Read** | 548 GB/s | 全局内存读取 |
| **Kernel Write** | 562 GB/s | 全局内存写入 |
| **Kernel Copy** | 539 GB/s | 读+写，~85% 理论带宽 |

### 访问模式对带宽的巨大影响 ⚠️

| 访问模式 | 有效带宽 | 相对性能 |
|---------|---------|---------|
| **合并访问 (stride=1)** | 523 GB/s | **100%** ✅ |
| stride=2 | 139 GB/s | 27% |
| stride=4 | 73 GB/s | 14% |
| stride=8 | 39 GB/s | 7% |
| stride=32 | 24 GB/s | 5% |
| **随机访问** | 24 GB/s | **4.5%** ❌ |

**关键结论**：跨步或随机访问可导致带宽下降 **95%**！

### 合并访问 (Coalesced Access) 原理

**好的访问模式** - 相邻线程访问相邻内存：
```cpp
// ✅ 合并访问 - GPU 可合并成一次内存事务
int idx = blockIdx.x * blockDim.x + threadIdx.x;
output[idx] = input[idx];  // 线程 0→data[0], 线程 1→data[1] ...
```

**差的访问模式** - 跨步访问：
```cpp
// ❌ 跨步访问 - 需要多次内存事务
output[idx * 2] = input[idx * 2];  // stride=2，带宽降 73%
```

### Grid-Stride Loop 模式

处理任意大小数据的标准模式：

```cpp
__global__ void kernel(float* data, size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;  // 总线程数

    for (size_t i = idx; i < n; i += stride) {
        data[i] = ...;  // 每个线程处理多个元素
    }
}
```

**为什么用 `i += stride`**：
- 突破 grid 大小限制（最多 65535 blocks）
- 保持合并访问（相邻线程仍访问相邻内存）
- 同一 kernel 适用于任意数据规模

### Template vs Runtime 参数

```cpp
// Template（编译时常量）- 更优
template <int STRIDE>
__global__ void kernel(...) {
    idx * STRIDE;  // 编译器可优化为位移操作
}

// Runtime 参数 - 稍慢
__global__ void kernel(..., int stride) {
    idx * stride;  // 需要真正的乘法 + 额外寄存器
}
```

### 优化建议

1. **确保合并访问** - 相邻线程访问相邻地址
2. **使用向量类型** - `float4` 一次读取 16 字节
3. **利用 LDS** - 减少重复的全局内存访问
4. **AoS → SoA** - 结构数组 → 数组结构，改善访问模式

### 与 DirectX 对比

| HIP 概念 | DirectX 等价物 |
|---------|---------------|
| Global Memory | D3D12_HEAP_TYPE_DEFAULT |
| LDS (共享内存) | groupshared |
| hipMemcpy H2D | UpdateSubresource / CopyTextureRegion |
| 合并访问 | 顺序纹理采样 / 结构化缓冲区访问 |

---

## 10. Wave Occupancy 计算方法（2026-03-17）

> 代码示例：[03_profiling_and_perf/04_occupancy/](../03_profiling_and_perf/04_occupancy/)

### 什么是 Occupancy

**Occupancy（占用率）** = 每个 CU 实际运行的 Wavefronts / CU 最大支持的 Wavefronts

```
Occupancy = Active Wavefronts / Max Wavefronts × 100%
```

### 核心 API

```cpp
// 获取每 CU 可运行的最大 blocks 数
int numBlocks;
hipOccupancyMaxActiveBlocksPerMultiprocessor(
    &numBlocks,           // 输出：每 CU 最多可运行多少 blocks
    myKernel,             // Kernel 函数指针
    blockSize,            // Block 大小（线程数）
    dynamicSharedMem);    // 动态共享内存大小（字节）

// 自动选择最佳 block size
int blockSize, minGridSize;
hipOccupancyMaxPotentialBlockSize(
    &minGridSize,         // 输出：最小 grid 大小
    &blockSize,           // 输出：推荐的 block 大小
    myKernel,             // Kernel 函数指针
    dynamicSharedMem,     // 动态共享内存
    blockSizeLimit);      // Block 大小上限（0=不限制）
```

### Occupancy 计算步骤

```cpp
// 1. 获取 GPU 属性
hipDeviceProp_t prop;
hipGetDeviceProperties(&prop, 0);

// 2. 计算 CU 最大支持的 wavefronts
int maxWavefrontsPerCU = prop.maxThreadsPerMultiProcessor / prop.warpSize;
// 例如：2048 / 32 = 64 wavefronts

// 3. 调用 API 获取每 CU 可运行的 blocks 数
int blocksPerCU;
hipOccupancyMaxActiveBlocksPerMultiprocessor(&blocksPerCU, kernel, blockSize, 0);

// 4. 计算实际活跃 wavefronts
int wavefrontsPerBlock = blockSize / prop.warpSize;
int activeWavefronts = blocksPerCU * wavefrontsPerBlock;

// 5. 计算占用率
float occupancy = (float)activeWavefronts / maxWavefrontsPerCU * 100.0f;
```

### 计算示例 (RX 9070 XT)

**GPU 参数**：
- `maxThreadsPerMultiProcessor` = 2048
- `warpSize` = 32
- `maxWavefrontsPerCU` = 2048 / 32 = **64**

**Kernel 使用 32KB LDS，blockSize=256**：
```
API 返回: blocksPerCU = 1
wavefrontsPerBlock = 256 / 32 = 8
activeWavefronts = 1 × 8 = 8
occupancy = 8 / 64 = 12.5%
```

**Kernel 无 LDS，blockSize=256**：
```
API 返回: blocksPerCU = 8
wavefrontsPerBlock = 256 / 32 = 8
activeWavefronts = 8 × 8 = 64
occupancy = 64 / 64 = 100%
```

### 为什么通过 Blocks 计算？

资源是按 **Block** 分配的：

| 资源 | 分配单位 | 影响 |
|-----|---------|------|
| **LDS（共享内存）** | 每 Block | Block 越大，LDS 需求越多 |
| **Registers（寄存器）** | 每 Thread | Kernel 复杂度影响 |
| **Block 数量限制** | 每 CU | 硬件限制 |

`hipOccupancyMaxActiveBlocksPerMultiprocessor` 综合考虑所有资源限制，返回能同时运行的 Blocks 数，再换算成 Wavefronts 得到占用率。

### 与 DirectX 对比

| HIP | DirectX | 说明 |
|-----|---------|------|
| Wavefront (32 threads) | Wave (32 lanes) | 基本执行单位 |
| Block / Workgroup | Thread Group | 共享 LDS 的线程组 |
| CU (Compute Unit) | SIMD Unit | 硬件执行单元 |
| `hipOccupancyMaxActiveBlocksPerMultiprocessor` | 无直接等价 | DX 通过 PIX 查看占用率 |

### 快速参考

```cpp
// 获取最佳 block size
int blockSize, minGridSize;
hipOccupancyMaxPotentialBlockSize(&minGridSize, &blockSize, myKernel, 0, 0);

// 计算占用率
int blocksPerCU;
hipOccupancyMaxActiveBlocksPerMultiprocessor(&blocksPerCU, myKernel, blockSize, 0);

hipDeviceProp_t prop;
hipGetDeviceProperties(&prop, 0);
int maxWaves = prop.maxThreadsPerMultiProcessor / prop.warpSize;
int activeWaves = blocksPerCU * (blockSize / prop.warpSize);
float occupancy = 100.0f * activeWaves / maxWaves;
printf("Occupancy: %.1f%%\n", occupancy);
```

---

## 11. 修改 GPU Kernel ISA 并调试（2026-03-17）

> 详细文档：[MODIFY_KERNEL_ISA.md](MODIFY_KERNEL_ISA.md)
> 脚本位置：[01_vector_add/1_dump_asm.sh](../01_vector_add/1_dump_asm.sh), [01_vector_add/2_recompile.sh](../01_vector_add/2_recompile.sh)

### 核心流程

```
C++ 源码 → GPU 汇编 (.s) → 手动修改 ISA → 重新编译 → 调试 C++ 源码
```

### 三步操作

```bash
# 步骤 1: 生成 GPU 汇编
./1_dump_asm.sh

# 步骤 2: 手动编辑 .s 文件
vim vector_add-hip-amdgcn-amd-amdhsa-gfx1201.s

# 步骤 3: 重新编译
./2_recompile.sh
```

### 常用 ISA 修改

| 原始指令 | 修改后 | 效果 |
|----------|--------|------|
| `v_add_f32` | `v_mul_f32` | 加法 → 乘法 |
| `v_add_f32` | `v_sub_f32` | 加法 → 减法 |
| `v_add_f32` | `v_max_f32` | 加法 → 取最大值 |

### 调试方法

修改后的可执行文件可以用原始 C++ 源码调试：

```bash
# 命令行调试
rocgdb ./vector_add_debug_modified

# VSCode 调试
# 选择 "HIP Debug Modified Kernel" 配置，按 F5
```

**关键点**：
- 调试器显示 C++ 源码位置
- 实际执行的是修改后的 ISA 指令
- 适用于验证指令行为、学习 GPU 汇编

### 编译工具链

```
.s (汇编) → llvm-mc → .o (目标文件)
.o → lld → .out (共享对象)
.out → clang-offload-bundler → .hipfb (Fatbin)
.hipfb + 源码 → clang++ --cuda-host-only → 可执行文件
```

---

## 12. Advanced Thread Trace (ATT) 指令级追踪（2026-03-20）

### 什么是 ATT

**Advanced Thread Trace** 是 rocprofv3 的指令级追踪功能，可以分析 wavefront 的每条指令执行时间。

### 安装 rocprof-trace-decoder

ROCm 7.2 默认不包含 ATT 解码器，需要单独安装：

```bash
# 下载 (Ubuntu 24.04)
curl -L -o /tmp/rocprof-trace-decoder.deb \
  "https://github.com/ROCm/rocprof-trace-decoder/releases/download/0.1.6/rocprof-trace-decoder-ubuntu-24.04-0.1.6-Linux.deb"

# 安装
sudo dpkg -i /tmp/rocprof-trace-decoder.deb

# 验证安装位置
ls /opt/rocm/lib/librocprof-trace-decoder.so
```

> GitHub 仓库：https://github.com/ROCm/rocprof-trace-decoder

### 使用 ATT

```bash
# 基本用法
rocprofv3 --att -d ./att_output -- ./your_program

# 指定目标 CU
rocprofv3 --att --att-target-cu 0 -- ./program

# 连续追踪多个 kernel
rocprofv3 --att --att-consecutive-kernels 3 -- ./program
```

### 输出文件

| 文件类型 | 说明 |
|----------|------|
| `*.att` | 原始二进制追踪数据 |
| `*_code_object_*.out` | Kernel ELF 代码对象 |
| `*_results.db` | SQLite 结果数据库 |
| `ui_output_*/` | 解码后的 JSON（用于可视化） |

### 可视化工具

**ROCprof Compute Viewer (RCV)** - AMD 官方 ATT 可视化工具（Qt6 GUI）。

> ⚠️ pip 安装不可用，需要从源码编译。

**安装依赖** (Ubuntu 24.04)：

```bash
sudo apt install -y libgl1 qt6-base-dev qmake6 build-essential cmake \
    libxkbcommon-dev qt6-tools-dev-tools
```

**从源码编译**：

```bash
cd ~
git clone https://github.com/ROCm/rocprof-compute-viewer.git
cd rocprof-compute-viewer
mkdir build && cd build
cmake .. -DQT_VERSION_MAJOR=6
make -j$(nproc)

# 添加到 PATH
echo 'export PATH="$HOME/rocprof-compute-viewer/build:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

**使用**：

```bash
# RCV 读取的是 ui_output_* 文件夹，不是 .att 文件
rocprof-compute-viewer ./att_output/ui_output_agent_XXX_dispatch_Y/
```

**ui_output 文件夹结构**：

| 文件 | 说明 |
|------|------|
| `code.json` | ISA 指令和 latency 数据 |
| `filenames.json` | GPU 信息和 wave 文件列表 |
| `occupancy.json` | dispatch 信息 |
| `*.wave` | wave trace 数据 |

**功能**：
- Hotspot 分析（指令延迟热点）
- Wave States（IDLE, EXEC, STALL, WAIT）
- Memory Ops 依赖分析

> GitHub：https://github.com/ROCm/rocprof-compute-viewer

### 注意事项

- Kernel 执行太快时追踪数据很少，建议用计算密集型程序测试
- `dispatch_1` 通常是 HIP runtime 内部初始化 kernel，你的 kernel 在后续 dispatch

---

## 🔗 相关资源

### 项目内文档

- [rocgdb 速查表](ROCGDB_CHEATSHEET.md) - 命令快速参考
- [汇编和反汇编对比](CheckDisam.md) - 理解优化等级差异
- [修改 GPU ISA 并调试](MODIFY_KERNEL_ISA.md) - 手动修改 kernel 指令

### 外部资源

- [ROCm 官方文档](https://rocm.docs.amd.com/)
- [ROCgdb 调试指南](https://rocm.docs.amd.com/projects/ROCgdb/en/latest/)
- [HIP 编程指南](https://rocm.docs.amd.com/projects/HIP/en/latest/)
- [AMD GPU ISA 参考](https://gpuopen.com/amd-isa-documentation/)

---

## 📌 快速参考卡片

### rocgdb 最常用命令

```gdb
# 调试控制
break kernel        # 设置断点
run                 # 运行
next                # 单步执行
continue            # 继续

# GPU 调试
info threads        # 查看所有 threads/wavefronts
info lanes          # 查看当前 lane 详情
lane N              # 切换 lane

# 查看数据
print idx           # 打印变量
print A[0]@64       # 打印数组
x/20i $pc           # 查看 20 条指令
disassemble /m      # 混合源码和汇编
```

### hipcc 编译选项

```bash
-g              # 添加调试符号
-O0             # 无优化（调试用）
-O2/-O3         # 优化（生产用）
-S              # 生成汇编文件
--save-temps    # 保留所有中间文件
```

---

最后更新：2026-03-20
基于：ROCm 7.2.26015, rocgdb 16.3, AMD RX 9070 XT (gfx1201)
