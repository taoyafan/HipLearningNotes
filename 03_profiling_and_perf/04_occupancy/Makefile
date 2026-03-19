# Makefile for 04_occupancy

HIPCC = hipcc
CFLAGS = -O3 -Wall

TARGETS = occupancy_test

all: $(TARGETS)

occupancy_test: occupancy_test.cpp
	$(HIPCC) $(CFLAGS) -o $@ $<

run: all
	@echo "=========================================="
	@echo "Running occupancy_test..."
	@echo "=========================================="
	./occupancy_test

# 使用 rocprofv3 分析（查看 VGPR/SGPR/LDS）
profile: all
	rocprofv3 --kernel-trace -f csv -- ./occupancy_test
	@echo ""
	@echo "查看输出文件中的 VGPR_Count, SGPR_Count, LDS_Size 字段"

clean:
	rm -f $(TARGETS)
	rm -rf */  # 清理 rocprofv3 输出目录

.PHONY: all run profile clean
