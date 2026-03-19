HIPCC = hipcc
TARGET = vector_add

all: $(TARGET)

$(TARGET): vector_add.cpp
	$(HIPCC) -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
