# Makefile for 03_memory_bandwidth

HIPCC = hipcc
CFLAGS = -O3 -Wall

TARGETS = bandwidth_test coalescing_test

all: $(TARGETS)

bandwidth_test: bandwidth_test.cpp
	$(HIPCC) $(CFLAGS) -o $@ $<

coalescing_test: coalescing_test.cpp
	$(HIPCC) $(CFLAGS) -o $@ $<

run: all
	@echo "=========================================="
	@echo "Running bandwidth_test..."
	@echo "=========================================="
	./bandwidth_test
	@echo ""
	@echo "=========================================="
	@echo "Running coalescing_test..."
	@echo "=========================================="
	./coalescing_test

clean:
	rm -f $(TARGETS)

.PHONY: all run clean
