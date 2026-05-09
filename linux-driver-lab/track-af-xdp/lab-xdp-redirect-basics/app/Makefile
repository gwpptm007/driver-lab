# SPDX-License-Identifier: GPL-2.0
#
# Makefile — lab-xdp-redirect-basics 构建配置
#
# 编译产物：
#   build/xdp_redirect_basics.bpf.o  — BPF 目标文件（内核态程序）
#   build/xdp_loader                  — libbpf 加载器（用户态程序）
#
# 关键编译选项说明：
#   clang -target bpf        → 生成 BPF bytecode（而非普通 x86 机器码）
#   -I/usr/include/x86_64-linux-gnu → 解决 asm/types.h not found（Ubuntu 22.04 兼容）
#

CLANG ?= clang
CC ?= cc
PKG_CONFIG ?= pkg-config

BUILD_DIR ?= build
BPF_C := xdp_redirect_basics.bpf.c
BPF_OBJ := $(BUILD_DIR)/xdp_redirect_basics.bpf.o
LOADER_C := xdp_loader.c
LOADER_BIN := $(BUILD_DIR)/xdp_loader

# 用户态编译器选项（cc 用于编译 xdp_loader.c）
CFLAGS ?= -O2 -g -Wall -Wextra

# BPF 编译选项
# -target bpf        → clang 生成 BPF bytecode（而不是 x86）
# -I/usr/include/x86_64-linux-gnu → Ubuntu 22.04 上 asm/types.h 的路径
#                        （clang -target bpf 默认不包含 x86_64 multiarch 路径，
#                        导致找不到 asm/types.h，报 "file not found" 错误）
BPF_CFLAGS ?= -O2 -g -Wall -Werror -target bpf -I/usr/include/x86_64-linux-gnu

# libbpf 链接选项（pkg-config 查不到时 fallback）
LIBBPF_CFLAGS := $(shell $(PKG_CONFIG) --cflags libbpf 2>/dev/null)
LIBBPF_LIBS := $(shell $(PKG_CONFIG) --libs libbpf 2>/dev/null)
ifeq ($(strip $(LIBBPF_LIBS)),)
# fallback：直接用 -lbpf -lelf -lz（Ubuntu 22.04 需要这些）
LIBBPF_LIBS := -lbpf -lelf -lz
endif

.PHONY: all clean show

# 默认目标：编译 BPF object + loader
all: $(BPF_OBJ) $(LOADER_BIN)

$(BUILD_DIR):
	mkdir -p $@

#-----------------------------------------------
# BPF 程序编译
#   clang -target bpf -c xxx.bpf.c -o xxx.bpf.o
#
# BPF object 是 ELF 格式，包含：
#   - map 定义（给 libbpf 知道要创建哪些 map）
#   - program bytecode（内核验证后执行）
#-----------------------------------------------
$(BPF_OBJ): $(BPF_C) | $(BUILD_DIR)
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

#-----------------------------------------------
# 用户态 loader 编译
#   cc xxx_loader.c -o xxx_loader -lbpf -lelf -lz
#
# 只链接 libbpf，不链接内核
#-----------------------------------------------
$(LOADER_BIN): $(LOADER_C) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LIBBPF_CFLAGS) $< -o $@ $(LIBBPF_LIBS)

# 显示编译变量（调试用）
show:
	@echo "BPF_OBJ=$(BPF_OBJ)"
	@echo "LOADER_BIN=$(LOADER_BIN)"
	@echo "LIBBPF_CFLAGS=$(LIBBPF_CFLAGS)"
	@echo "LIBBPF_LIBS=$(LIBBPF_LIBS)"

clean:
	rm -rf $(BUILD_DIR)
