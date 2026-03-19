/**
 * occupancy_test.cpp - GPU 占用率测试
 *
 * 演示不同配置对占用率和性能的影响：
 * 1. 不同 block size 的影响
 * 2. 不同寄存器使用量的影响
 * 3. 不同 LDS 使用量的影响
 *
 * 编译: hipcc -O3 -o occupancy_test occupancy_test.cpp
 * 运行: ./occupancy_test
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
constexpr size_t ARRAY_SIZE = 64 * 1024 * 1024;  // 64M 元素
constexpr int NUM_ITERATIONS = 20;    // 增加迭代次数以获得稳定结果
constexpr int WARMUP_ITERATIONS = 5;  // warmup 次数

// ============================================================================
// Kernel 定义 - 不同寄存器使用量
// ============================================================================

/**
 * 低寄存器版本 - 简单操作
 */
__global__ void kernel_low_registers(const float* __restrict__ input,
                                      float* __restrict__ output,
                                      size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        output[idx] = input[idx] * 2.0f;
    }
}

/**
 * 中等寄存器版本 - 更多局部变量
 */
__global__ void kernel_medium_registers(const float* __restrict__ input,
                                         float* __restrict__ output,
                                         size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float v = input[idx];
        float a = v * 2.0f;
        float b = v * 3.0f;
        float c = v * 4.0f;
        float d = a + b + c;
        float e = a * b - c;
        float f = d / (e + 1.0f);
        output[idx] = f;
    }
}

/**
 * 高寄存器版本 - 大量局部变量
 */
__global__ void kernel_high_registers(const float* __restrict__ input,
                                       float* __restrict__ output,
                                       size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float v = input[idx];

        // 使用大量局部变量迫使使用更多寄存器
        float r[16];
        for (int i = 0; i < 16; i++) {
            r[i] = v * (float)(i + 1);
        }

        float sum = 0.0f;
        for (int i = 0; i < 16; i++) {
            sum += r[i] * r[15 - i];
        }

        output[idx] = sum;
    }
}

// ============================================================================
// Kernel 定义 - 不同 LDS 使用量
// ============================================================================

/**
 * 无 LDS - 基准版本
 */
__global__ void kernel_no_lds(const float* __restrict__ input,
                               float* __restrict__ output,
                               size_t n) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        output[idx] = input[idx] * 2.0f;
    }
}

/**
 * 小 LDS - 1KB per workgroup
 */
__global__ void kernel_small_lds(const float* __restrict__ input,
                                  float* __restrict__ output,
                                  size_t n) {
    __shared__ float lds[256];  // 1KB

    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    int lid = threadIdx.x;

    if (idx < n) {
        lds[lid] = input[idx];
        __syncthreads();
        output[idx] = lds[lid] * 2.0f;
    }
}

/**
 * 中 LDS - 8KB per workgroup
 */
__global__ void kernel_medium_lds(const float* __restrict__ input,
                                   float* __restrict__ output,
                                   size_t n) {
    __shared__ float lds[2048];  // 8KB

    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    int lid = threadIdx.x;

    if (idx < n) {
        // 填充 LDS（模拟实际使用）
        for (int i = lid; i < 2048; i += blockDim.x) {
            lds[i] = (float)i;
        }
        __syncthreads();

        float sum = 0.0f;
        for (int i = 0; i < 8; i++) {
            sum += lds[(lid + i * 256) % 2048];
        }

        output[idx] = input[idx] + sum * 0.001f;
    }
}

/**
 * 大 LDS - 32KB per workgroup
 */
__global__ void kernel_large_lds(const float* __restrict__ input,
                                  float* __restrict__ output,
                                  size_t n) {
    __shared__ float lds[8192];  // 32KB

    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    int lid = threadIdx.x;

    if (idx < n) {
        // 填充 LDS
        for (int i = lid; i < 8192; i += blockDim.x) {
            lds[i] = (float)i;
        }
        __syncthreads();

        float sum = 0.0f;
        for (int i = 0; i < 32; i++) {
            sum += lds[(lid + i * 256) % 8192];
        }

        output[idx] = input[idx] + sum * 0.0001f;
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

void print_gpu_info() {
    hipDeviceProp_t prop;
    HIP_CHECK(hipGetDeviceProperties(&prop, 0));

    printf("========================================\n");
    printf("    GPU 占用率测试\n");
    printf("========================================\n");
    printf("GPU: %s\n", prop.name);
    printf("Compute Units: %d\n", prop.multiProcessorCount);
    printf("Max Threads/Block: %d\n", prop.maxThreadsPerBlock);
    printf("Max Threads/CU: %d\n", prop.maxThreadsPerMultiProcessor);
    printf("Registers/Block: %d\n", prop.regsPerBlock);
    printf("Shared Memory/Block: %zu KB\n", prop.sharedMemPerBlock / 1024);
    printf("Warp Size: %d\n", prop.warpSize);
    printf("========================================\n\n");
}

template <typename KernelFunc>
void run_test(const char* name, KernelFunc kernel,
              float* d_input, float* d_output, size_t n,
              int blockSize) {
    int gridSize = (n + blockSize - 1) / blockSize;

    // Warmup - 多次运行让 GPU 进入稳定频率
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        kernel<<<gridSize, blockSize>>>(d_input, d_output, n);
    }
    HIP_CHECK(hipDeviceSynchronize());

    // 测量
    GpuTimer timer;
    timer.start();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        kernel<<<gridSize, blockSize>>>(d_input, d_output, n);
    }
    timer.stop();

    float avg_time_ms = timer.elapsed_ms() / NUM_ITERATIONS;
    float bandwidth = (n * sizeof(float) * 2 / 1e9) / (avg_time_ms / 1000.0);

    printf("  %-25s Block=%4d  Time=%.3f ms  BW=%.1f GB/s\n",
           name, blockSize, avg_time_ms, bandwidth);
}

// ============================================================================
// 测试函数
// ============================================================================

void test_block_size(float* d_input, float* d_output, size_t n) {
    printf("=== 测试 1: Block Size 对性能的影响 ===\n");
    printf("(使用简单 kernel，主要测试 block size)\n\n");

    int blockSizes[] = {64, 128, 256, 512, 1024};

    for (int bs : blockSizes) {
        run_test("kernel_low_registers", kernel_low_registers,
                 d_input, d_output, n, bs);
    }
    printf("\n");
}

void test_register_usage(float* d_input, float* d_output, size_t n) {
    printf("=== 测试 2: 寄存器使用量对性能的影响 ===\n");
    printf("(Block Size = 256)\n\n");

    int blockSize = 256;

    run_test("Low Registers", kernel_low_registers,
             d_input, d_output, n, blockSize);
    run_test("Medium Registers", kernel_medium_registers,
             d_input, d_output, n, blockSize);
    run_test("High Registers", kernel_high_registers,
             d_input, d_output, n, blockSize);

    printf("\n");
}

void test_lds_usage(float* d_input, float* d_output, size_t n) {
    printf("=== 测试 3: LDS 使用量对性能的影响 ===\n");
    printf("(Block Size = 256)\n\n");

    int blockSize = 256;

    run_test("No LDS (0 KB)", kernel_no_lds,
             d_input, d_output, n, blockSize);
    run_test("Small LDS (1 KB)", kernel_small_lds,
             d_input, d_output, n, blockSize);
    run_test("Medium LDS (8 KB)", kernel_medium_lds,
             d_input, d_output, n, blockSize);
    run_test("Large LDS (32 KB)", kernel_large_lds,
             d_input, d_output, n, blockSize);

    printf("\n");
}

/**
 * 计算并打印占用率
 */
template <typename KernelFunc>
void print_occupancy(const char* name, KernelFunc kernel,
                     int blockSize, size_t dynamicSharedMem = 0) {
    hipDeviceProp_t prop;
    HIP_CHECK(hipGetDeviceProperties(&prop, 0));

    int numBlocks;
    HIP_CHECK(hipOccupancyMaxActiveBlocksPerMultiprocessor(
        &numBlocks, kernel, blockSize, dynamicSharedMem));

    // 计算占用率
    int wavefrontsPerBlock = blockSize / prop.warpSize;
    int activeWavefronts = numBlocks * wavefrontsPerBlock;
    int maxWavefronts = prop.maxThreadsPerMultiProcessor / prop.warpSize;
    float occupancy = (float)activeWavefronts / maxWavefronts * 100.0f;

    printf("  %-25s Block=%4d  Blocks/CU=%2d  Wavefronts=%2d/%2d  Occupancy=%.1f%%\n",
           name, blockSize, numBlocks, activeWavefronts, maxWavefronts, occupancy);
}

void test_occupancy_api() {
    printf("=== 测试 4: 占用率计算 ===\n\n");

    hipDeviceProp_t prop;
    HIP_CHECK(hipGetDeviceProperties(&prop, 0));
    printf("  Max Threads/CU: %d, Wavefront Size: %d, Max Wavefronts/CU: %d\n\n",
           prop.maxThreadsPerMultiProcessor, prop.warpSize,
           prop.maxThreadsPerMultiProcessor / prop.warpSize);

    // 测试不同 kernel 的占用率
    printf("  --- 不同 Block Size ---\n");
    print_occupancy("kernel_low_registers", kernel_low_registers, 64);
    print_occupancy("kernel_low_registers", kernel_low_registers, 128);
    print_occupancy("kernel_low_registers", kernel_low_registers, 256);
    print_occupancy("kernel_low_registers", kernel_low_registers, 512);
    print_occupancy("kernel_low_registers", kernel_low_registers, 1024);

    printf("\n  --- 不同寄存器使用量 (Block=256) ---\n");
    print_occupancy("kernel_low_registers", kernel_low_registers, 256);
    print_occupancy("kernel_medium_registers", kernel_medium_registers, 256);
    print_occupancy("kernel_high_registers", kernel_high_registers, 256);

    printf("\n  --- 不同 LDS 使用量 (Block=256) ---\n");
    print_occupancy("kernel_no_lds", kernel_no_lds, 256, 0);
    print_occupancy("kernel_small_lds (1KB)", kernel_small_lds, 256, 1024);
    print_occupancy("kernel_medium_lds (8KB)", kernel_medium_lds, 256, 8192);
    print_occupancy("kernel_large_lds (32KB)", kernel_large_lds, 256, 32768);

    printf("\n");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    print_gpu_info();

    size_t n = ARRAY_SIZE;
    size_t size = n * sizeof(float);

    // 分配内存
    float *d_input, *d_output;
    HIP_CHECK(hipMalloc(&d_input, size));
    HIP_CHECK(hipMalloc(&d_output, size));

    // 初始化
    float* h_input = (float*)malloc(size);
    for (size_t i = 0; i < n; i++) {
        h_input[i] = 1.0f;
    }
    HIP_CHECK(hipMemcpy(d_input, h_input, size, hipMemcpyHostToDevice));
    free(h_input);

    // 运行测试
    test_block_size(d_input, d_output, n);
    test_register_usage(d_input, d_output, n);
    test_lds_usage(d_input, d_output, n);
    test_occupancy_api();

    // 清理
    HIP_CHECK(hipFree(d_input));
    HIP_CHECK(hipFree(d_output));

    printf("========================================\n");
    printf("    测试完成\n");
    printf("========================================\n");
    printf("💡 提示:\n");
    printf("  - 使用 rocprofv3 查看详细资源使用:\n");
    printf("    rocprofv3 --kernel-trace -f csv -- ./occupancy_test\n");
    printf("  - 查看 kernel_trace.csv 中的 VGPR_Count, SGPR_Count, LDS_Size\n");
    printf("  - 高占用率不一定 = 高性能，需要具体分析\n");

    return 0;
}
