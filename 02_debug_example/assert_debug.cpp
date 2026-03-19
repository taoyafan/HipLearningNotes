/*
 * GPU Assert 调试示例
 *
 * 学习目标：
 * 1. 如何在 kernel 中使用 assert() 进行运行时检查
 * 2. 理解 GPU assert 失败时的行为
 * 3. 使用 assert 捕获常见错误（越界、非法值、逻辑错误）
 *
 * 编译方法：
 *   hipcc -g -O0 -o assert_debug assert_debug.cpp
 *
 * 运行：
 *   ./assert_debug
 *
 * 注意：Assert 只在 Debug 模式有效（-g -O0），Release 模式会被优化掉
 */

#include <hip/hip_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <cassert>  // GPU assert 支持

// 错误检查宏
#define HIP_CHECK(call) \
    do { \
        hipError_t err = call; \
        if (err != hipSuccess) { \
            printf("HIP Error: %s at %s:%d\n", \
                   hipGetErrorString(err), __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

// ==================== 示例 1: 边界检查 ====================
// 这是最常见的 assert 用法：检查数组越界
__global__ void kernel_with_bounds_check(int* data, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Assert 检查：确保不会越界访问
    assert(idx < N && "Thread index out of bounds!");

    // 如果上面的 assert 通过，这里的访问是安全的
    data[idx] = idx * 2;
}

// ==================== 示例 2: 数值范围检查 ====================
// 检查计算结果是否在合理范围内
__global__ void kernel_with_value_check(float* input, float* output, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < N) {
        float value = input[idx];

        // Assert 检查：输入值必须非负
        assert(value >= 0.0f && "Input value must be non-negative!");

        // 计算平方根
        output[idx] = sqrtf(value);

        // Assert 检查：结果不能是 NaN
        assert(!isnan(output[idx]) && "Output is NaN!");
    }
}

// ==================== 示例 3: 逻辑错误检查 ====================
// 检查共享内存的使用是否正确
__global__ void kernel_with_logic_check(int* data, int N) {
    __shared__ int shared_data[256];  // 假设 blockDim.x <= 256

    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int tid = threadIdx.x;

    // Assert 检查：确保 block size 不超过共享内存大小
    assert(blockDim.x <= 256 && "Block size exceeds shared memory allocation!");

    if (idx < N) {
        // 加载数据到共享内存
        shared_data[tid] = data[idx];
        __syncthreads();

        // 简单的操作：使用相邻线程的数据
        if (tid > 0) {
            data[idx] = shared_data[tid] + shared_data[tid - 1];
        }
    }
}

// ==================== 示例 4: 原子操作检查 ====================
// 检查原子操作的结果
__global__ void kernel_with_atomic_check(int* counter, int* results, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < N) {
        // 获取唯一的索引
        int my_index = atomicAdd(counter, 1);

        // Assert 检查：原子操作返回的索引必须有效
        assert(my_index >= 0 && my_index < N && "Atomic counter out of range!");

        // 使用这个索引
        results[my_index] = idx;
    }
}

// ==================== 示例 5: 故意触发 Assert 失败 ====================
// 这个函数演示 assert 失败时的行为
__global__ void kernel_with_intentional_failure(int* data, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < N) {
        data[idx] = idx;

        // 故意让某个线程触发 assert
        // 这个 assert 会失败！
        assert(idx != 100 && "Intentional assertion failure at thread 100!");
    }
}

// ==================== 测试函数 ====================

void test_bounds_check() {
    printf("\n=== 测试 1: 边界检查 ===\n");

    const int N = 1024;
    int *d_data;

    HIP_CHECK(hipMalloc(&d_data, N * sizeof(int)));

    // 正确的配置：线程数等于数据大小
    int threads = 256;
    int blocks = (N + threads - 1) / threads;

    printf("启动 kernel: %d blocks x %d threads = %d threads\n", blocks, threads, blocks * threads);
    printf("数据大小: %d\n", N);

    kernel_with_bounds_check<<<blocks, threads>>>(d_data, N);
    HIP_CHECK(hipDeviceSynchronize());

    printf("✅ 边界检查通过\n");

    HIP_CHECK(hipFree(d_data));
}

void test_value_check() {
    printf("\n=== 测试 2: 数值范围检查 ===\n");

    const int N = 100;
    float *h_input = (float*)malloc(N * sizeof(float));
    float *h_output = (float*)malloc(N * sizeof(float));
    float *d_input, *d_output;

    // 初始化：都是正数
    for (int i = 0; i < N; i++) {
        h_input[i] = (float)i;
    }

    HIP_CHECK(hipMalloc(&d_input, N * sizeof(float)));
    HIP_CHECK(hipMalloc(&d_output, N * sizeof(float)));
    HIP_CHECK(hipMemcpy(d_input, h_input, N * sizeof(float), hipMemcpyHostToDevice));

    kernel_with_value_check<<<1, 256>>>(d_input, d_output, N);
    HIP_CHECK(hipDeviceSynchronize());

    HIP_CHECK(hipMemcpy(h_output, d_output, N * sizeof(float), hipMemcpyDeviceToHost));

    printf("✅ 数值检查通过 (前5个结果: %.2f, %.2f, %.2f, %.2f, %.2f)\n",
           h_output[0], h_output[1], h_output[2], h_output[3], h_output[4]);

    free(h_input);
    free(h_output);
    HIP_CHECK(hipFree(d_input));
    HIP_CHECK(hipFree(d_output));
}

void test_logic_check() {
    printf("\n=== 测试 3: 逻辑错误检查 ===\n");

    const int N = 512;
    int *d_data;

    HIP_CHECK(hipMalloc(&d_data, N * sizeof(int)));

    // 使用 256 个线程（符合共享内存大小）
    kernel_with_logic_check<<<2, 256>>>(d_data, N);
    HIP_CHECK(hipDeviceSynchronize());

    printf("✅ 逻辑检查通过 (block size = 256)\n");

    HIP_CHECK(hipFree(d_data));
}

void test_intentional_failure() {
    printf("\n=== 测试 4: 故意触发 Assert 失败 ===\n");
    printf("警告：这个测试会导致程序异常退出！\n");
    printf("如果您想看到 assert 失败的效果，取消下面代码的注释\n\n");

    // 取消注释以下代码来看 assert 失败的效果：
    const int N = 200;
    int *d_data;

    HIP_CHECK(hipMalloc(&d_data, N * sizeof(int)));

    kernel_with_intentional_failure<<<1, 256>>>(d_data, N);
    HIP_CHECK(hipDeviceSynchronize());  // 这里会捕获到 assert 失败

    HIP_CHECK(hipFree(d_data));

    printf("(跳过故意失败测试)\n");
}

// ==================== 主函数 ====================

int main() {
    printf("========================================\n");
    printf("    GPU Assert 调试示例\n");
    printf("========================================\n");

    // 获取 GPU 信息
    int device;
    hipDeviceProp_t prop;
    HIP_CHECK(hipGetDevice(&device));
    HIP_CHECK(hipGetDeviceProperties(&prop, device));
    printf("使用 GPU: %s\n", prop.name);

    // 运行所有测试
    test_bounds_check();
    test_value_check();
    test_logic_check();
    test_intentional_failure();

    printf("\n========================================\n");
    printf("    所有测试完成！\n");
    printf("========================================\n");
    printf("\n💡 Assert 使用技巧：\n");
    printf("   1. 只在 Debug 模式启用（-g -O0) \n");
    printf("   2. 检查边界、数值范围、逻辑条件\n");
    printf("   3. 失败时会终止 kernel 执行\n");
    printf("   4. 错误信息包含文件名和行号\n");
    printf("   5. Release 模式会被优化掉，不影响性能\n");

    return 0;
}
