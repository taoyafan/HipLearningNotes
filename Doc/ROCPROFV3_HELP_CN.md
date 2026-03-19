# rocprofv3 命令行帮助（中文版）

> 翻译自 ROCm 7.2 rocprofv3 --help
> 最后更新：2026-03-17

## 基本用法

```bash
rocprofv3 [选项] -- <应用程序> [应用程序参数]
```

**重要**：rocprofv3 需要用双横线 `--` 分隔工具选项和要执行的应用程序。

---

## 通用选项

| 选项 | 说明 |
|------|------|
| `-h, --help` | 显示帮助信息并退出 |
| `-v, --version` | 打印版本信息并退出 |

---

## 输入/输出选项

| 选项 | 说明 |
|------|------|
| `-i INPUT, --input INPUT` | 运行配置的输入文件（JSON 或 YAML）或计数器收集配置（TXT） |
| `-o OUTPUT_FILE, --output-file OUTPUT_FILE` | 输出文件名。默认路径：`%hostname%/%pid%` |
| `-d OUTPUT_DIRECTORY, --output-directory OUTPUT_DIRECTORY` | 输出目录路径。默认路径：`%hostname%/%pid%` |
| `-f FORMAT, --output-format FORMAT` | 输出格式。支持：`csv`, `json`, `pftrace`, `otf2`, `rocpd` |
| `--output-config [BOOL]` | 生成 rocprofv3 配置的输出文件，如 `out_config.json` |
| `--log-level LEVEL` | 设置日志级别。可选：`fatal`, `error`, `warning`, `info`, `trace`, `env`, `config` |
| `-E FILE, --extra-counters FILE` | 包含额外计数器定义的 YAML 文件路径 |

---

## 聚合追踪选项

| 选项 | 说明 |
|------|------|
| `-r, --runtime-trace` | **推荐** 收集运行时追踪数据，包括：HIP Runtime API、Marker (ROCTx) API、RCCL API、rocDecode API、rocJPEG API、内存操作（拷贝、scratch、分配）、Kernel 调度。类似 `--sys-trace` 但不包含 HIP 编译器 API 和底层 HSA API |
| `-s, --sys-trace` | 收集系统级追踪数据，包括：HIP API、HSA API、Marker (ROCTx) API、RCCL API、rocDecode API、rocJPEG API、内存操作、Kernel 调度 |

---

## 基础追踪选项

| 选项 | 说明 |
|------|------|
| `--hip-trace` | 组合 `--hip-runtime-trace` 和 `--hip-compiler-trace`。仅启用 HIP API 追踪，不包含 kernel 追踪、内存拷贝追踪等 |
| `--marker-trace` | 收集 Marker (ROCTx) 追踪。旧版 rocprof 中称为 `--roctx-trace`，现已改进并增加更多功能 |
| `--kernel-trace` | 收集 Kernel 调度追踪 |
| `--memory-copy-trace` | 收集内存拷贝追踪。旧版 rocprof 中是 HIP/HSA 追踪的一部分，现为独立选项 |
| `--memory-allocation-trace` | 收集内存分配追踪。显示起始地址、分配大小、发生分配的 agent |
| `--scratch-memory-trace` | 收集 Scratch 内存操作追踪。帮助确定 scratch 分配并有效管理 |
| `--hsa-trace` | 收集所有 HSA 追踪：`--hsa-core-trace`、`--hsa-amd-trace`、`--hsa-image-trace`、`--hsa-finalizer-trace`。仅启用 HSA API 追踪 |
| `--rccl-trace` | 收集 RCCL（ROCm 通信集合库）追踪 |
| `--kokkos-trace` | 启用内置 Kokkos Tools 支持（隐含 `--marker-trace` 和 `--kernel-rename`） |
| `--rocdecode-trace` | 收集 rocDecode 追踪 |
| `--rocjpeg-trace` | 收集 rocJPEG 追踪 |

---

## 细粒度追踪选项

| 选项 | 说明 |
|------|------|
| `--hip-runtime-trace` | 收集 HIP Runtime API 追踪，如以 `hip` 开头的公共 HIP API 函数（如 `hipSetDevice`） |
| `--hip-compiler-trace` | 收集 HIP 编译器生成的代码追踪，如以 `__hip` 开头的 HIP API 函数（如 `__hipRegisterFatBinary`） |
| `--hsa-core-trace` | 收集 HSA 核心 API 追踪，如仅以 `hsa_` 为前缀的函数（如 `hsa_init`） |
| `--hsa-amd-trace` | 收集 HSA AMD 扩展 API 追踪，如以 `hsa_amd_` 为前缀的函数（如 `hsa_amd_coherency_get_type`） |
| `--hsa-image-trace` | 收集 HSA Image 扩展 API 追踪，如以 `hsa_ext_image_` 为前缀的函数（如 `hsa_ext_image_get_capability`） |
| `--hsa-finalizer-trace` | 收集 HSA Finalizer 扩展 API 追踪，如以 `hsa_ext_program_` 为前缀的函数（如 `hsa_ext_program_create`） |

---

## 计数器收集选项

| 选项 | 说明 |
|------|------|
| `--pmc [PMC ...]` | 指定要收集的性能监控计数器（多个计数器用逗号或空格分隔）。注意：如果整组计数器无法在单次运行中收集，任务将失败 |

---

## PC 采样选项

| 选项 | 说明 |
|------|------|
| `--pc-sampling-beta-enabled` | 启用 PC 采样支持（Beta 版本） |
| `--pc-sampling-unit` | 采样单位：`instructions`（指令）、`cycles`（周期）、`time`（时间） |
| `--pc-sampling-method` | 采样方法：`stochastic`（随机）、`host_trap`（主机陷阱） |
| `--pc-sampling-interval` | PC 采样间隔 |

---

## 后处理追踪选项

| 选项 | 说明 |
|------|------|
| `--stats` | 收集已启用追踪类型的统计信息。必须与一个或多个追踪选项组合使用。与旧版 rocprof 不同，不再默认包含 kernel 统计 |
| `-S, --summary` | 在分析会话结束时输出单个追踪数据摘要 |
| `-D, --summary-per-domain` | 在分析会话结束时输出每个追踪域的摘要 |
| `--summary-groups REGEX` | 输出与正则表达式匹配的每组域的摘要。例如 `'KERNEL_DISPATCH\|MEMORY_COPY'` 将生成 KERNEL_DISPATCH 和 MEMORY_COPY 域的摘要 |

---

## 摘要选项

| 选项 | 说明 |
|------|------|
| `--summary-output-file FILE` | 摘要输出到文件、stdout 或 stderr（默认：stderr） |
| `-u, --summary-units UNIT` | 输出摘要的时间单位：`sec`（秒）、`msec`（毫秒）、`usec`（微秒）、`nsec`（纳秒） |

---

## Kernel 命名选项

| 选项 | 说明 |
|------|------|
| `-M, --mangled-kernels` | 不对 kernel 名称进行 demangle（保持编译器生成的原始名称） |
| `-T, --truncate-kernels` | 截断 demangled 的 kernel 名称。旧版 rocprof 中称为 `--basenames [on/off]` |
| `--kernel-rename` | 使用 `roctxRangePush`/`roctxRangePop` 定义的区域名称重命名 kernel。旧版 rocprof 中称为 `--roctx-rename` |

---

## 过滤选项

| 选项 | 说明 |
|------|------|
| `--kernel-include-regex REGEX` | 仅包含与此正则表达式匹配的 kernel 进行计数器收集和线程追踪（不匹配的将被排除） |
| `--kernel-exclude-regex REGEX` | 排除与此正则表达式匹配的 kernel（在 `--kernel-include-regex` 之后应用） |
| `--kernel-iteration-range RANGE` | 迭代范围 |
| `-P, --collection-period (延迟):(采集时间):(重复次数)` | 指定采集周期。延迟是采集开始前的等待时间，采集时间是数据收集持续时间，重复次数是循环重复次数（0 = 无限重复）。默认单位为秒，可用 `--collection-period-unit` 更改 |
| `--collection-period-unit UNIT` | 更改 `--collection-period` 使用的时间单位：`hour`、`min`、`sec`、`msec`、`usec`、`nsec` |
| `--selected-regions` | 仅分析被 `roctxProfilerResume(0)` 和 `roctxProfilerPause(0)` 包围的代码区域 |

---

## Perfetto 专用选项

| 选项 | 说明 |
|------|------|
| `--perfetto-backend` | Perfetto 数据收集后端：`inprocess`（进程内）、`system`（系统模式，需要启动 traced 和 perfetto 守护进程） |
| `--perfetto-buffer-size KB` | Perfetto 输出缓冲区大小（KB）。默认：1 GB |
| `--perfetto-buffer-fill-policy` | 缓冲区满时的处理策略：`discard`（丢弃）、`ring_buffer`（环形缓冲区） |
| `--perfetto-shmem-size-hint KB` | Perfetto 共享内存大小提示（KB）。默认：64 KB |

---

## 显示选项

| 选项 | 说明 |
|------|------|
| `-L, --list-avail` | 列出可用的 PC 采样配置和计数器收集指标。旧版 rocprof 中称为 `--list-basic`、`--list-derived`、`--list-counters` |
| `--group-by-queue` | 按 HIP stream 而非 HSA queue 显示 kernel 和内存拷贝操作的提交位置 |

---

## 高级选项

| 选项 | 说明 |
|------|------|
| `--preload [LIBS ...]` | 预加载到 LD_PRELOAD 的库（适用于 sanitizer 库） |
| `--rocm-root PATH` | 使用给定路径作为 ROCm 根路径 |
| `--sdk-soversion VERSION` | 使用指定的 rocprofiler-sdk 共享对象版本号。例如 `--sdk-soversion=X` 对应 `librocprofiler-sdk.so.X` |
| `--sdk-version VERSION` | 使用指定的 rocprofiler-sdk 版本号。例如 `--sdk-version=X.Y.Z` 对应 `librocprofiler-sdk.so.X.Y.Z` |
| `-A, --agent-index MODE` | Agent 索引模式：<br>• `absolute`：绝对索引（node_id），如 Agent-0, Agent-2, Agent-4<br>• `relative`：相对索引（logical_node_id），考虑 cgroups 掩码，如 Agent-0, Agent-1, Agent-2（默认）<br>• `type-relative`：按类型的相对索引，如 CPU-0, GPU-0, GPU-1 |
| `--disable-signal-handlers` | 禁用 rocprofv3 的信号处理器。设为 true 时，应用程序自己的信号处理器将被使用 |
| `--process-sync` | 启用进程同步。设为 true 时，rocprofv3 会强制进程等待对等进程完成写入追踪数据后再继续 |
| `--minimum-output-data KB` | 仅当输出数据大小 > 指定值时生成输出文件。用于控制空文件的生成 |
| `-p PID, --pid PID, --attach PID` | 附加到目标进程（通过 PID）并作为工具在该进程内执行 |
| `--attach-duration-msec MS` | 使用 `--pid` 时，设置分析器附加的持续时间（毫秒）。未设置时，分析器会等待按 Enter 键后分离 |

---

## Advanced Thread Trace (ATT) 选项

高级线程追踪 - 指令级 GPU 追踪功能。

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `--advanced-thread-trace, --att` | - | 启用线程追踪 |
| `--att-library-path PATH` | - | 解码器库的搜索路径 |
| `--att-target-cu ID` | 1 | 目标计算单元 ID（或 WGP） |
| `--att-simd-select MASK` | 0xF | 要启用的 SIMD 位掩码（gfx9）或 SIMD ID（gfx10+） |
| `--att-consecutive-kernels N` | 0 | 连续追踪的 kernel 数量 |
| `--att-buffer-size SIZE` | 256MB | 线程追踪缓冲区大小 |
| `--att-shader-engine-mask MASK` | 0x1 | 要启用的 Shader Engine 位掩码 |
| `--att-gpu-index INDEXES` | All | 要启用线程追踪的 GPU 索引（逗号分隔） |
| `--att-perfcounters COUNTERS` | - | (gfx9) 性能计数器列表及其可选的 SIMD 掩码 |
| `--att-perfcounter-ctrl N` | 0 | (gfx9) 采集周期，范围 [1,32]。0 = 禁用 |
| `--att-activity N` | - | (gfx9) 收集硬件活动计数器。范围 [1,16]。推荐值：8 |
| `--att-serialize-all` | - | 序列化所有 kernel，而不仅仅是被追踪的 |

---

## 使用示例

### 基础追踪

```bash
# 运行时追踪（推荐入门）
rocprofv3 --runtime-trace -f csv -- ./myapp

# 系统级追踪（包含 HSA API）
rocprofv3 --sys-trace -f csv -- ./myapp

# 仅 kernel 追踪
rocprofv3 --kernel-trace -f csv -- ./myapp

# 内存操作追踪
rocprofv3 --memory-copy-trace --memory-allocation-trace -f csv -- ./myapp
```

### 指定输出

```bash
# 指定输出目录
rocprofv3 --runtime-trace -f csv -d ./output -- ./myapp

# 指定输出文件名
rocprofv3 --runtime-trace -f csv -o my_trace -- ./myapp
```

### 性能计数器

```bash
# 收集特定计数器
rocprofv3 --pmc SQ_WAVES,SQ_INSTS_VALU -f csv -- ./myapp

# 列出可用计数器
rocprofv3 --list-avail
```

### 高级线程追踪 (ATT)

```bash
# 启用 ATT
rocprofv3 --att -- ./myapp

# 指定目标 CU
rocprofv3 --att --att-target-cu 0 -- ./myapp

# 追踪连续 3 个 kernel
rocprofv3 --att --att-consecutive-kernels 3 -- ./myapp
```

### 附加到运行中的进程

```bash
# 附加到 PID 为 1234 的进程
rocprofv3 --attach 1234 --hip-trace --kernel-trace

# 附加 10 毫秒后分离
rocprofv3 --attach 1234 --attach-duration-msec 10 --hsa-trace
```

### MPI 应用

```bash
# 将 rocprofv3 放在 job launcher 内部
mpirun -n 4 rocprofv3 --hip-trace -- ./mympiapp
```

---

## 输出格式说明

| 格式 | 说明 |
|------|------|
| `csv` | 逗号分隔值文件，易于分析和导入 |
| `json` | JSON 格式，适合程序处理 |
| `pftrace` | Perfetto 追踪格式，可用 Perfetto UI 可视化 |
| `otf2` | OTF2 格式，适合大规模 HPC 分析 |
| `rocpd` | ROCm 性能数据库格式 |

---

## 常见用法速查

```bash
# 最常用：运行时追踪 + CSV 输出
rocprofv3 -r -f csv -- ./app

# 查看可用计数器
rocprofv3 -L

# Kernel 时间统计
rocprofv3 --kernel-trace --stats -f csv -- ./app

# 指令级追踪
rocprofv3 --att -- ./app
```

---

## 相关文档

- [rocprofv3 性能分析基础](../03_profiling_and_perf/02_rocprof_basics/README.md)
- [学习笔记 - rocprofv3 章节](LEARNING_NOTES.md#8-rocprofv3-性能分析工具2026-03-13)
- [ROCm 官方文档](https://rocm.docs.amd.com/projects/rocprofiler-sdk/en/latest/)
