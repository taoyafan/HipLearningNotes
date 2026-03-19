#include <hip/hip_runtime.h>
#include <stdio.h>

// HIP kernel: 每个线程计算一个元素的加法
__global__ void vector_add(const float* A, const float* B, float* C, int N) {
    // 计算当前线程的全局索引
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    // 边界检查：确保不越界
    if (idx < N) {
        C[idx] = A[idx] + B[idx];
    }
}

int main() {
    const int N = 1024;           // 向量长度
    const int SIZE = N * sizeof(float);

    // ===== 1. 分配主机(CPU)内存 =====
    float *h_A = (float*)malloc(SIZE);
    float *h_B = (float*)malloc(SIZE);
    float *h_C = (float*)malloc(SIZE);

    // 初始化数据
    for (int i = 0; i < N; i++) {
        h_A[i] = i * 1.0f;
        h_B[i] = i * 2.0f;
    }

    // ===== 2. 分配设备(GPU)内存 =====
    float *d_A, *d_B, *d_C;
    (void)hipMalloc(&d_A, SIZE);
    (void)hipMalloc(&d_B, SIZE);
    (void)hipMalloc(&d_C, SIZE);

    // ===== 3. 将数据从主机复制到设备 =====
    (void)hipMemcpy(d_A, h_A, SIZE, hipMemcpyHostToDevice);
    (void)hipMemcpy(d_B, h_B, SIZE, hipMemcpyHostToDevice);

    // ===== 4. 配置并启动 kernel =====
    int threadsPerBlock = 256;    // 每个 block 256 个线程
    int blocksPerGrid = (N + threadsPerBlock - 1) / threadsPerBlock;  // 向上取整

    printf("启动配置: %d blocks, %d threads/block\n", blocksPerGrid, threadsPerBlock);

    // 启动 kernel (注意 HIP 使用 <<< >>> 语法)
    vector_add<<<blocksPerGrid, threadsPerBlock>>>(d_A, d_B, d_C, N);

    // 等待 GPU 完成
    (void)hipDeviceSynchronize();

    // ===== 5. 将结果从设备复制回主机 =====
    (void)hipMemcpy(h_C, d_C, SIZE, hipMemcpyDeviceToHost);

    // ===== 6. 验证结果 =====
    bool success = true;
    for (int i = 0; i < N; i++) {
        float expected = h_A[i] + h_B[i];
        if (h_C[i] != expected) {
            printf("错误: C[%d] = %f, 期望 %f\n", i, h_C[i], expected);
            success = false;
            break;
        }
    }

    if (success) {
        printf("验证通过! 前5个结果:\n");
        for (int i = 0; i < 5; i++) {
            printf("  %f + %f = %f\n", h_A[i], h_B[i], h_C[i]);
        }
    }

    // ===== 7. 释放内存 =====
    (void)hipFree(d_A);
    (void)hipFree(d_B);
    (void)hipFree(d_C);
    free(h_A);
    free(h_B);
    free(h_C);

    return 0;
}
