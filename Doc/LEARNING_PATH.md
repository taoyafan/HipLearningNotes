# HIP 学习路径 - DirectX 3D 驱动背景专用

## 👤 背景

- **经验**: DirectX 3D 驱动开发
- **熟悉**: HLSL, GPU 工作流程, 图形管线
- **目标**: 快速掌握 HIP 调试、性能分析、贡献 ROCm

---

## 🎯 HLSL vs HIP 快速对照表

| HLSL Compute Shader | HIP/CUDA | 说明 |
|---------------------|----------|------|
| `[numthreads(X,Y,Z)]` | `<<<blocks, threads>>>` | 线程组织 |
| `SV_DispatchThreadID` | `blockIdx * blockDim + threadIdx` | 全局线程索引 |
| `SV_GroupThreadID` | `threadIdx` | 组内线程索引 |
| `SV_GroupID` | `blockIdx` | 线程组索引 |
| `groupshared` | `__shared__` | 共享内存 |
| `GroupMemoryBarrierWithGroupSync()` | `__syncthreads()` | 同步 |
| `InterlockedAdd/Max` | `atomicAdd/Max` | 原子操作 |

**主要区别**：
- HIP 没有图形管线，纯计算
- 需要手动管理 CPU↔GPU 数据传输
- 更灵活的内存模型（pinned memory, unified memory）

---

## 📅 2周速成计划

### **Week 1: 调试与性能分析工具链**

#### Day 1-2: 调试工具
- [ ] **rocgdb**: GPU 调试器
  - 设置断点、查看 GPU 线程状态
  - 切换线程：`rocm thread (blockIdx):(threadIdx)`
  - 查看变量：`print threadIdx.x`, `print data[idx]`
- [ ] **GPU assert**: kernel 中的运行时检查
- [ ] **compute-sanitizer**: 内存错误检测
  - `--tool memcheck`: 内存越界
  - `--tool racecheck`: 竞态条件

#### Day 3-5: 性能分析
- [ ] **rocprofv3**: 核心分析工具（RDNA4/MI35x 及更新使用 v3）
  ```bash
  rocprofv3 --runtime-trace -f csv -- ./app  # 运行时追踪（推荐）
  rocprofv3 --kernel-trace -f csv -- ./app   # Kernel 追踪
  rocprofv3 --sys-trace -f csv -- ./app      # 系统级完整追踪
  ```
- [ ] **关键性能计数器**:
  - `TCC_HIT/MISS`: L2 缓存命中率
  - `FETCH_SIZE/WRITE_SIZE`: 内存带宽
  - `SQ_WAVES`: 波前数量
  - `ALUStalledByLDS`: LDS 停顿
- [ ] **roctracer**: API 调用追踪（生成 Chrome Tracing）
- [ ] **Omniperf**: AMD 新一代性能分析工具（类似 Nsight Compute）

#### Day 6-7: 实战优化
- [ ] 内存带宽测试
- [ ] 占用率优化
- [ ] 瓶颈分析实战

---

### **Week 2: GPGPU 算法 + ROCm 贡献**

#### Day 8-10: GPGPU 特有算法

**这些算法在 3D 图形中很少用，但在通用计算中核心：**

**1. Reduction（归约）**
- **用途**: 求和、最大值、机器学习损失函数
- **挑战**: 线程协作、shared memory、树形归约
- **实现**: naive → shared memory → warp shuffle (3个版本)

**2. Scan（前缀和）**
- **用途**: 流压缩、基数排序、动态内存分配
- **挑战**: 不能简单并行，需特殊算法（Blelloch, Kogge-Stone）
- **应用**: 与 3D 渲染完全不同的计算模式

**3. Sparse Matrix（稀疏矩阵）**
- **用途**: 科学计算、图算法、机器学习
- **挑战**: 不规则访问模式、难以优化
- **格式**: CSR, COO, ELL

**4. Graph Algorithms（图算法）**
- **用途**: 社交网络、推荐系统、路径规划
- **挑战**: 高度不规则、负载不均衡
- **示例**: BFS, PageRank

#### Day 11-12: 为 ROCm 贡献

**ROCm 生态系统架构**:
```
应用层: PyTorch/TensorFlow, hipBLAS/hipFFT
HIP 层: HIP API, hipcc 编译器
驱动层: ROCr (Runtime), ROCt (Thunk), AMDGPU 驱动
```

**推荐贡献领域**（基于驱动背景）:
- **rocm-smi**: GPU 监控工具
- **roctracer**: API 追踪工具
- **HIP Runtime**: 运行时库优化

**开发流程**:
```bash
# Clone 仓库
git clone https://github.com/ROCm/HIP.git

# 构建
mkdir build && cd build
cmake ..
make -j$(nproc)

# 运行测试
cd ../tests
./run_tests.sh

# 提交 PR
git checkout -b fix-xyz
git commit -m "[Component] Brief description"
gh pr create
```

**找任务**: 搜索 "good first issue" 标签

#### Day 13-14: 综合项目
- [ ] 矩阵乘法完整优化（从 naive 到 tile + shared memory）
- [ ] 性能分析报告（使用 rocprofv3）
- [ ] 考虑提交到 ROCm examples

---

## 🗂️ 项目结构规划

```
hip_learning/
├── 01_vector_add/           # ✅ 已完成：基础示例
├── 02_debug_example/        # ✅ 已完成：调试技巧
│
├── 03_profiling_and_perf/   # Week 1: 性能分析
│   ├── 01_timing_basics.cpp       # hipEvent 计时
│   ├── 02_rocprof_guide.cpp       # rocprof 使用示例
│   ├── 03_memory_bandwidth.cpp    # 内存带宽测试
│   ├── 04_occupancy.cpp           # 占用率优化
│   ├── 05_bottleneck_analysis.cpp # 瓶颈分析
│   ├── rocprof_metrics.txt        # 性能计数器配置
│   └── README.md                  # 详细说明
│
├── 04_compute_algorithms/   # Week 2: GPGPU 算法
│   ├── 01_reduction/
│   │   ├── naive.cpp             # 朴素版本
│   │   ├── shared_mem.cpp        # 使用 shared memory
│   │   ├── warp_shuffle.cpp      # 使用 warp intrinsics
│   │   └── benchmark.cpp         # 性能对比
│   ├── 02_scan/
│   │   ├── blelloch_scan.cpp     # Blelloch 算法
│   │   └── applications.cpp      # 应用示例
│   ├── 03_sparse_matrix/
│   │   ├── csr_format.cpp        # CSR 格式
│   │   └── spmv.cpp              # 稀疏矩阵向量乘
│   └── 04_graph_algorithms/
│       ├── bfs.cpp               # 广度优先搜索
│       └── pagerank.cpp          # PageRank
│
└── 05_rocm_contribution/    # ROCm 贡献指南
    ├── setup_dev_env.md         # 开发环境设置
    ├── coding_guidelines.md     # 代码规范
    └── pr_workflow.md           # PR 流程
```

---

## 🛠️ 核心工具速查

### rocgdb（GPU 调试）
```bash
rocgdb ./app
(gdb) break kernel_name
(gdb) run
(gdb) info rocm threads         # 查看 GPU 线程
(gdb) rocm thread (0,0):(1,5)  # 切换到特定线程
(gdb) print threadIdx.x
```

### rocprofv3（性能分析）
```bash
# 运行时追踪（推荐入门）
rocprofv3 --runtime-trace -f csv -- ./app

# Kernel 追踪
rocprofv3 --kernel-trace -f csv -- ./app

# 系统级完整追踪
rocprofv3 --sys-trace -f csv -- ./app

# 指定计数器
rocprofv3 --pmc SQ_WAVES,SQ_INSTS_VALU -- ./app

# 查看帮助
rocprofv3 --help
```

> **注意**: RDNA4 (navi4x) / MI35x 及更新架构必须使用 rocprofv3。命令中必须用 `--` 分隔选项和应用程序。使用 `-f csv` 输出 CSV 格式。

### compute-sanitizer（错误检测）
```bash
compute-sanitizer --tool memcheck ./app   # 内存错误
compute-sanitizer --tool racecheck ./app  # 竞态条件
```

### roctracer（API 追踪）
```bash
roctracer --hip-trace --hsa-trace \
          --output-format json \
          ./app
# 在 chrome://tracing 中打开 JSON
```

### Omniperf（新一代分析器）
```bash
omniperf profile -n myapp -- ./app
omniperf analyze -p workloads/myapp/
```

---

## 📚 学习资源

### 官方文档
- [ROCm HIP Programming Guide](https://rocm.docs.amd.com/projects/HIP/en/latest/)
- [HIP API Reference](https://rocm.docs.amd.com/projects/HIP/en/latest/doxygen/html/)
- [Performance Guidelines](https://rocm.docs.amd.com/projects/HIP/en/latest/how-to/performance_guidelines.html)

### GitHub 仓库
- [ROCm/HIP](https://github.com/ROCm/HIP)
- [ROCm/rocprofiler](https://github.com/ROCm/rocprofiler)
- [ROCm/roctracer](https://github.com/ROCm/roctracer)

### 经典书籍（CUDA 相关，概念通用）
- **Programming Massively Parallel Processors** (PMPP)
- **CUDA by Example**

---

## ✅ 检查点

完成每个阶段后打勾：

### Week 1
- [ ] 熟练使用 rocgdb 调试 GPU kernel
- [ ] 能使用 rocprofv3 收集性能指标
- [ ] 理解内存带宽和占用率的概念
- [ ] 完成至少一次 kernel 性能优化

### Week 2
- [ ] 实现并理解 Reduction 算法
- [ ] 实现并理解 Scan 算法
- [ ] 成功编译 ROCm 仓库
- [ ] 找到至少一个可以贡献的 issue

### 最终目标
- [ ] 能独立调试和优化 HIP kernel
- [ ] 能使用 profiler 分析性能瓶颈
- [ ] 理解 GPGPU 和图形计算的区别
- [ ] 完成至少一次 ROCm 代码贡献（PR）

---

## 💡 下一步行动

**立即开始**: 创建 `03_profiling_and_perf/` 目录，开始性能分析学习。

**长期规划**: 2 周后评估进度，决定是深入某个领域（如贡献 roctracer）还是学习更高级主题（如 GPU 内核优化、多 GPU 编程）。
