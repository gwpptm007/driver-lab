CLANG ?= clang
CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra
BPF_CFLAGS ?= -O2 -g -Wall -Werror -target bpf -I/usr/include/x86_64-linux-gnu
LDFLAGS ?= -lbpf -lelf -lz
BUILD_DIR := build

BPF_SRC := af_xdp_forwarder_kern.bpf.c
BPF_OBJ := $(BUILD_DIR)/af_xdp_forwarder_kern.bpf.o
USER_SRC := af_xdp_forwarder.c
USER_BIN := $(BUILD_DIR)/af_xdp_forwarder

.PHONY: all clean show

all: $(BPF_OBJ) $(USER_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BPF_OBJ): $(BPF_SRC) | $(BUILD_DIR)
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

$(USER_BIN): $(USER_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

show:
	@echo "CLANG=$(CLANG)"
	@echo "CC=$(CC)"
	@echo "BPF_OBJ=$(BPF_OBJ)"
	@echo "USER_BIN=$(USER_BIN)"

clean:
	rm -rf $(BUILD_DIR)
