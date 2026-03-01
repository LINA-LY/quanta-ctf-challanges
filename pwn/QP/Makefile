CC = gcc
CFLAGS = -Wall -Wextra -O0 -g -fno-stack-protector -z execstack -no-pie
LDFLAGS = -lseccomp

TARGET = quantum_printer
SOURCE = quantum_printer_v2.c

.PHONY: all clean debug release

all: release

debug: $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $<
	@echo "Built debug version (no seccomp)"

release: $(SOURCE)
	$(CC) $(CFLAGS) -DUSE_SECCOMP -o $(TARGET) $< $(LDFLAGS)
	@echo "Built release version (with seccomp)"

clean:
	rm -f $(TARGET)
	@echo "Cleaned build artifacts"
