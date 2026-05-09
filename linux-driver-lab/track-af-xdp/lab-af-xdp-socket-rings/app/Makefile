BPF_CLANG ?= clang
CC ?= cc
BUILD_DIR ?= build
BPF_CFLAGS ?= -O2 -g -Wall -Werror -target bpf -D__TARGET_ARCH_x86 -I/usr/include/x86_64-linux-gnu
USER_CFLAGS ?= -O2 -g -Wall -Wextra
USER_LDLIBS ?= -lbpf -lelf -lz

BPF_SRC := af_xdp_kern.bpf.c
BPF_OBJ := $(BUILD_DIR)/af_xdp_kern.bpf.o
USER_SRC := af_xdp_rings.c
USER_BIN := $(BUILD_DIR)/af_xdp_rings

.PHONY: all clean show

all: $(BPF_OBJ) $(USER_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BPF_OBJ): $(BPF_SRC) | $(BUILD_DIR)
	$(BPF_CLANG) $(BPF_CFLAGS) -c $< -o $@

$(USER_BIN): $(USER_SRC) | $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) $< -o $@ $(USER_LDLIBS)

show:
	@echo "BPF_OBJ=$(BPF_OBJ)"
	@echo "USER_BIN=$(USER_BIN)"
	@echo "BPF_CFLAGS=$(BPF_CFLAGS)"
	@echo "USER_LDLIBS=$(USER_LDLIBS)"

clean:
	rm -rf $(BUILD_DIR)
