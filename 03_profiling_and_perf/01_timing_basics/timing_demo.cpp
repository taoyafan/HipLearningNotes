/*
 * HIP Kernel 计时基础示例
 *
 * 学习目标：
 * 1. 使用 hipEvent 测量 kernel 执行时间
 * 2. 理解 CPU 时间 vs GPU 时间的差异
 * 3. 测量内存传输时间
 * 4. 计算有效带宽和 GFLOPS
 *
 * 编译：hipcc -O3 -o timing_demo timing_demo.cpp
 * 运行：./timing_demo
 */

#include <hip/hip_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define HIP_CHECK(call) \
    do { \
        hipError_t err = call; \
        if (err != hipSuccess) { \
            printf("HIP Error: %s at %s:%d\n", \
                   hipGetErrorString(err), __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

// ==================== 简单的向量加法 kernel ====================
__global__ void vector_add(float* A, float* B, float* C, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < N) {
        C[idx] = A[idx] + B[idx];
    }
}

// ==================== 计算密集型 kernel（用于测试 GFLOPS）====================
__global__ void compute_intensive(float* A, float* B, float* C, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < N) {
        float result = A[idx];
        // 每个元素执行 100 次浮点运算
        for (int i = 0; i < 100; i++) {
            result = result * 1.01f + B[idx];
        }
        C[idx] = result;
    }
}

// ==================== CPU 计时函数 ====================
double get_wall_time() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return (double)time.tv_sec + (double)time.tv_usec * 1.0e-6;
}

// ==================== GPU 计时（使用 hipEvent）====================
void demo_hip_event_timing() {
    printf("\n=== 示例 1: 使用 hipEvent 测量 GPU 时间 ===\n");

    const int N = 10000000;  // 1000 万元素
    const size_t SIZE = N * sizeof(float);

    // 分配主机内存
    float *h_A = (float*)malloc(SIZE);
    float *h_B = (float*)malloc(SIZE);
    float *h_C = (float*)malloc(SIZE);

    // 初始化数据
    for (int i = 0; i < N; i++) {
        h_A[i] = (float)i;
        h_B[i] = (float)i * 2.0f;
    }

    // 分配 GPU 内存
    float *d_A, *d_B, *d_C;
    HIP_CHECK(hipMalloc(&d_A, SIZE));
    HIP_CHECK(hipMalloc(&d_B, SIZE));
    HIP_CHECK(hipMalloc(&d_C, SIZE));

    // 创建 hipEvent 用于计时
    hipEvent_t start, stop;
    HIP_CHECK(hipEventCreate(&start));
    HIP_CHECK(hipEventCreate(&stop));

    // 测量内存拷贝时间（Host to Device）
    HIP_CHECK(hipEventRecord(start, 0));
    HIP_CHECK(hipMemcpy(d_A, h_A, SIZE, hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_B, h_B, SIZE, hipMemcpyHostToDevice));
    HIP_CHECK(hipEventRecord(stop, 0));
    HIP_CHECK(hipEventSynchronize(stop));

    float h2d_time;
    HIP_CHECK(hipEventElapsedTime(&h2d_time, start, stop));
    printf("Host to Device 传输时间: %.3f ms\n", h2d_time);
    printf("  数据量: %.2f MB\n", SIZE * 2.0 / 1024.0 / 1024.0);
    printf("  带宽: %.2f GB/s\n", (SIZE * 2.0 / 1024.0 / 1024.0 / 1024.0) / (h2d_time / 1000.0));

    // 测量 kernel 执行时间
    int threadsPerBlock = 256;
    int blocksPerGrid = (N + threadsPerBlock - 1) / threadsPerBlock;

    HIP_CHECK(hipEventRecord(start, 0));
    vector_add<<<blocksPerGrid, threadsPerBlock>>>(d_A, d_B, d_C, N);
    HIP_CHECK(hipEventRecord(stop, 0));
    HIP_CHECK(hipEventSynchronize(stop));

    float kernel_time;
    HIP_CHECK(hipEventElapsedTime(&kernel_time, start, stop));
    printf("\nKernel 执行时间: %.3f ms\n", kernel_time);
    printf("  处理元素: %d\n", N);
    printf("  吞吐量: %.2f GElements/s\n", (N / 1e9) / (kernel_time / 1000.0));

    // 测量内存拷贝时间（Device to Host）
    HIP_CHECK(hipEventRecord(start, 0));
    HIP_CHECK(hipMemcpy(h_C, d_C, SIZE, hipMemcpyDeviceToHost));
    HIP_CHECK(hipEventRecord(stop, 0));
    HIP_CHECK(hipEventSynchronize(stop));

    float d2h_time;
    HIP_CHECK(hipEventElapsedTime(&d2h_time, start, stop));
    printf("\nDevice to Host 传输时间: %.3f ms\n", d2h_time);
    printf("  带宽: %.2f GB/s\n", (SIZE / 1024.0 / 1024.0 / 1024.0) / (d2h_time / 1000.0));

    printf("\n总时间: %.3f ms\n", h2d_time + kernel_time + d2h_time);

    // 验证结果
    bool correct = true;
    for (int i = 0; i < 10; i++) {
        if (h_C[i] != h_A[i] + h_B[i]) {
            correct = false;
            break;
        }
    }
    printf("结果验证: %s\n", correct ? "✅ 正确" : "❌ 错误");

    // 清理
    HIP_CHECK(hipEventDestroy(start));
    HIP_CHECK(hipEventDestroy(stop));
    HIP_CHECK(hipFree(d_A));
    HIP_CHECK(hipFree(d_B));
    HIP_CHECK(hipFree(d_C));
    free(h_A);
    free(h_B);
    free(h_C);
}

// ==================== CPU vs GPU 计时对比 ====================
void demo_cpu_vs_gpu_timing() {
    printf("\n=== 示例 2: CPU 时间 vs GPU 时间 ===\n");

    const int N = 5000000;
    const size_t SIZE = N * sizeof(float);

    float *h_A = (float*)malloc(SIZE);
    float *h_B = (float*)malloc(SIZE);
    float *h_C = (float*)malloc(SIZE);
    float *d_A, *d_B, *d_C;

    for (int i = 0; i < N; i++) {
        h_A[i] = (float)i;
        h_B[i] = (float)i * 2.0f;
    }

    HIP_CHECK(hipMalloc(&d_A, SIZE));
    HIP_CHECK(hipMalloc(&d_B, SIZE));
    HIP_CHECK(hipMalloc(&d_C, SIZE));
    HIP_CHECK(hipMemcpy(d_A, h_A, SIZE, hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_B, h_B, SIZE, hipMemcpyHostToDevice));

    // CPU 计时（wall clock time）
    double cpu_start = get_wall_time();

    int threadsPerBlock = 256;
    int blocksPerGrid = (N + threadsPerBlock - 1) / threadsPerBlock;
    vector_add<<<blocksPerGrid, threadsPerBlock>>>(d_A, d_B, d_C, N);
    HIP_CHECK(hipDeviceSynchronize());  // 等待 GPU 完成

    double cpu_end = get_wall_time();
    double cpu_time = (cpu_end - cpu_start) * 1000.0;  // 转换为毫秒

    // GPU 计时（使用 hipEvent）
    hipEvent_t start, stop;
    HIP_CHECK(hipEventCreate(&start));
    HIP_CHECK(hipEventCreate(&stop));

    HIP_CHECK(hipEventRecord(start, 0));
    vector_add<<<blocksPerGrid, threadsPerBlock>>>(d_A, d_B, d_C, N);
    HIP_CHECK(hipEventRecord(stop, 0));
    HIP_CHECK(hipEventSynchronize(stop));

    float gpu_time;
    HIP_CHECK(hipEventElapsedTime(&gpu_time, start, stop));

    printf("CPU 计时（wall time）: %.3f ms\n", cpu_time);
    printf("GPU 计时（hipEvent）: %.3f ms\n", gpu_time);
    printf("差异: %.3f ms\n", cpu_time - gpu_time);
    printf("\n💡 CPU 时间包含 kernel 启动开销，GPU 时间只计算实际执行时间\n");

    // 清理
    HIP_CHECK(hipEventDestroy(start));
    HIP_CHECK(hipEventDestroy(stop));
    HIP_CHECK(hipFree(d_A));
    HIP_CHECK(hipFree(d_B));
    HIP_CHECK(hipFree(d_C));
    free(h_A);
    free(h_B);
    free(h_C);
}

// ==================== 测量 GFLOPS ====================
void demo_gflops_measurement() {
    printf("\n=== 示例 3: 计算 GFLOPS（每秒十亿次浮点运算）===\n");

    const int N = 1000000;
    const size_t SIZE = N * sizeof(float);

    float *h_A = (float*)malloc(SIZE);
    float *h_B = (float*)malloc(SIZE);
    float *h_C = (float*)malloc(SIZE);
    float *d_A, *d_B, *d_C;

    for (int i = 0; i < N; i++) {
        h_A[i] = 1.0f;
        h_B[i] = 0.01f;
    }

    HIP_CHECK(hipMalloc(&d_A, SIZE));
    HIP_CHECK(hipMalloc(&d_B, SIZE));
    HIP_CHECK(hipMalloc(&d_C, SIZE));
    HIP_CHECK(hipMemcpy(d_A, h_A, SIZE, hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_B, h_B, SIZE, hipMemcpyHostToDevice));

    hipEvent_t start, stop;
    HIP_CHECK(hipEventCreate(&start));
    HIP_CHECK(hipEventCreate(&stop));

    int threadsPerBlock = 256;
    int blocksPerGrid = (N + threadsPerBlock - 1) / threadsPerBlock;

    // 测量计算密集型 kernel
    HIP_CHECK(hipEventRecord(start, 0));
    compute_intensive<<<blocksPerGrid, threadsPerBlock>>>(d_A, d_B, d_C, N);
    HIP_CHECK(hipEventRecord(stop, 0));
    HIP_CHECK(hipEventSynchronize(stop));

    float time_ms;
    HIP_CHECK(hipEventElapsedTime(&time_ms, start, stop));

    // 计算 FLOPS
    // 每个元素：100 次迭代 × 2 次运算（乘法 + 加法）= 200 次浮点运算
    long long total_flops = (long long)N * 200;
    double gflops = (total_flops / 1e9) / (time_ms / 1000.0);

    printf("Kernel 执行时间: %.3f ms\n", time_ms);
    printf("总浮点运算次数: %lld (%.2f billion)\n", total_flops, total_flops / 1e9);
    printf("性能: %.2f GFLOPS\n", gflops);

    // 获取 GPU 信息
    hipDeviceProp_t prop;
    HIP_CHECK(hipGetDeviceProperties(&prop, 0));
    printf("\nGPU: %s\n", prop.name);
    printf("理论峰值性能（参考）: 取决于具体 GPU 型号\n");

    // 清理
    HIP_CHECK(hipEventDestroy(start));
    HIP_CHECK(hipEventDestroy(stop));
    HIP_CHECK(hipFree(d_A));
    HIP_CHECK(hipFree(d_B));
    HIP_CHECK(hipFree(d_C));
    free(h_A);
    free(h_B);
    free(h_C);
}

// ==================== 主函数 ====================
int main() {
    printf("========================================\n");
    printf("    HIP Kernel 计时示例\n");
    printf("========================================\n");

    // 获取 GPU 信息
    int device;
    hipDeviceProp_t prop;
    HIP_CHECK(hipGetDevice(&device));
    HIP_CHECK(hipGetDeviceProperties(&prop, device));
    printf("使用 GPU: %s\n", prop.name);
    printf("计算能力: %d.%d\n", prop.major, prop.minor);
    printf("全局内存: %.2f GB\n", prop.totalGlobalMem / 1024.0 / 1024.0 / 1024.0);

    // 运行示例
    demo_hip_event_timing();
    demo_cpu_vs_gpu_timing();
    demo_gflops_measurement();

    printf("\n========================================\n");
    printf("    所有测试完成！\n");
    printf("========================================\n");

    printf("\n📖 关键概念总结：\n");
    printf("   1. hipEvent - GPU 时间测量的标准方法\n");
    printf("   2. hipEventRecord - 在 GPU stream 中插入时间戳\n");
    printf("   3. hipEventElapsedTime - 计算两个 event 之间的时间差\n");
    printf("   4. CPU 时间 > GPU 时间（包含 kernel 启动开销）\n");
    printf("   5. 带宽 = 数据量 / 时间\n");
    printf("   6. GFLOPS = 浮点运算次数 / 时间\n");

    return 0;
}
