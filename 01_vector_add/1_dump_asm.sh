#!/bin/bash
# 步骤 1: 生成 GPU 汇编文件
# 用法: ./1_dump_asm.sh

set -e

ARCH="${1:-gfx1201}"
SRC="vector_add.cpp"

echo "=== 生成带调试信息的汇编 (架构: $ARCH) ==="
hipcc -g -O0 --save-temps --offload-arch=$ARCH -o vector_add_debug $SRC 2>/dev/null

ASM_FILE="vector_add-hip-amdgcn-amd-amdhsa-$ARCH.s"

if [ -f "$ASM_FILE" ]; then
    echo "✓ 汇编文件已生成: $ASM_FILE"
    echo ""
    echo "=== Kernel 函数的关键指令 ==="
    grep -n "v_add_f32\|v_mul_f32\|v_sub_f32\|v_fma_f32" "$ASM_FILE" | head -10
    echo ""
    echo "下一步:"
    echo "  1. 编辑 $ASM_FILE"
    echo "  2. 运行 ./2_recompile.sh"
else
    echo "✗ 错误: 未找到 $ASM_FILE"
    exit 1
fi
