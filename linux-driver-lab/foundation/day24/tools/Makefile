CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
STRIP := $(CROSS_COMPILE)strip
TOOL_OUT ?= ../workdir/tools/aarch64/day24_mmio_tool

all: $(TOOL_OUT)

$(TOOL_OUT): day24_mmio_tool.c ../include/day24_ivshmem_uapi.h
	mkdir -p $(dir $@)
	$(CC) -O2 -Wall -Wextra -static -I../include -o $@ day24_mmio_tool.c
	$(STRIP) $@ || true

clean:
	rm -f $(TOOL_OUT)
