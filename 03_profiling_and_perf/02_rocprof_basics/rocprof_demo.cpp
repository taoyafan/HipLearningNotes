#include <hip/hip_runtime.h>
#include <stdio.h>
#include <stdlib.h>

// HIP 错误检查宏
#define HIP_CHECK(call) \
    do { \
        hipError_t err = call; \
        if (err != hipSuccess) { \
            printf("HIP Error: %s at %s:%d\n", \
                   hipGetErrorString(err), __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

// 简单的向量加法 kernel
__global__ void vector_add(float* A, float* B, float* C, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < N) {
        C[idx] = A[idx] + B[idx];
    }
}

// 计算密集型 kernel（用于测试 GFLOPS）
__global__ void compute_intensive(float* data, int N, int iterations) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < N) {
        float value = data[idx];
        // 大量浮点运算
        for (int i = 0; i < iterations; i++) {
            value = value * 1.01f + 0.5f;  // 2 个 FLOPs per iteration
        }
        data[idx] = value;
    }
}

// 内存密集型 kernel（用于测试内存带宽）
__global__ void memory_intensive(float* input, float* output, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < N) {
        // 简单的内存读写，计算量很少
        output[idx] = input[idx] * 2.0f;
    }
}

int main() {
    const int N = 10 * 1024 * 1024;  // 10M 元素
    const int size = N * sizeof(float);

    printf("=== rocprof 性能分析演示 ===\n");
    printf("数组大小: %d 元素 (%.2f MB)\n\n", N, size / 1024.0 / 1024.0);

    // 分配主机内存
    float *h_A = (float*)malloc(size);
    float *h_B = (float*)malloc(size);
    float *h_C = (float*)malloc(size);

    // 初始化数据
    for (int i = 0; i < N; i++) {
        h_A[i] = i * 1.0f;
        h_B[i] = i * 2.0f;
    }

    // 分配设备内存
    float *d_A, *d_B, *d_C;
    HIP_CHECK(hipMalloc(&d_A, size));
    HIP_CHECK(hipMalloc(&d_B, size));
    HIP_CHECK(hipMalloc(&d_C, size));

    // 拷贝数据到设备
    HIP_CHECK(hipMemcpy(d_A, h_A, size, hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_B, h_B, size, hipMemcpyHostToDevice));

    // 配置 kernel 参数
    int threadsPerBlock = 256;
    int blocksPerGrid = (N + threadsPerBlock - 1) / threadsPerBlock;

    printf("正在执行 3 个不同特性的 kernel...\n\n");

    // ===== Kernel 1: 简单向量加法 =====
    printf("[1] 向量加法 (基础操作)\n");
    vector_add<<<blocksPerGrid, threadsPerBlock>>>(d_A, d_B, d_C, N);
    HIP_CHECK(hipDeviceSynchronize());

    // ===== Kernel 2: 计算密集型 =====
    printf("[2] 计算密集型 kernel (测试 GFLOPS)\n");
    compute_intensive<<<blocksPerGrid, threadsPerBlock>>>(d_A, N, 100);
    HIP_CHECK(hipDeviceSynchronize());

    // ===== Kernel 3: 内存密集型 =====
    printf("[3] 内存密集型 kernel (测试带宽)\n");
    memory_intensive<<<blocksPerGrid, threadsPerBlock>>>(d_A, d_C, N);
    HIP_CHECK(hipDeviceSynchronize());

    printf("\n所有 kernel 执行完成！\n");
    printf("提示: 使用 rocprof 运行此程序可以查看详细性能数据\n");

    // 清理
    HIP_CHECK(hipFree(d_A));
    HIP_CHECK(hipFree(d_B));
    HIP_CHECK(hipFree(d_C));
    free(h_A);
    free(h_B);
    free(h_C);

    return 0;
}
