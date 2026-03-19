# rocgdb 快速参考

## ⚠️ 重要说明

**rocgdb 的 GPU 调试功能取决于您的 ROCm 版本！**

本文档提供两种使用方式：
1. **终端直接使用 rocgdb**（推荐初学者）
2. **VS Code Debug Console**（需要 `-exec` 前缀）

---

## 📌 基础命令

### 标准 GDB 命令
```gdb
# 终端 rocgdb 中：
(gdb) info threads              # 查看所有线程（CPU + GPU）

# VS Code Debug Console 中：
-exec info threads

# 切换线程
(gdb) thread 2                  # 切换到线程 2
```

### ✅ AMD ROCm GPU 特定命令（已验证可用）

```gdb
# 查看 GPU 相关信息
(gdb) info agents               # 显示当前活动的异构代理（GPU设备）
(gdb) info dispatches           # 显示当前活动的异构调度
(gdb) info lanes                # 显示当前已知的 lanes（GPU 线程）
(gdb) info queues               # 显示当前活动的异构队列

# VS Code 中需要加 -exec 前缀：
-exec info agents
-exec info dispatches
-exec info lanes
-exec info queues
```

### ❌ 不存在的命令（不要使用）

```gdb
# ❌ 以下命令在标准 rocgdb 中不存在：
(gdb) info rocm threads         # 错误：Undefined info command
(gdb) rocm thread (0,0):(5,0)   # 不存在
```

---

## 🔍 查看变量

### 打印变量
```gdb
# 终端 rocgdb 中：
(gdb) print idx
(gdb) print data[0]

# VS Code Debug Console 中：
-exec print idx
-exec print data[0]

# 打印 GPU 内置变量（如果支持）
(gdb) print threadIdx.x
(gdb) print blockIdx.x
(gdb) print blockDim.x

# 打印数组范围
(gdb) print data[0]@10    # 打印 data[0] 到 data[9]
```

### 查看内存（x 命令）

**基本格式**: `x/[数量][格式][大小] [地址]`

```gdb
# 查看指令（最常用）
-exec x/20i $pc           # 从当前位置显示 20 条汇编指令
-exec x/10i vector_add    # 从函数开始显示 10 条指令

# 查看内存（十六进制）
-exec x/10xw data         # 查看 10 个 word（4字节，十六进制）
-exec x/64xb data         # 查看 64 个 byte（1字节，十六进制）
-exec x/8xg $sp           # 查看 8 个 giant（8字节，十六进制）

# 查看内存（十进制）
-exec x/10dw data         # 十进制 word
-exec x/10d data          # 十进制（默认大小）

# 查看浮点数
-exec x/10f d_A           # 查看 10 个浮点数
-exec x/256fw 0x7fff...   # 查看 256 个浮点 word

# 查看字符串
-exec x/s 0x404000        # 查看字符串

# 查看二进制
-exec x/8t data           # 以二进制格式显示

# 查看地址
-exec x/10a $sp           # 显示 10 个地址
```

#### 格式选项速查表

| 格式 | 说明 | 示例 |
|------|------|------|
| **i** | 指令（instruction） | `x/20i $pc` |
| **x** | 十六进制 | `x/10xw addr` |
| **d** | 十进制 | `x/10dw addr` |
| **f** | 浮点数 | `x/10f addr` |
| **s** | 字符串 | `x/s addr` |
| **c** | 字符 | `x/10c addr` |
| **t** | 二进制 | `x/8t addr` |
| **a** | 地址 | `x/10a $sp` |

#### 大小选项速查表

| 大小 | 说明 | 字节数 |
|------|------|--------|
| **b** | byte | 1 |
| **h** | halfword | 2 |
| **w** | word | 4 |
| **g** | giant | 8 |

#### 特殊寄存器

| 寄存器 | 说明 |
|--------|------|
| `$pc` | 程序计数器（当前指令位置） |
| `$sp` | 栈指针 |
| `$v0-$v255` | VGPR（AMD GPU 向量寄存器） |
| `$s0-$s103` | SGPR（AMD GPU 标量寄存器） |

### 查看所有局部变量
```gdb
-exec info locals
-exec info args           # 查看函数参数
```

---

## 🎮 执行控制

### 断点管理
```gdb
# 查看所有断点
-exec info breakpoints

# 在函数设置断点
-exec break kernel_name

# 在特定行设置断点
-exec break thread_debug.cpp:42

# 删除断点
-exec delete 1            # 删除断点 #1
-exec clear              # 删除当前行的断点
```

### 单步执行（C++ 源码级别）
```gdb
# 执行下一行（不进入函数）
-exec next
-exec n

# 执行下一行（进入函数）
-exec step
-exec s

# 继续运行到下一个断点
-exec continue
-exec c

# 运行到当前函数返回
-exec finish
```

### 单步执行（ISA 汇编级别）⭐
```gdb
# 执行一条 ISA 汇编指令（不进入函数调用）
-exec nexti
-exec ni

# 执行一条 ISA 汇编指令（进入函数调用）
-exec stepi
-exec si

# 执行 N 条汇编指令
-exec stepi 5        # 执行 5 条指令

# 查看当前指令
-exec x/i $pc        # 显示当前即将执行的指令
-exec x/10i $pc      # 显示接下来 10 条指令
```

**区别**：
- `next`/`step`: 执行一行 **C++ 源代码**（可能对应多条 ISA 指令）
- `nexti`/`stepi`: 执行一条 **ISA 汇编指令**（GPU 硬件级别）

---

## 📊 调试信息

### 查看调用栈
```gdb
# 查看完整调用栈
-exec backtrace
-exec bt

# 查看调用栈的某一帧
-exec frame 0
-exec frame 1
```

### 查看源代码
```gdb
# 查看当前位置的源代码
-exec list

# 查看特定行
-exec list 42

# 查看函数代码
-exec list kernel_name
```

### 查看寄存器
```gdb
# 查看所有寄存器（包括 CPU 和 GPU）
-exec info registers

# 查看特定寄存器
-exec info registers v0      # VGPR 0
-exec info registers s0      # SGPR 0
-exec info registers exec    # 执行掩码
-exec info registers pc      # 程序计数器
```

### 反汇编 GPU 代码
```gdb
# 反汇编当前函数（显示 GPU ISA）
-exec disassemble

# 混合显示源代码和汇编
-exec disassemble /m

# 显示原始机器码
-exec disassemble /r

# 指定地址范围（限制输出）
-exec disassemble $pc, $pc+100      # 从当前位置，显示接下来 100 字节
-exec disassemble $pc, +50          # 简写形式

# 使用 x 命令查看指令（推荐，可精确控制行数）
-exec x/10i $pc                     # 显示 10 条指令
-exec x/20i $pc                     # 显示 20 条指令
-exec x/5i kernel_name              # 从函数开始显示 5 条指令
```

**按回车重复**：
```gdb
-exec x/10i $pc
... 显示 10 条指令 ...
[Enter]           # 自动继续显示接下来的 10 条！
... 又显示 10 条 ...
```

---

## 🎯 GPU 调试命令详解

### 查看 GPU 设备信息

```gdb
# 查看所有 GPU 代理（devices）
(gdb) info agents

# 示例输出：
# Agent 1: CPU
# Agent 2: gfx906 [AMD Radeon VII]
```

### 查看 GPU 执行状态

```gdb
# 在 kernel 运行时查看
(gdb) info dispatches           # 当前的 GPU 调度
(gdb) info queues               # 命令队列状态
(gdb) info lanes                # GPU 线程（lanes）信息
```

### 查看 GPU 变量

```gdb
# 在 kernel 断点处，尝试打印 GPU 变量
(gdb) print idx                 # 局部变量（通常可用）
(gdb) print data[idx]           # 数组访问（通常可用）

# GPU 内置变量（可能需要手动计算）
(gdb) print threadIdx.x         # 可能不可用
(gdb) print blockIdx.x          # 可能不可用

# 如果内置变量不可用，在 kernel 中手动保存：
# int tid = threadIdx.x;
# int bid = blockIdx.x;
# 然后可以打印：
(gdb) print tid
(gdb) print bid
```

---

## 💡 实用技巧

### 条件断点
```gdb
# 只在特定条件下中断
-exec break kernel_name if idx == 100
-exec break thread_debug.cpp:42 if threadIdx.x == 5
```

### 监视点（数据断点）
```gdb
# 当变量改变时中断
-exec watch variable_name

# 当内存地址的值改变时中断
-exec watch *0x12345678
```

### 自动化命令
```gdb
# 每次停止时自动执行命令
-exec define hook-stop
> info rocm threads
> print idx
> end
```

### 保存断点
```gdb
# 保存当前所有断点
-exec save breakpoints breakpoints.txt

# 加载断点
-exec source breakpoints.txt
```

---

## 🐛 常见问题解决

### 问题 1: `Undefined info command: "rocm threads"`
**原因**: 您的 rocgdb 版本不支持 `info rocm` 命令
**解决**:
1. 使用标准 GDB 命令：`info threads`
2. 检查 rocgdb 版本：`rocgdb --version`
3. 查看可用命令：`help info`

### 问题 2: `-var-create: unable to create variable object`
**原因**: 程序还没运行到 GPU 代码（VS Code）
**解决**: 先在 `__global__` 函数内设置断点，等程序运行到那里

### 问题 3: `Cannot access memory at address`
**原因**: 尝试访问无效的 GPU 内存地址
**解决**: 检查指针是否有效，数组索引是否越界

### 问题 4: 看不到变量值
**原因**: 优化导致变量被优化掉
**解决**: 使用 `-g -O0` 编译（已在 tasks.json 中配置）

### 问题 5: GPU 内置变量打印不出来
**原因**: rocgdb 可能不完全支持 GPU 变量
**解决**:
1. 在 kernel 中手动计算并保存到局部变量
2. 使用 `printf` 在 kernel 内打印

---

## 📖 实战演示

### 场景 1: 基础调试流程

```gdb
# 1. 编译（带调试符号）
$ cd 02_debug_example
$ hipcc -g -O0 -o thread_debug thread_debug.cpp

# 2. 启动 rocgdb
$ rocgdb ./thread_debug

# 3. 在 kernel 内设置断点
(gdb) break simple_kernel
(gdb) run

# 4. 程序在断点处暂停后
(gdb) info threads          # 查看所有线程
(gdb) print idx             # 查看当前线程的 idx
(gdb) print data[idx]       # 查看数据
(gdb) next                  # 单步执行
(gdb) continue              # 继续运行
```

### 场景 2: 查找数组越界

```gdb
# 在可能越界的位置设置断点
(gdb) break kernel_name
(gdb) run

# 检查索引
(gdb) print idx
(gdb) print N
(gdb) print idx < N              # 应该是 true

# 如果发现 idx >= N，找到 bug！
```

### 场景 3: 使用条件断点

```gdb
# 只在特定条件下中断（比如 idx == 100）
(gdb) break kernel_name if idx == 100
(gdb) run

# 或者先设置普通断点，再添加条件
(gdb) break kernel_name
(gdb) condition 1 idx == 100
```

### 场景 4: 查看 GPU 汇编和内存

```gdb
# 在 kernel 断点处
(gdb) break vector_add
(gdb) run

# 查看即将执行的 GPU 汇编指令
(gdb) x/20i $pc
# 输出: AMD GPU ISA 指令（v_add_f32, global_load_dword 等）

# 查看 GPU 数组数据（十六进制）
(gdb) print d_A
$1 = (float *) 0x7ffff3400000
(gdb) x/10xw 0x7ffff3400000

# 查看 GPU 数组数据（浮点数）
(gdb) x/10f 0x7ffff3400000

# 对比 kernel 执行前后的数据
(gdb) x/10f d_C              # 执行前
(gdb) next                    # 执行一步
(gdb) x/10f d_C              # 执行后，查看变化
```

### 场景 5: 使用 x 命令的技巧

```gdb
# 重复上一个 x 命令（按回车）
(gdb) x/20i $pc
... 显示 20 条指令 ...
(gdb) [Enter]                 # 继续显示接下来的 20 条
... 又显示 20 条 ...

# 结合 print 使用
(gdb) print &A[100]
$1 = (float *) 0x7ffff3400190
(gdb) x/10f $1                # 使用 $1 引用之前的结果

# 查看同一内存，不同格式
(gdb) x/4xw 0x7fff...         # 十六进制
(gdb) x/4f 0x7fff...          # 浮点数
(gdb) x/16xb 0x7fff...        # 字节
```

---

## 🔗 相关资源

- [ROCm Debug Guide](https://rocm.docs.amd.com/projects/ROCgdb/en/latest/)
- [GDB Manual](https://sourceware.org/gdb/documentation/)
- 项目文档: [02_debug_example/README.md](README.md)

---

## 💻 VS Code 快捷键

| 快捷键 | 功能 |
|--------|------|
| `F5` | 开始调试 / 继续执行 |
| `F9` | 设置/取消断点 |
| `F10` | 单步跳过（next） |
| `F11` | 单步进入（step） |
| `Shift+F11` | 跳出当前函数（finish） |
| `Shift+F5` | 停止调试 |
| `Ctrl+Shift+F5` | 重启调试 |

---

## 📝 备注

### 重要提醒
1. **rocgdb 的 GPU 调试功能有限**，不如 NVIDIA 的 cuda-gdb 完善
2. 某些 GPU 特定命令（如 `info rocm threads`）在标准版本中不存在
3. 推荐使用**标准 GDB 命令** + **kernel 内 printf** 组合调试
4. 对于复杂的 GPU 调试，考虑使用 `assert()` 和性能分析工具

### 配置说明
- **VS Code**: 命令需要 `-exec` 前缀
- **终端 rocgdb**: 不需要 `-exec`
- **环境变量**: `HSA_ENABLE_DEBUG=1`（已在 launch.json 配置）
- **编译选项**: `-g -O0`（已在 tasks.json 配置）

### 推荐调试方法
1. **基础调试**: rocgdb + 标准 GDB 命令
2. **运行时检查**: 在 kernel 中使用 `assert()`
3. **打印调试**: 在 kernel 中使用 `printf()`
4. **性能问题**: 使用 `rocprof` 和 `Omniperf`
