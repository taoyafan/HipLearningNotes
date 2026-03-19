# 调试示例目录

本目录包含 HIP GPU 调试相关的示例代码。

## 📁 文件列表

### 1. `debug_demo.cpp` ✅
**主题**: 竞态条件和原子操作

**学习内容**:
- 演示 GPU 竞态条件问题
- 使用原子操作解决数据冲突
- 错误检查宏的使用

**编译运行**:
```bash
hipcc -o debug_demo debug_demo.cpp
./debug_demo
```

---

### 2. `thread_debug.cpp` ✅
**主题**: 调试器断点演示

**学习内容**:
- 使用 rocgdb 调试 GPU 代码
- 设置断点和单步执行
- 查看 GPU 线程状态

**编译运行**:
```bash
hipcc -g -O0 -o thread_debug thread_debug.cpp
rocgdb ./thread_debug
```

**常用 rocgdb 命令**:
```gdb
(gdb) break kernel_name     # 在 kernel 入口设断点
(gdb) run                   # 运行程序
(gdb) info rocm threads     # 查看所有 GPU 线程
(gdb) rocm thread (0,0):(1,5)  # 切换到特定线程 (block, thread)
(gdb) print variable_name   # 打印变量值
(gdb) next                  # 单步执行
(gdb) continue              # 继续执行
```

---

### 3. `assert_debug.cpp` 🆕
**主题**: GPU Assert 运行时检查

**学习内容**:
- 在 kernel 中使用 `assert()` 进行运行时检查
- 边界检查、数值范围检查
- 逻辑错误检查
- Assert 失败时的行为

**编译运行**:
```bash
# 编译（Debug 模式，assert 有效）
hipcc -g -O0 -o assert_debug assert_debug.cpp
./assert_debug

# 编译（Release 模式，assert 被优化掉）
hipcc -O3 -o assert_debug_release assert_debug.cpp
./assert_debug_release
```

**示例代码**:
```cpp
__global__ void kernel(int* data, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    // 边界检查
    assert(idx < N && "Thread index out of bounds!");

    // 数值检查
    assert(data[idx] >= 0 && "Negative value detected!");

    data[idx] = idx * 2;
}
```

---

### 4. `sanitizer_demo.cpp` 🆕
**主题**: Compute Sanitizer 内存检查

**学习内容**:
- 检测内存越界访问
- 检测未初始化内存使用
- 检测竞态条件
- 检测内存泄漏

**编译运行**:
```bash
# 编译
hipcc -g -O0 -o sanitizer_demo sanitizer_demo.cpp

# 正常运行
./sanitizer_demo

# 使用 rocgdb 检查（AMD ROCm 平台）
rocgdb ./sanitizer_demo
```

**常见错误类型**:

| 错误类型 | 说明 | 检测方法 |
|---------|------|---------|
| 数组越界 | 访问 `data[N]` 而不是 `data[N-1]` | 运行时崩溃或随机结果 |
| 未初始化内存 | 使用未 `hipMemcpy` 的 GPU 内存 | 随机数据 |
| 竞态条件 | 多线程写同一位置，无原子操作 | 结果不确定 |
| 内存泄漏 | `hipMalloc` 后没有 `hipFree` | 内存耗尽 |

---

## 🛠️ ROCm 调试工具对比

### AMD ROCm vs NVIDIA CUDA

| 工具类型 | CUDA (NVIDIA) | ROCm (AMD) |
|---------|---------------|------------|
| 调试器 | cuda-gdb | rocgdb |
| 内存检查 | compute-sanitizer --tool memcheck | rocgdb + 手动检查 |
| 竞态检测 | compute-sanitizer --tool racecheck | 手动检查 + assert |
| 性能分析 | Nsight Compute | rocprof, Omniperf |
| API 追踪 | Nsight Systems | roctracer |

**⚠️ 注意**: AMD ROCm 的 sanitizer 工具不如 NVIDIA CUDA 完善，主要依靠：
- `rocgdb` 调试器
- `assert()` 运行时检查
- 手动代码审查
- 性能分析工具间接发现问题

---

## 📚 调试最佳实践

### 1. **编译时使用调试标志**
```bash
hipcc -g -O0 -lineinfo your_code.cpp
```
- `-g`: 生成调试符号
- `-O0`: 禁用优化（方便调试）
- `-lineinfo`: 包含行号信息

### 2. **使用错误检查宏**
```cpp
#define HIP_CHECK(call) \
    do { \
        hipError_t err = call; \
        if (err != hipSuccess) { \
            printf("HIP Error: %s at %s:%d\n", \
                   hipGetErrorString(err), __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

// 使用
HIP_CHECK(hipMalloc(&d_data, size));
```

### 3. **Kernel 内打印调试**
```cpp
if (threadIdx.x == 0 && blockIdx.x == 0) {
    printf("[DEBUG] value = %d\n", value);
}
```

### 4. **使用 Assert 检查假设**
```cpp
assert(idx < N && "Index out of bounds!");
assert(value >= 0 && "Value must be non-negative!");
```

### 5. **边界检查**
```cpp
// ✅ 正确
if (idx < N) {
    data[idx] = value;
}

// ❌ 错误
if (idx <= N) {
    data[idx] = value;  // 当 idx == N 时越界！
}
```

### 6. **原子操作避免竞态**
```cpp
// ❌ 错误：竞态条件
result[0] += value;

// ✅ 正确：使用原子操作
atomicAdd(&result[0], value);
```

---

## 🚀 下一步学习

完成调试示例后，建议继续学习：

1. **性能分析** (`../03_profiling_and_perf/`)
   - 使用 rocprof 分析性能
   - 内存带宽测试
   - 占用率优化

2. **并行算法** (`../04_compute_algorithms/`)
   - Reduction（归约）
   - Scan（前缀和）
   - 并行排序

3. **实战项目**
   - 图像处理
   - 科学计算
   - 深度学习基础

---

## 📖 参考资源

- [ROCm Documentation](https://rocm.docs.amd.com/)
- [HIP Programming Guide](https://rocm.docs.amd.com/projects/HIP/en/latest/)
- [rocgdb User Guide](https://rocm.docs.amd.com/projects/ROCgdb/en/latest/)
- [GPU Assert in CUDA](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#assertions) (概念类似)
