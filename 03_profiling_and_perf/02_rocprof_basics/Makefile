# Makefile for rocprof demo

TARGET = rocprof_demo
SRC = rocprof_demo.cpp
CXX = hipcc
CXXFLAGS = -O3

# 默认目标：编译
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# 普通运行（无性能分析）
run: $(TARGET)
	./$(TARGET)

# rocprof 基础统计 ⭐ 最常用
profile: $(TARGET)
	rocprof --stats ./$(TARGET)
	@echo ""
	@echo "=== 性能报告已生成 ==="
	@echo "查看详细数据: cat results.csv"

# HSA API 追踪
trace: $(TARGET)
	rocprof --hsa-trace ./$(TARGET)
	@echo ""
	@echo "=== API 追踪完成 ==="
	@echo "查看详细数据: cat results.json"

# 清理
clean:
	rm -f $(TARGET)
	rm -f results.* *.csv *.json *.db *.txt
	rm -rf rocprof_*

# 帮助信息
help:
	@echo "可用命令:"
	@echo "  make         - 编译程序"
	@echo "  make run     - 普通运行（无性能分析）"
	@echo "  make profile - 使用 rocprof 分析 ⭐"
	@echo "  make trace   - HSA API 追踪"
	@echo "  make clean   - 清理所有生成文件"

.PHONY: all run profile trace clean help
