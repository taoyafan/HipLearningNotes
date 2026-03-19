# 查看 AMD GPU 汇编代码指南

本指南展示如何查看和理解 AMD GPU 的 ISA（Instruction Set Architecture）。

---

## 🎯 快速方法总结

### 方法 1: 生成汇编文件（推荐）
```bash
# 生成 .s 汇编文件
hipcc -S -o vector_add.s vector_add.cpp
hipcc -S -O0 -o vector_add_O0.s vector_add.cpp

# 保留所有中间文件
hipcc --save-temps -o vector_add vector_add.cpp
# 生成: *.bc (LLVM bitcode), *.ll (LLVM IR), *.s (汇编)

# 查看汇编
less vector_add.s
# 或者
grep -A 50 "\.globl.*vector_add" vector_add.s
```

### 方法 2: 反汇编二进制文件
```bash
# 编译
hipcc -o vector_add vector_add.cpp

# 反汇编
/opt/rocm/llvm/bin/llvm-objdump -d vector_add > disasm.txt

# 只查看 GPU kernel
/opt/rocm/llvm/bin/llvm-objdump -d vector_add | grep -A 100 "_Z10vector_add"
```

### 方法 3: 在 rocgdb 中查看
```gdb
rocgdb ./vector_add
(gdb) break vector_add
(gdb) run
(gdb) disassemble              # 反汇编当前函数
(gdb) disassemble /m           # 混合源代码和汇编 Use this !!!!!!
(gdb) x/20i $pc                # 查看程序计数器处的指令
```

### 方法 4: ROCm 工具
```bash
# 列出代码对象
/opt/rocm/bin/rocm-obj-ls vector_add

# 提取 kernel
/opt/rocm/bin/rocm-obj-extract vector_add --kernel vector_add -o kernel.co

# 反汇编
/opt/rocm/llvm/bin/llvm-objdump -d kernel.co
```

---

## 📊 AMD GPU ISA 汇编代码解读

### vector_add kernel 的实际汇编（已生成）

```assembly
_Z10vector_addPKfS0_Pfi:                ; @_Z10vector_addPKfS0_Pfi
; %bb.0:                                # 基本块 0（入口）
	s_clause 0x1                        # 标量子句（允许连续的 load 指令）
	s_load_dword s0, s[4:5], 0x2c       # 加载 N (第4个参数) 到 SGPR s0
	s_load_dword s1, s[4:5], 0x18       # 加载 blockDim 到 SGPR s1
	s_waitcnt lgkmcnt(0)                # 等待标量内存加载完成
	s_and_b32 s0, s0, 0xffff            # 掩码操作
	v_mad_u64_u32 v[0:1], null, s6, s0, v[0:1]  # 计算全局索引 idx
	                                    # v[0:1] = blockIdx * blockDim + threadIdx
	s_mov_b32 s0, exec_lo               # 保存执行掩码
	v_cmpx_gt_i32_e64 s1, v0            # 比较 idx < N，更新 exec 掩码
	s_cbranch_execz .LBB0_2             # 如果所有 lanes 都不活跃，跳转到结尾

; %bb.1:                                # 基本块 1（实际计算）
	s_load_dwordx4 s[0:3], s[4:5], 0x0  # 加载 A, B 指针（4个 DWORD = 128 bits）
	v_ashrrev_i32_e32 v1, 31, v0        # 符号扩展（计算高 32 位）
	s_load_dwordx2 s[4:5], s[4:5], 0x10 # 加载 C 指针
	v_lshlrev_b64 v[0:1], 2, v[0:1]     # idx *= 4（float 是 4 字节）
	s_waitcnt lgkmcnt(0)                # 等待标量加载完成

	# 计算 A[idx] 的地址：A + idx*4
	v_add_co_u32 v2, vcc_lo, s0, v0     # 低 32 位
	v_add_co_ci_u32_e64 v3, null, s1, v1, vcc_lo  # 高 32 位（带进位）

	# 计算 B[idx] 的地址：B + idx*4
	v_add_co_u32 v4, vcc_lo, s2, v0
	v_add_co_ci_u32_e64 v5, null, s3, v1, vcc_lo

	# 从全局内存加载数据
	global_load_dword v2, v[2:3], off   # v2 = A[idx]
	global_load_dword v3, v[4:5], off   # v3 = B[idx]

	# 计算 C[idx] 的地址
	v_add_co_u32 v0, vcc_lo, s4, v0
	v_add_co_ci_u32_e64 v1, null, s5, v1, vcc_lo

	s_waitcnt vmcnt(0)                  # 等待向量内存加载完成
	v_add_f32_e32 v2, v2, v3            # v2 = A[idx] + B[idx]（浮点加法）
	global_store_dword v[0:1], v2, off  # C[idx] = v2

.LBB0_2:                                # 基本块 2（退出）
	s_endpgm                            # 结束程序
```

---

## 🔍 指令格式详解

### 寄存器类型

| 前缀 | 寄存器类型 | 说明 | 数量 |
|------|-----------|------|------|
| **v** | VGPR | Vector GPR（每个 lane 独立） | 256 个 |
| **s** | SGPR | Scalar GPR（整个 wave 共享） | 104 个 |
| **vcc** | VCC | Vector Condition Code（条件码） | 1 个 |
| **exec** | EXEC | Execution Mask（执行掩码） | 1 个 |
| **ttmp** | Trap Temp | 陷阱临时寄存器 | 16 个 |

### 指令类型

#### 标量指令 (S_*)
```assembly
s_load_dword s0, s[4:5], 0x2c    # 从内存加载到 SGPR
s_waitcnt lgkmcnt(0)              # 等待标量内存操作
s_mov_b32 s0, exec_lo             # 移动数据
s_and_b32 s0, s0, 0xffff          # 按位与
s_cbranch_execz label             # 条件分支
```

#### 向量指令 (V_*)
```assembly
v_add_f32_e32 v2, v2, v3          # 浮点加法（每个 lane 独立）
v_mad_u64_u32 v[0:1], null, s6, s0, v[0:1]  # 乘加（64位）
v_cmpx_gt_i32_e64 s1, v0          # 比较并更新 exec 掩码
v_lshlrev_b64 v[0:1], 2, v[0:1]   # 64位左移
```

#### 全局内存访问
```assembly
global_load_dword v2, v[2:3], off   # 从全局内存加载 DWORD
global_store_dword v[0:1], v2, off  # 存储到全局内存
```

#### 等待指令
```assembly
s_waitcnt lgkmcnt(0)    # 等待 LDS/GDS/Konstant memory 操作完成
s_waitcnt vmcnt(0)      # 等待 VRAM（全局内存）操作完成
```

---

## 💡 关键模式识别

### 模式 1: 计算全局线程索引
```assembly
# C 代码: int idx = blockIdx.x * blockDim.x + threadIdx.x;

v_mad_u64_u32 v[0:1], null, s6, s0, v[0:1]
# v[0:1] = s6 * s0 + v[0:1]
#        = blockIdx * blockDim + threadIdx
```

**对应关系**：
- `s6` = blockIdx
- `s0` = blockDim
- `v[0:1]` = threadIdx（输入），idx（输出）

### 模式 2: 边界检查
```assembly
# C 代码: if (idx < N)

v_cmpx_gt_i32_e64 s1, v0       # 比较 N > idx，更新 exec 掩码
s_cbranch_execz .LBB0_2        # 如果所有 lanes 不活跃，跳过
```

**关键**：
- 使用 `v_cmpx_*` 指令自动更新 `exec` 掩码
- 不活跃的 lanes 不会执行后续指令

### 模式 3: 计算数组索引
```assembly
# C 代码: C[idx] = ...（idx 是 int，但数组元素是 float/4字节）

v_lshlrev_b64 v[0:1], 2, v[0:1]    # idx *= 4（左移 2 位 = 乘以 4）
```

### 模式 4: 64位地址计算
```assembly
# C 代码: &C[idx]（C 是 64 位指针）

v_add_co_u32 v0, vcc_lo, s4, v0           # 低 32 位：addr_lo = C_lo + offset_lo
v_add_co_ci_u32_e64 v1, null, s5, v1, vcc_lo  # 高 32 位（带进位）
```

### 模式 5: 内存访问
```assembly
# C 代码: float a = A[idx];

global_load_dword v2, v[2:3], off    # v2 = *(float*)(v[2:3])
s_waitcnt vmcnt(0)                    # 等待加载完成
```

### 模式 6: 浮点运算
```assembly
# C 代码: C[idx] = A[idx] + B[idx];

v_add_f32_e32 v2, v2, v3    # v2 = v2 + v3（浮点加法）
```

---

## 🎓 与 DirectX 驱动对比

| 概念 | AMD GPU ISA | DirectX HLSL | 您的驱动经验 |
|------|------------|--------------|-------------|
| **寄存器** | VGPR (v0-v255) | 每线程寄存器 | GPU 硬件寄存器 |
| **共享寄存器** | SGPR (s0-s103) | Uniform 变量 | 常量缓冲区 |
| **执行掩码** | exec | - | 类似像素 Quad 掩码 |
| **内存访问** | global_load/store | Load/Store | VRAM 读写 |
| **条件执行** | v_cmpx + exec | if/else | 分支预测 |
| **同步** | s_waitcnt | - | 内存屏障 |

**关键理解**：
- AMD GPU 是 **SIMT**（单指令多线程），不是 SIMD
- `exec` 掩码控制哪些 lanes 执行（类似 DX 的 pixel quad mask）
- 内存访问是**合并的**（coalesced）以提高带宽

---

## 📋 常用分析技巧

### 技巧 1: 统计指令类型
```bash
# 统计有多少标量 load
grep -c "s_load" vector_add.s

# 统计有多少全局内存访问
grep -c "global_" vector_add.s

# 统计浮点运算
grep -c "v_.*_f32" vector_add.s
```

### 技巧 2: 查找关键指令
```bash
# 查找内存访问
grep "global_load\|global_store" vector_add.s

# 查找等待指令（可能的性能瓶颈）
grep "s_waitcnt" vector_add.s

# 查找分支指令
grep "branch" vector_add.s
```

### 技巧 3: 对比优化效果
```bash
# 编译未优化版本
hipcc -S -O0 -o vector_add_O0.s vector_add.cpp

# 编译优化版本
hipcc -S -O3 -o vector_add_O3.s vector_add.cpp

# 对比行数（指令数量）
wc -l vector_add_O0.s vector_add_O3.s

# 详细对比
diff -u vector_add_O0.s vector_add_O3.s | less
```

### 技巧 4: 查看特定优化
```bash
# 查看循环展开
hipcc -S -O3 -mllvm -unroll-threshold=1000 -o unrolled.s vector_add.cpp

# 查看向量化
hipcc -S -O3 -mllvm -force-vector-width=4 -o vectorized.s vector_add.cpp
```

---

## 🔗 相关文档

### AMD 官方文档
- [GCN ISA 参考](https://www.amd.com/en/support/gpu/amd-radeon-5000-series)
- [RDNA ISA 参考](https://gpuopen.com/rdna-performance-guide/)
- [ROCm LLVM 后端文档](https://llvm.org/docs/AMDGPUUsage.html)

### 实用工具
- [Godbolt Compiler Explorer](https://godbolt.org) - 在线查看汇编
- `llvm-mca` - 分析指令吞吐量
- `rocprof` - 性能分析（会关联汇编代码）

---

## 📝 实战示例

### 查看当前生成的汇编
```bash
cd /home/yafan/hip_learning/01_vector_add

# 查看完整汇编
less vector_add.s

# 只看 kernel 部分
grep -A 50 "_Z10vector_addPKfS0_Pfi:" vector_add.s

# 统计指令数量
grep -A 100 "_Z10vector_addPKfS0_Pfi:" vector_add.s | grep "^\s*[vsgl]_" | wc -l
```

### 在 rocgdb 中实时查看
```bash
rocgdb ./vector_add

# 设置断点并运行
(gdb) break vector_add
(gdb) run

# 反汇编
(gdb) disassemble

# 混合源码和汇编
(gdb) disassemble /m

# 查看寄存器状态
(gdb) info registers
```

---

## ❓ 常见问题：为什么 .s 文件很短，但 rocgdb 显示的代码很长？

### 核心原因

#### `.s` 文件（编译器输出）
- **指令数**: 约 31 条实际 GPU 指令
- **总行数**: 223 行（包含元数据）
- **内容**: 编译器生成的干净、优化后的代码

```bash
# 统计实际指令
grep -E "^\s+(s_|v_|global_)" vector_add-hip-amdgcn-amd-amdhsa-gfx1201.s | wc -l
# 输出: 31
```

#### `rocgdb disassemble`（运行时代码）
- **指令数**: 80-200+ 条（取决于优化等级）
- **内容**: 实际执行的机器码 + 调试代码 + 未优化的冗余指令

---

### 详细对比

| 特性 | .s 汇编文件 | rocgdb disassemble |
|------|------------|-------------------|
| **来源** | 编译器直接输出 | 运行时机器码反汇编 |
| **指令数** | 31 条（优化后） | 80-200+ 条（取决于 -O 等级） |
| **内容** | 纯 kernel 代码 + 元数据 | kernel + 调试代码 + 未优化代码 |
| **优化** | 反映 `-O` 编译选项 | 反映实际执行代码（含调试版本） |
| **调试信息** | 无（纯汇编） | 有（源代码行号、变量名） |
| **可读性** | ✅ 干净简洁 | ❌ 包含冗余代码 |
| **用途** | 学习 GPU 指令、分析优化 | 运行时调试、查看变量值 |

---

### 差异来源

#### 1. 编译优化等级

```bash
# -O0 (无优化，用于调试)
hipcc -g -O0 -o vector_add vector_add.cpp
# rocgdb 看到: ~150 条指令（包含大量冗余代码）

# -O3 (完全优化)
hipcc -O3 -o vector_add vector_add.cpp
# rocgdb 看到: ~35-40 条指令（接近 .s 文件）
```

**-O0 vs -O3 的差异**：
- `-O0`: 保留所有中间变量、不做寄存器优化、保留边界检查
- `-O3`: 消除冗余指令、激进寄存器优化、内联函数

#### 2. 调试代码 (`-g`)

```bash
hipcc -g -O0 -o vector_add vector_add.cpp
```

编译器可能插入：
- 额外的 `s_nop` 指令（用于断点）
- 边界检查代码
- 寄存器值保存/恢复（方便查看变量）
- 栈对齐代码

#### 3. 元数据 vs 实际代码

**.s 文件结构**：
```assembly
# 第 8-42 行：实际 GPU 指令（31 条）
_Z10vector_addPKfS0_Pfi:
	s_load_b32 s2, s[0:1], 0x2c
	...
	s_endpgm

# 第 43-223 行：元数据（不是实际指令）
.amdhsa_kernel _Z10vector_addPKfS0_Pfi
	.amdhsa_next_free_vgpr 6        # 使用 6 个 VGPR
	.amdhsa_next_free_sgpr 8        # 使用 8 个 SGPR
	...
```

**rocgdb 只显示实际指令**，不包含元数据，但包含调试版本的额外代码。

---

### 实验验证

#### 实验 1: 对比优化等级的影响

```bash
cd /home/yafan/hip_learning/01_vector_add

# 编译 -O0 版本
hipcc -g -O0 -o vector_add_O0 vector_add.cpp

# 编译 -O3 版本
hipcc -g -O3 -o vector_add_O3 vector_add.cpp

# 在 rocgdb 中对比
rocgdb ./vector_add_O0
(gdb) break vector_add
(gdb) run
(gdb) set pagination off
(gdb) disassemble | wc -l
# 可能输出: 100-150 行

rocgdb ./vector_add_O3
(gdb) break vector_add
(gdb) run
(gdb) disassemble | wc -l
# 可能输出: 40-50 行
```

#### 实验 2: 查看实际指令数

```bash
# .s 文件中的指令数
grep -E "^\s+(s_|v_|global_)" vector_add-hip-amdgcn-amd-amdhsa-gfx1201.s | wc -l
# 输出: 31

# rocgdb 中的指令数（需要在调试会话中）
(gdb) break vector_add
(gdb) run
(gdb) disassemble | grep -E "^\s+0x" | wc -l
# 可能输出: 80-150（取决于优化）
```

---

### 使用建议

#### 学习 GPU 汇编时 → 查看 .s 文件

```bash
hipcc -S -O2 -o kernel.s kernel.cpp
less kernel.s
```

**优点**：
- ✅ 代码简洁，易于理解
- ✅ 反映编译器优化策略
- ✅ 可以用编辑器搜索、标注
- ✅ 查看寄存器使用、元数据

#### 调试运行时问题 → 使用 rocgdb

```bash
rocgdb ./program
(gdb) break kernel
(gdb) run
(gdb) disassemble /m     # 混合源代码和汇编（推荐）
(gdb) info registers
(gdb) x/10f array
```

**优点**：
- ✅ 查看实际执行的代码
- ✅ 查看寄存器和内存值
- ✅ 单步执行
- ✅ 源代码对应关系

#### 性能分析 → 查看优化后的 .s

```bash
hipcc -S -O3 kernel.cpp
# 分析:
# - 指令数量（越少越好）
# - 内存访问次数（global_load/store）
# - 寄存器使用（.amdhsa_next_free_vgpr）
```

---

### 关键理解

1. **`.s` 文件 ≠ 最终执行代码**
   - `.s` 是编译器的中间输出
   - 链接器可能插入额外代码（运行时初始化、调试钩子）

2. **-O0 vs -O3 差异巨大**
   ```
   -O0: 可能 150 条指令（未优化）
   -O3: 可能 35 条指令（优化后）
   ```

3. **两者结合使用效果最好**
   - `.s` 文件 → 理解编译器意图
   - rocgdb → 调试实际运行问题

---

最后更新：2026-03-11
测试环境：ROCm 7.2.26015, hipcc/clang 18.0, AMD gfx1036/gfx1201
