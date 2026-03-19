#!/bin/bash
# 步骤 2: 从修改后的 .s 文件重新编译
# 用法: ./2_recompile.sh [汇编文件]

set -e

ARCH="${2:-gfx1201}"
ASM_FILE="${1:-vector_add-hip-amdgcn-amd-amdhsa-$ARCH.s}"
SRC="vector_add.cpp"
OUTPUT="vector_add_debug_modified"

if [ ! -f "$ASM_FILE" ]; then
    echo "✗ 错误: 未找到 $ASM_FILE"
    echo "请先运行 ./1_dump_asm.sh"
    exit 1
fi

echo "=== 使用汇编文件: $ASM_FILE ==="

echo "=== 步骤 1/4: 汇编 .s -> .o ==="
/opt/rocm-7.2.0/lib/llvm/bin/llvm-mc \
  -triple=amdgcn-amd-amdhsa \
  -mcpu=$ARCH \
  -filetype=obj \
  -g \
  -o kernel_modified.o \
  "$ASM_FILE" 2>/dev/null

echo "=== 步骤 2/4: 链接 .o -> .out ==="
/opt/rocm-7.2.0/lib/llvm/bin/lld \
  -flavor gnu \
  -m elf64_amdgpu \
  --no-undefined \
  -shared \
  -o kernel_modified.out \
  kernel_modified.o

echo "=== 步骤 3/4: 打包 .out -> .hipfb ==="
/opt/rocm-7.2.0/lib/llvm/bin/clang-offload-bundler \
  -type=o \
  -bundle-align=4096 \
  -targets=host-x86_64-unknown-linux-gnu,hipv4-amdgcn-amd-amdhsa--$ARCH \
  -input=/dev/null \
  -input=kernel_modified.out \
  -output=kernel_modified.hipfb

echo "=== 步骤 4/4: 编译最终可执行文件 ==="
/opt/rocm-7.2.0/lib/llvm/bin/clang++ \
  -g -O0 \
  -x hip \
  --cuda-host-only \
  -Xclang -fcuda-include-gpubinary -Xclang kernel_modified.hipfb \
  -L/opt/rocm-7.2.0/lib -lamdhip64 \
  -o $OUTPUT \
  $SRC

echo ""
echo "=== 构建完成! ==="
echo "可执行文件: $OUTPUT"
echo ""
echo "运行测试:"
echo "  ./$OUTPUT"
echo ""
echo "调试:"
echo "  rocgdb ./$OUTPUT"
echo "  或在 VSCode 中选择 'HIP Debug Modified Kernel'"
