/**
 * bandwidth_test.cpp - GPU 内存带宽测试
 *
 * 测试内容：
 * 1. Host-to-Device (H2D) 传输带宽
 * 2. Device-to-Host (D2H) 传输带宽
 * 3. Device-to-Device (D2D) 复制带宽
 * 4. Kernel 读取带宽
 * 5. Kernel 写入带宽
 * 6. Kernel 读写带宽（复制）
 *
 * 编译: hipcc -O3 -o bandwidth_test bandwidth_test.cpp
 * 运行: ./bandwidth_test
 */

#include <hip/hip_runtime.h>
#include <stdio.h>
#include <stdlib.h>

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
constexpr size_t DEFAULT_SIZE = 256 * 1024 * 1024;  // 256 MB
constexpr int NUM_ITERATIONS = 10;                   // 测量次数

// ============================================================================
// Kernel 定义
// ============================================================================

/**
 * 只读 kernel - 测试全局内存读取带宽
 * 每个线程读取一个 float，累加到 dummy 防止优化掉
 */
__global__ void kernel_read_only(const float* __restrict__ input,
                                  float* __restrict__ dummy,
                                  size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;

    float sum = 0.0f;
    for (size_t i = idx; i < n; i += stride) {
        sum += input[i];
    }

    // 防止编译器优化掉读取操作
    if (idx == 0) {
        *dummy = sum;
    }
}

/**
 * 只写 kernel - 测试全局内存写入带宽
 */
__global__ void kernel_write_only(float* __restrict__ output,
                                   float value,
                                   size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;

    for (size_t i = idx; i < n; i += stride) {
        output[i] = value;
    }
}

/**
 * 读写 kernel - 测试复制带宽（读 + 写）
 */
__global__ void kernel_copy(const float* __restrict__ input,
                            float* __restrict__ output,
                            size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;

    for (size_t i = idx; i < n; i += stride) {
        output[i] = input[i];
    }
}

/**
 * 向量化读写 kernel - 使用 float4 提高吞吐量
 */
__global__ void kernel_copy_vectorized(const float4* __restrict__ input,
                                        float4* __restrict__ output,
                                        size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;

    for (size_t i = idx; i < n; i += stride) {
        output[i] = input[i];
    }
}

// ============================================================================
// 辅助函数
// ============================================================================

/**
 * 获取 GPU 理论带宽
 */
void print_gpu_info() {
    hipDeviceProp_t prop;
    HIP_CHECK(hipGetDeviceProperties(&prop, 0));

    printf("========================================\n");
    printf("    GPU 内存带宽测试\n");
    printf("========================================\n");
    printf("GPU: %s\n", prop.name);
    printf("显存大小: %.2f GB\n", prop.totalGlobalMem / (1024.0 * 1024.0 * 1024.0));
    printf("内存时钟: %d MHz\n", prop.memoryClockRate / 1000);
    printf("内存位宽: %d-bit\n", prop.memoryBusWidth);

    // 计算理论带宽: 时钟频率 × 位宽 × 2 (DDR) / 8 (转换为字节)
    double theoretical_bandwidth =
        (prop.memoryClockRate * 1000.0) *  // Hz
        (prop.memoryBusWidth / 8.0) *       // 字节
        2.0 /                               // DDR 双倍数据率
        1e9;                                // 转换为 GB/s

    printf("理论带宽: %.1f GB/s\n", theoretical_bandwidth);
    printf("========================================\n\n");
}

/**
 * 使用 hipEvent 测量带宽
 */
float measure_bandwidth(float time_ms, size_t bytes) {
    return (bytes / 1e9) / (time_ms / 1000.0);  // GB/s
}

/**
 * 创建 hipEvent 并测量时间
 */
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

// ============================================================================
// 测试函数
// ============================================================================

void test_h2d_bandwidth(size_t size) {
    printf("=== 测试 1: Host-to-Device (H2D) 带宽 ===\n");

    float* h_data = (float*)malloc(size);
    float* d_data;
    HIP_CHECK(hipMalloc(&d_data, size));

    // 初始化数据
    for (size_t i = 0; i < size / sizeof(float); i++) {
        h_data[i] = 1.0f;
    }

    // Warmup
    HIP_CHECK(hipMemcpy(d_data, h_data, size, hipMemcpyHostToDevice));

    // 测量
    GpuTimer timer;
    timer.start();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        HIP_CHECK(hipMemcpy(d_data, h_data, size, hipMemcpyHostToDevice));
    }
    timer.stop();

    float avg_time_ms = timer.elapsed_ms() / NUM_ITERATIONS;
    float bandwidth = measure_bandwidth(avg_time_ms, size);

    printf("  数据大小: %.2f MB\n", size / (1024.0 * 1024.0));
    printf("  平均时间: %.3f ms\n", avg_time_ms);
    printf("  带宽: %.2f GB/s\n\n", bandwidth);

    HIP_CHECK(hipFree(d_data));
    free(h_data);
}

void test_d2h_bandwidth(size_t size) {
    printf("=== 测试 2: Device-to-Host (D2H) 带宽 ===\n");

    float* h_data = (float*)malloc(size);
    float* d_data;
    HIP_CHECK(hipMalloc(&d_data, size));

    // 初始化 GPU 数据
    HIP_CHECK(hipMemset(d_data, 0, size));

    // Warmup
    HIP_CHECK(hipMemcpy(h_data, d_data, size, hipMemcpyDeviceToHost));

    // 测量
    GpuTimer timer;
    timer.start();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        HIP_CHECK(hipMemcpy(h_data, d_data, size, hipMemcpyDeviceToHost));
    }
    timer.stop();

    float avg_time_ms = timer.elapsed_ms() / NUM_ITERATIONS;
    float bandwidth = measure_bandwidth(avg_time_ms, size);

    printf("  数据大小: %.2f MB\n", size / (1024.0 * 1024.0));
    printf("  平均时间: %.3f ms\n", avg_time_ms);
    printf("  带宽: %.2f GB/s\n\n", bandwidth);

    HIP_CHECK(hipFree(d_data));
    free(h_data);
}

void test_d2d_bandwidth(size_t size) {
    printf("=== 测试 3: Device-to-Device (D2D) 带宽 ===\n");

    float *d_src, *d_dst;
    HIP_CHECK(hipMalloc(&d_src, size));
    HIP_CHECK(hipMalloc(&d_dst, size));

    // 初始化
    HIP_CHECK(hipMemset(d_src, 0, size));

    // Warmup
    HIP_CHECK(hipMemcpy(d_dst, d_src, size, hipMemcpyDeviceToDevice));

    // 测量
    GpuTimer timer;
    timer.start();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        HIP_CHECK(hipMemcpy(d_dst, d_src, size, hipMemcpyDeviceToDevice));
    }
    timer.stop();

    float avg_time_ms = timer.elapsed_ms() / NUM_ITERATIONS;
    // D2D 需要读和写，所以实际带宽是传输量的 2 倍
    float bandwidth = measure_bandwidth(avg_time_ms, size * 2);

    printf("  数据大小: %.2f MB\n", size / (1024.0 * 1024.0));
    printf("  平均时间: %.3f ms\n", avg_time_ms);
    printf("  有效带宽: %.2f GB/s (读 + 写)\n\n", bandwidth);

    HIP_CHECK(hipFree(d_src));
    HIP_CHECK(hipFree(d_dst));
}

void test_kernel_read_bandwidth(size_t size) {
    printf("=== 测试 4: Kernel 读取带宽 ===\n");

    size_t n = size / sizeof(float);
    float *d_input, *d_dummy;
    HIP_CHECK(hipMalloc(&d_input, size));
    HIP_CHECK(hipMalloc(&d_dummy, sizeof(float)));

    // 初始化
    HIP_CHECK(hipMemset(d_input, 0, size));

    int blockSize = 256;
    int gridSize = (n + blockSize - 1) / blockSize;
    // 限制 grid 大小以充分利用 GPU
    gridSize = min(gridSize, 65536);

    // Warmup
    kernel_read_only<<<gridSize, blockSize>>>(d_input, d_dummy, n);
    HIP_CHECK(hipDeviceSynchronize());

    // 测量
    GpuTimer timer;
    timer.start();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        kernel_read_only<<<gridSize, blockSize>>>(d_input, d_dummy, n);
    }
    timer.stop();

    float avg_time_ms = timer.elapsed_ms() / NUM_ITERATIONS;
    float bandwidth = measure_bandwidth(avg_time_ms, size);

    printf("  数据大小: %.2f MB\n", size / (1024.0 * 1024.0));
    printf("  平均时间: %.3f ms\n", avg_time_ms);
    printf("  读取带宽: %.2f GB/s\n\n", bandwidth);

    HIP_CHECK(hipFree(d_input));
    HIP_CHECK(hipFree(d_dummy));
}

void test_kernel_write_bandwidth(size_t size) {
    printf("=== 测试 5: Kernel 写入带宽 ===\n");

    size_t n = size / sizeof(float);
    float* d_output;
    HIP_CHECK(hipMalloc(&d_output, size));

    int blockSize = 256;
    int gridSize = (n + blockSize - 1) / blockSize;
    gridSize = min(gridSize, 65536);

    // Warmup
    kernel_write_only<<<gridSize, blockSize>>>(d_output, 1.0f, n);
    HIP_CHECK(hipDeviceSynchronize());

    // 测量
    GpuTimer timer;
    timer.start();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        kernel_write_only<<<gridSize, blockSize>>>(d_output, 1.0f, n);
    }
    timer.stop();

    float avg_time_ms = timer.elapsed_ms() / NUM_ITERATIONS;
    float bandwidth = measure_bandwidth(avg_time_ms, size);

    printf("  数据大小: %.2f MB\n", size / (1024.0 * 1024.0));
    printf("  平均时间: %.3f ms\n", avg_time_ms);
    printf("  写入带宽: %.2f GB/s\n\n", bandwidth);

    HIP_CHECK(hipFree(d_output));
}

void test_kernel_copy_bandwidth(size_t size) {
    printf("=== 测试 6: Kernel 复制带宽 (读 + 写) ===\n");

    size_t n = size / sizeof(float);
    float *d_input, *d_output;
    HIP_CHECK(hipMalloc(&d_input, size));
    HIP_CHECK(hipMalloc(&d_output, size));

    // 初始化
    HIP_CHECK(hipMemset(d_input, 0, size));

    int blockSize = 256;
    int gridSize = (n + blockSize - 1) / blockSize;
    gridSize = min(gridSize, 65536);

    // Warmup
    kernel_copy<<<gridSize, blockSize>>>(d_input, d_output, n);
    HIP_CHECK(hipDeviceSynchronize());

    // 测量
    GpuTimer timer;
    timer.start();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        kernel_copy<<<gridSize, blockSize>>>(d_input, d_output, n);
    }
    timer.stop();

    float avg_time_ms = timer.elapsed_ms() / NUM_ITERATIONS;
    // 复制涉及读和写，所以是 2 倍数据量
    float bandwidth = measure_bandwidth(avg_time_ms, size * 2);

    printf("  数据大小: %.2f MB\n", size / (1024.0 * 1024.0));
    printf("  平均时间: %.3f ms\n", avg_time_ms);
    printf("  有效带宽: %.2f GB/s (读 + 写)\n\n", bandwidth);

    HIP_CHECK(hipFree(d_input));
    HIP_CHECK(hipFree(d_output));
}

void test_kernel_copy_vectorized(size_t size) {
    printf("=== 测试 7: 向量化复制带宽 (float4) ===\n");

    size_t n = size / sizeof(float4);  // float4 = 16 字节
    float *d_input, *d_output;
    HIP_CHECK(hipMalloc(&d_input, size));
    HIP_CHECK(hipMalloc(&d_output, size));

    // 初始化
    HIP_CHECK(hipMemset(d_input, 0, size));

    int blockSize = 256;
    int gridSize = (n + blockSize - 1) / blockSize;
    gridSize = min(gridSize, 65536);

    float4* d_input4 = reinterpret_cast<float4*>(d_input);
    float4* d_output4 = reinterpret_cast<float4*>(d_output);

    // Warmup
    kernel_copy_vectorized<<<gridSize, blockSize>>>(d_input4, d_output4, n);
    HIP_CHECK(hipDeviceSynchronize());

    // 测量
    GpuTimer timer;
    timer.start();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        kernel_copy_vectorized<<<gridSize, blockSize>>>(d_input4, d_output4, n);
    }
    timer.stop();

    float avg_time_ms = timer.elapsed_ms() / NUM_ITERATIONS;
    float bandwidth = measure_bandwidth(avg_time_ms, size * 2);

    printf("  数据大小: %.2f MB\n", size / (1024.0 * 1024.0));
    printf("  平均时间: %.3f ms\n", avg_time_ms);
    printf("  有效带宽: %.2f GB/s (读 + 写)\n\n", bandwidth);

    HIP_CHECK(hipFree(d_input));
    HIP_CHECK(hipFree(d_output));
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    size_t size = DEFAULT_SIZE;

    // 允许命令行指定大小 (MB)
    if (argc > 1) {
        size = atoi(argv[1]) * 1024 * 1024;
    }

    print_gpu_info();

    // 运行所有测试
    test_h2d_bandwidth(size);
    test_d2h_bandwidth(size);
    test_d2d_bandwidth(size);
    test_kernel_read_bandwidth(size);
    test_kernel_write_bandwidth(size);
    test_kernel_copy_bandwidth(size);
    test_kernel_copy_vectorized(size);

    printf("========================================\n");
    printf("    测试完成\n");
    printf("========================================\n");
    printf("💡 提示:\n");
    printf("  - H2D/D2H 受限于 PCIe 带宽 (~15 GB/s for PCIe 4.0 x16)\n");
    printf("  - D2D/Kernel 接近 GPU 显存带宽\n");
    printf("  - 向量化访问 (float4) 通常更高效\n");
    printf("  - 使用 rocprofv3 分析:\n");
    printf("    rocprofv3 --runtime-trace -f csv -- ./bandwidth_test\n");

    return 0;
}
