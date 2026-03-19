/**
 * coalescing_test.cpp - 内存合并访问测试
 *
 * 演示不同内存访问模式对带宽的影响：
 * 1. 合并访问 (Coalesced) - 连续访问
 * 2. 跨步访问 (Strided) - stride = 2, 4, 8, 16, 32
 * 3. 随机访问 (Random)
 *
 * 编译: hipcc -O3 -o coalescing_test coalescing_test.cpp
 * 运行: ./coalescing_test
 */

#include <hip/hip_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <random>

// 错误检查宏
#define HIP_CHECK(call)                                                 \
    do {                                                                \
        hipError_t err = call;                                          \
        if (err != hipSuccess) {                                        \
            printf("HIP Error: %s at %s:%d\n",                          \
                   hipGetErrorString(err), __FILE__, __LINE__);         \
            exit(1);                                                    \
        }                                                               \
    } while (0)

// 测试参数
constexpr size_t ARRAY_SIZE = 64 * 1024 * 1024;  // 64M 元素 = 256 MB
constexpr int NUM_ITERATIONS = 10;

// ============================================================================
// Kernel 定义
// ============================================================================

/**
 * 合并访问 - 最优模式
 * 相邻线程访问相邻内存地址
 */
__global__ void kernel_coalesced(const float* __restrict__ input,
                                  float* __restrict__ output,
                                  size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;

    for (size_t i = idx; i < n; i += stride) {
        output[i] = input[i] * 2.0f;
    }
}

/**
 * 跨步访问 - 每个线程跳过 STRIDE 个元素
 * STRIDE 越大，性能越差
 */
template <int STRIDE>
__global__ void kernel_strided(const float* __restrict__ input,
                                float* __restrict__ output,
                                size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;

    // 每个线程处理 index * STRIDE 位置的元素
    for (size_t i = idx; i < n / STRIDE; i += stride) {
        size_t actual_idx = i * STRIDE;
        if (actual_idx < n) {
            output[actual_idx] = input[actual_idx] * 2.0f;
        }
    }
}

/**
 * 随机访问 - 通过索引数组间接访问
 */
__global__ void kernel_random(const float* __restrict__ input,
                               float* __restrict__ output,
                               const int* __restrict__ indices,
                               size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;

    for (size_t i = idx; i < n; i += stride) {
        int rand_idx = indices[i];
        output[i] = input[rand_idx] * 2.0f;
    }
}

// ============================================================================
// 辅助函数
// ============================================================================

class GpuTimer {
public:
    GpuTimer() {
        HIP_CHECK(hipEventCreate(&start_));
        HIP_CHECK(hipEventCreate(&stop_));
    }

    ~GpuTimer() {
        hipEventDestroy(start_);
        hipEventDestroy(stop_);
    }

    void start() {
        HIP_CHECK(hipEventRecord(start_, 0));
    }

    void stop() {
        HIP_CHECK(hipEventRecord(stop_, 0));
        HIP_CHECK(hipEventSynchronize(stop_));
    }

    float elapsed_ms() {
        float ms;
        HIP_CHECK(hipEventElapsedTime(&ms, start_, stop_));
        return ms;
    }

private:
    hipEvent_t start_, stop_;
};

void print_header() {
    hipDeviceProp_t prop;
    HIP_CHECK(hipGetDeviceProperties(&prop, 0));

    printf("========================================\n");
    printf("    内存合并访问测试\n");
    printf("========================================\n");
    printf("GPU: %s\n", prop.name);

    double theoretical_bandwidth =
        (prop.memoryClockRate * 1000.0) *
        (prop.memoryBusWidth / 8.0) *
        2.0 / 1e9;

    printf("理论带宽: %.1f GB/s\n", theoretical_bandwidth);
    printf("数组大小: %zu MB\n", ARRAY_SIZE * sizeof(float) / (1024 * 1024));
    printf("========================================\n\n");
}

float measure_bandwidth(float time_ms, size_t elements) {
    // 读 + 写 = 2 倍数据量
    size_t bytes = elements * sizeof(float) * 2;
    return (bytes / 1e9) / (time_ms / 1000.0);  // GB/s
}

// ============================================================================
// 测试函数
// ============================================================================

void test_coalesced(float* d_input, float* d_output, size_t n) {
    printf("=== 测试 1: 合并访问 (Coalesced) ===\n");

    int blockSize = 256;
    int gridSize = min((int)((n + blockSize - 1) / blockSize), 65536);

    // Warmup
    kernel_coalesced<<<gridSize, blockSize>>>(d_input, d_output, n);
    HIP_CHECK(hipDeviceSynchronize());

    // 测量
    GpuTimer timer;
    timer.start();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        kernel_coalesced<<<gridSize, blockSize>>>(d_input, d_output, n);
    }
    timer.stop();

    float avg_time_ms = timer.elapsed_ms() / NUM_ITERATIONS;
    float bandwidth = measure_bandwidth(avg_time_ms, n);

    printf("  访问模式: 连续 (stride = 1)\n");
    printf("  平均时间: %.3f ms\n", avg_time_ms);
    printf("  有效带宽: %.2f GB/s\n\n", bandwidth);
}

template <int STRIDE>
void test_strided(float* d_input, float* d_output, size_t n) {
    printf("=== 测试: 跨步访问 (Stride = %d) ===\n", STRIDE);

    int blockSize = 256;
    int gridSize = min((int)((n / STRIDE + blockSize - 1) / blockSize), 65536);

    // Warmup
    kernel_strided<STRIDE><<<gridSize, blockSize>>>(d_input, d_output, n);
    HIP_CHECK(hipDeviceSynchronize());

    // 测量
    GpuTimer timer;
    timer.start();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        kernel_strided<STRIDE><<<gridSize, blockSize>>>(d_input, d_output, n);
    }
    timer.stop();

    float avg_time_ms = timer.elapsed_ms() / NUM_ITERATIONS;
    // 跨步访问只处理 n/STRIDE 个元素
    float bandwidth = measure_bandwidth(avg_time_ms, n / STRIDE);

    printf("  访问模式: 每隔 %d 个元素\n", STRIDE);
    printf("  平均时间: %.3f ms\n", avg_time_ms);
    printf("  有效带宽: %.2f GB/s\n\n", bandwidth);
}

void test_random(float* d_input, float* d_output, int* d_indices, size_t n) {
    printf("=== 测试: 随机访问 (Random) ===\n");

    int blockSize = 256;
    int gridSize = min((int)((n + blockSize - 1) / blockSize), 65536);

    // Warmup
    kernel_random<<<gridSize, blockSize>>>(d_input, d_output, d_indices, n);
    HIP_CHECK(hipDeviceSynchronize());

    // 测量
    GpuTimer timer;
    timer.start();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        kernel_random<<<gridSize, blockSize>>>(d_input, d_output, d_indices, n);
    }
    timer.stop();

    float avg_time_ms = timer.elapsed_ms() / NUM_ITERATIONS;
    float bandwidth = measure_bandwidth(avg_time_ms, n);

    printf("  访问模式: 完全随机\n");
    printf("  平均时间: %.3f ms\n", avg_time_ms);
    printf("  有效带宽: %.2f GB/s\n\n", bandwidth);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    print_header();

    size_t n = ARRAY_SIZE;
    size_t size = n * sizeof(float);

    // 分配 GPU 内存
    float *d_input, *d_output;
    int* d_indices;
    HIP_CHECK(hipMalloc(&d_input, size));
    HIP_CHECK(hipMalloc(&d_output, size));
    HIP_CHECK(hipMalloc(&d_indices, n * sizeof(int)));

    // 初始化输入数据
    float* h_input = (float*)malloc(size);
    for (size_t i = 0; i < n; i++) {
        h_input[i] = 1.0f;
    }
    HIP_CHECK(hipMemcpy(d_input, h_input, size, hipMemcpyHostToDevice));
    free(h_input);

    // 生成随机索引
    printf("生成随机索引数组...\n");
    int* h_indices = (int*)malloc(n * sizeof(int));
    for (size_t i = 0; i < n; i++) {
        h_indices[i] = i;
    }
    // Fisher-Yates shuffle
    std::random_device rd;
    std::mt19937 g(rd());
    for (size_t i = n - 1; i > 0; i--) {
        std::uniform_int_distribution<size_t> dist(0, i);
        size_t j = dist(g);
        std::swap(h_indices[i], h_indices[j]);
    }
    HIP_CHECK(hipMemcpy(d_indices, h_indices, n * sizeof(int), hipMemcpyHostToDevice));
    free(h_indices);
    printf("完成。\n\n");

    // 运行测试
    test_coalesced(d_input, d_output, n);

    printf("========================================\n");
    printf("    跨步访问测试\n");
    printf("========================================\n\n");

    test_strided<2>(d_input, d_output, n);
    test_strided<4>(d_input, d_output, n);
    test_strided<8>(d_input, d_output, n);
    test_strided<16>(d_input, d_output, n);
    test_strided<32>(d_input, d_output, n);

    printf("========================================\n");
    printf("    随机访问测试\n");
    printf("========================================\n\n");

    test_random(d_input, d_output, d_indices, n);

    // 打印总结
    printf("========================================\n");
    printf("    总结\n");
    printf("========================================\n");
    printf("访问模式对带宽影响巨大：\n");
    printf("  - 合并访问 (stride=1): 最高带宽\n");
    printf("  - 跨步访问: stride 越大，带宽越低\n");
    printf("  - 随机访问: 最低带宽\n");
    printf("\n");
    printf("💡 优化建议:\n");
    printf("  1. 确保相邻线程访问相邻内存\n");
    printf("  2. 使用 AoS → SoA 数据布局转换\n");
    printf("  3. 利用共享内存重排数据\n");
    printf("  4. 使用向量类型 (float4) 增加吞吐量\n");

    // 清理
    HIP_CHECK(hipFree(d_input));
    HIP_CHECK(hipFree(d_output));
    HIP_CHECK(hipFree(d_indices));

    return 0;
}
