# rocprofv3 性能分析工具

**rocprofv3** 是 AMD ROCm 最新的性能分析工具，类似于 NVIDIA 的 `nvprof`，或者 DirectX 开发中的 **PIX**。

> **GPU 版本要求**:
> - **RDNA4 (navi4x) / MI35x 及更新**: 必须使用 `rocprofv3`
> - RDNA3/CDNA3: 使用 `rocprofv2`
> - 更旧的架构: 使用 `rocprof` (v1)

## 与 DirectX 工具对比

| DirectX | ROCm | 功能 |
|---------|------|------|
| **PIX (GPU Capture)** | **rocprofv3** | GPU 性能分析、硬件计数器 |
| **GPU View** | **roctracer** | API 调用时间线追踪 |
| **Nsight Systems** | **Omniperf** | 系统级性能分析 |

---

## 编译和运行

### 基础运行（无分析）
```bash
hipcc -O3 -o rocprof_demo rocprof_demo.cpp
./rocprof_demo
```

### 使用 rocprofv3 分析

**重要**:
- rocprofv3 使用 `--` 分隔工具选项和应用程序
- 输出文件默认在 `<hostname>/<pid>/` 目录下，文件名格式为 `<pid>_<trace_type>.csv`

#### 1. 运行时追踪（最常用）⭐
```bash
rocprofv3 --runtime-trace -f csv -- ./rocprof_demo
```

**生成文件**：
| 文件名 | 内容 | 关键字段 |
|--------|------|----------|
| `<pid>_kernel_trace.csv` | Kernel 执行追踪 | Kernel_Name, Start/End_Timestamp, VGPR/SGPR_Count, Grid/Workgroup_Size |
| `<pid>_hip_api_trace.csv` | HIP API 调用 | Function (hipMalloc/hipMemcpy/...), Start/End_Timestamp, Correlation_Id |
| `<pid>_memory_copy_trace.csv` | 内存拷贝 | Direction (H2D/D2H), Source/Destination_Agent_Id, Start/End_Timestamp |
| `<pid>_memory_allocation_trace.csv` | 内存分配 | Operation (ALLOCATE/FREE), Agent_Id, Allocation_Size, Address |
| `<pid>_agent_info.csv` | 硬件信息 | Agent_Type (CPU/GPU), Name, Cu_Count, Simd_Count, Lds_Size |

#### 2. 系统级追踪（包含 HSA 底层 API）
```bash
rocprofv3 --sys-trace -f csv -- ./rocprof_demo
```

**生成文件**：`--runtime-trace` 的所有文件 +
| 文件名 | 内容 | 说明 |
|--------|------|------|
| `<pid>_hsa_api_trace.csv` | HSA 底层 API 调用 | HIP Runtime 内部调用的 HSA 函数，用于深度调试 |

#### 3. 单独追踪选项

**仅 Kernel 追踪**：
```bash
rocprofv3 --kernel-trace -f csv -- ./rocprof_demo
```
→ 生成 `<pid>_kernel_trace.csv`

**HIP API 追踪**：
```bash
rocprofv3 --hip-trace -f csv -- ./rocprof_demo
```
→ 生成 `<pid>_hip_api_trace.csv`

**内存拷贝追踪**：
```bash
rocprofv3 --memory-copy-trace -f csv -- ./rocprof_demo
```
→ 生成 `<pid>_memory_copy_trace.csv`

**内存分配追踪**：
```bash
rocprofv3 --memory-allocation-trace -f csv -- ./rocprof_demo
```
→ 生成 `<pid>_memory_allocation_trace.csv`

#### 4. 组合多种追踪
```bash
rocprofv3 --kernel-trace --hip-trace --memory-copy-trace -f csv -- ./rocprof_demo
```
→ 生成对应的多个 CSV 文件

#### 5. 收集硬件计数器
```bash
# 查看可用计数器
rocprofv3 --list-avail

# 直接在命令行指定计数器
rocprofv3 --pmc SQ_WAVES,SQ_INSTS_VALU -- ./rocprof_demo

# 使用输入文件
rocprofv3 -i counters.txt -- ./rocprof_demo
```
→ 生成 `<pid>_counter_collection.csv`

**counters.txt 示例**：
```
pmc: SQ_WAVES, SQ_INSTS_VALU
```

> ⚠️ **RDNA4 (gfx1201) 已知问题**: 截至 ROCm 7.2，硬件计数器功能在 RDNA4 上存在 bug，会导致 `unordered_map::at` 错误。追踪功能（--runtime-trace 等）正常工作。

#### 6. 指定输出目录
```bash
# 指定输出目录
rocprofv3 --kernel-trace -f csv -d ./profile_output -- ./rocprof_demo

# 指定输出文件名前缀
rocprofv3 --kernel-trace -f csv -o my_results -- ./rocprof_demo

# 同时输出多种格式（支持 csv, json, pftrace, otf2）
rocprofv3 --kernel-trace -f csv json -- ./rocprof_demo
```

---

## 常用分析场景

### 场景 1：快速了解程序行为 ⭐
```bash
rocprofv3 --runtime-trace -f csv -- ./rocprof_demo
# 查看生成的 CSV 文件
```

### 场景 2：找出最慢的 kernel
```bash
rocprofv3 --kernel-trace -f csv -- ./rocprof_demo
# 查看 kernel_trace.csv，按 Duration 排序
```

### 场景 3：分析内存传输
```bash
rocprofv3 --memory-copy-trace -f csv -- ./rocprof_demo
# 分析 Host↔Device 数据传输时间
```

### 场景 4：对比优化前后
```bash
# 优化前
rocprofv3 --kernel-trace -f csv -d ./before -- ./rocprof_demo

# 修改代码优化后
rocprofv3 --kernel-trace -f csv -d ./after -- ./rocprof_demo

# 对比结果
diff before/*/kernel_trace.csv after/*/kernel_trace.csv
```

---

## rocprofv3 输出文件说明

运行 `rocprofv3` 后，默认在 `<hostname>/<pid>/` 目录下生成文件。

### --runtime-trace vs --sys-trace 输出对比

| 追踪选项 | 生成的文件 | 说明 |
|----------|-----------|------|
| `--runtime-trace` | `<pid>_kernel_trace.csv` | Kernel 执行时间、寄存器使用 |
| | `<pid>_hip_api_trace.csv` | HIP API 调用 (hipMalloc, hipMemcpy...) |
| | `<pid>_memory_copy_trace.csv` | Host↔Device 内存拷贝 |
| | `<pid>_memory_allocation_trace.csv` | 内存分配/释放 |
| | `<pid>_agent_info.csv` | CPU/GPU 硬件信息 |
| `--sys-trace` | 以上所有文件 + | |
| | `<pid>_hsa_api_trace.csv` | HSA 底层 API 调用 |

### 文件内容详解

| 文件 | 内容 | 用途 |
|------|------|------|
| `kernel_trace.csv` | Kernel 名称、执行时间、VGPR/SGPR、Grid/Block 大小 | 找出最慢的 kernel |
| `hip_api_trace.csv` | HIP API 调用及耗时 | 分析 API 开销 |
| `hsa_api_trace.csv` | HSA 底层 API（仅 sys-trace） | 深度调试 HIP Runtime |
| `memory_copy_trace.csv` | H2D/D2H 传输时间、方向、大小 | 优化数据传输 |
| `memory_allocation_trace.csv` | 分配大小、地址、Correlation_Id | 分析内存使用 |
| `agent_info.csv` | GPU 型号、CU 数、LDS 大小等 | 了解硬件规格 |

### 什么时候用哪个？

```
--runtime-trace  ← 日常开发、性能分析（推荐）
     │
     │  需要更深入调试？
     ↓
--sys-trace      ← 调试 HIP Runtime 问题、学习底层原理
```

### 输出格式选项

```bash
rocprofv3 --runtime-trace -f csv -- ./rocprof_demo      # CSV（推荐）
rocprofv3 --runtime-trace -f json -- ./rocprof_demo     # JSON
rocprofv3 --runtime-trace -f pftrace -- ./rocprof_demo  # Perfetto trace
rocprofv3 --runtime-trace -f otf2 -- ./rocprof_demo     # OTF2 格式
```

**提示**：可以用 Excel 或 Python pandas 打开 `.csv` 文件进行深入分析。

---

## 常见性能指标解读

### 1. Kernel 执行时间
- **Duration**：执行时间（纳秒）
- 转换为毫秒：`Duration / 1,000,000`

### 2. 占用率（Occupancy）
- 实际活跃的 wavefront 数 / 硬件最大 wavefront 数
- 理想值：> 50%
- 低于 25% 说明可能有资源瓶颈（寄存器、共享内存）

### 3. 内存带宽利用率
- 实际带宽 / 硬件峰值带宽
- 理想值：> 60%（内存密集型 kernel）
- 低于 30% 说明可能是计算瓶颈，而非内存瓶颈

### 4. GFLOPS（计算吞吐量）
- 实际 GFLOPS / 硬件峰值 GFLOPS
- 理想值：> 30%（计算密集型 kernel）

---

## 学习路径建议

1. **现在**：运行 `rocprofv3 --runtime-trace -f csv` 查看程序整体行为
2. **下一步**：学习如何优化占用率（04_occupancy）
3. **高级**：使用 Omniperf 进行详细的硬件级分析

---

## rocprofv3 vs hipEvent 对比

| | **rocprofv3** | **hipEvent** |
|---|-------------|--------------|
| **使用方式** | 外部工具 | 代码中插入 |
| **需要修改代码** | ❌ 否 | ✅ 是 |
| **性能开销** | 较大（5-20%） | 很小（<1%） |
| **测量内容** | 硬件计数器、占用率、带宽 | 时间 |
| **适用场景** | 性能调优、找瓶颈 | 快速测试、自动化 |

**实际工作流程**：
1. 用 `hipEvent` 快速测量 kernel 时间
2. 发现性能问题时，用 `rocprofv3` 深入分析
3. 优化后再用 `hipEvent` 验证提升

---

## 下一步

完成 rocprofv3 基础后，建议学习：
- **03_memory_bandwidth**: 内存访问模式优化
- **04_occupancy**: 提高 GPU 占用率
- **Omniperf**: 更详细的硬件分析工具

---

## 参考资料

- ROCm 官方文档: https://rocm.docs.amd.com/projects/rocprofiler/en/latest/
- rocprofv3 命令行帮助: `rocprofv3 --help`

## rocprof 版本命令对照

| rocprof v1 | rocprofv2 | rocprofv3 | 说明 |
|------------|-----------|-----------|------|
| `rocprof --stats` | `rocprofv2 --kernel-trace` | `rocprofv3 --kernel-trace -f csv --` | Kernel 追踪 |
| `rocprof --hsa-trace` | `rocprofv2 --hsa-trace` | `rocprofv3 --hsa-trace -f csv --` | HSA API 追踪 |
| `rocprof --hip-trace` | `rocprofv2 --hip-trace` | `rocprofv3 --hip-trace -f csv --` | HIP API 追踪 |
| N/A | N/A | `rocprofv3 --runtime-trace -f csv --` | 运行时追踪（推荐）|
| N/A | N/A | `rocprofv3 --sys-trace -f csv --` | 系统级完整追踪 |
| `rocprof --list-basic` | `rocprofv2 --list-counters` | `rocprofv3 --help` | 查看选项 |
| `rocprof -i input.txt` | `rocprofv2 -i counters.txt` | `rocprofv3 -i counters.txt --` | 指定计数器文件 |

> **注意**:
> - rocprofv3 必须使用 `--` 分隔工具选项和目标应用程序
> - 使用 `-f csv` 输出 CSV 格式（默认为 SQLite 数据库）
