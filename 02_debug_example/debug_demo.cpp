#include <hip/hip_runtime.h>
#include <stdio.h>

// 演示：查找数组中的最大值（故意写一个有 bug 的版本）
__global__ void find_max_buggy(const int* data, int* result, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    // DEBUG: 打印每个线程的信息
    if (idx < 5) {  // 只打印前5个线程，避免输出太多
        printf("[DEBUG] Thread %d: blockIdx=%d, threadIdx=%d, blockDim=%d\n",
               idx, blockIdx.x, threadIdx.x, blockDim.x);
    }

    if (idx < N) {
        // DEBUG: 打印每个线程看到的数据
        if (idx < 5) {
            printf("[DEBUG] Thread %d: data[%d] = %d\n", idx, idx, data[idx]);
        }

        // BUG: 这里有竞态条件！多个线程同时写 result[0]
        if (data[idx] > result[0]) {
            printf("[DEBUG] Thread %d found larger value: %d > %d\n",
                   idx, data[idx], result[0]);
            result[0] = data[idx];  // 竞态条件！
        }
    }
}

// 正确版本：使用 atomicMax
__global__ void find_max_correct(const int* data, int* result, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < N) {
        // 使用原子操作避免竞态条件
        atomicMax(result, data[idx]);

        // DEBUG: 验证原子操作
        if (idx == 0) {
            printf("[DEBUG] After atomicMax, current max = %d\n", *result);
        }
    }
}

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

int main() {
    const int N = 16;
    const int SIZE = N * sizeof(int);

    // 准备测试数据
    int h_data[N] = {3, 7, 2, 9, 1, 8, 4, 6, 5, 12, 0, 11, 10, 15, 13, 14};
    int h_result = 0;
    int expected_max = 15;

    printf("=== 输入数据 ===\n");
    for (int i = 0; i < N; i++) {
        printf("%d ", h_data[i]);
    }
    printf("\n期望最大值: %d\n\n", expected_max);

    int *d_data, *d_result;
    HIP_CHECK(hipMalloc(&d_data, SIZE));
    HIP_CHECK(hipMalloc(&d_result, sizeof(int)));

    // ====== 测试有 Bug 的版本 ======
    printf("=== 测试 Buggy 版本 ===\n");
    h_result = 0;
    HIP_CHECK(hipMemcpy(d_data, h_data, SIZE, hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_result, &h_result, sizeof(int), hipMemcpyHostToDevice));

    find_max_buggy<<<1, 16>>>(d_data, d_result, N);
    HIP_CHECK(hipDeviceSynchronize());

    HIP_CHECK(hipMemcpy(&h_result, d_result, sizeof(int), hipMemcpyDeviceToHost));
    printf("Buggy 版本结果: %d (期望: %d) %s\n\n",
           h_result, expected_max,
           h_result == expected_max ? "✓" : "✗ 可能不对!");

    // ====== 测试正确版本 ======
    printf("=== 测试 Correct 版本 ===\n");
    h_result = 0;
    HIP_CHECK(hipMemcpy(d_result, &h_result, sizeof(int), hipMemcpyHostToDevice));

    find_max_correct<<<1, 16>>>(d_data, d_result, N);
    HIP_CHECK(hipDeviceSynchronize());

    HIP_CHECK(hipMemcpy(&h_result, d_result, sizeof(int), hipMemcpyDeviceToHost));
    printf("Correct 版本结果: %d (期望: %d) %s\n",
           h_result, expected_max,
           h_result == expected_max ? "✓" : "✗");

    // 清理
    HIP_CHECK(hipFree(d_data));
    HIP_CHECK(hipFree(d_result));

    return 0;
}
