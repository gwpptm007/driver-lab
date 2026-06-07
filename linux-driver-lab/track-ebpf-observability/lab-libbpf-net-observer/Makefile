# Makefile for lab-libbpf-net-observer
# Phase 4: 从 bpftrace 脚本 → C/libbpf 编译型观测工具
#
# 依赖: clang, bpftool, libbpf-dev (>= 0.5)
# 思路:
#   1. bpftool gen vmlinux.h          — 从内核 BTF 生成完整内核类型头文件
#   2. clang -target bpf              — 编译 BPF 内核程序
#   3. gcc                            — 编译 userspace 加载器 (原生 libbpf API, 无 skeleton)
#   4. ./skb_observer                 — 运行

CC       = gcc
CLANG    = clang
BPFTOOL  = bpftool

CFLAGS   = -g -O2 -Wall
BPF_CFLAGS = -g -O2 -target bpf -D__TARGET_ARCH_x86 \
	-fno-jump-tables -fno-stack-protector
LDFLAGS  = -lbpf -lelf -lz

SRCDIR   = src
BUILDDIR = build

VMLINUX_H = $(BUILDDIR)/vmlinux.h
BPF_OBJ   = $(BUILDDIR)/skb_observer.bpf.o
TARGET    = $(BUILDDIR)/skb_observer

.PHONY: all clean build_dir

all: build_dir $(TARGET)

build_dir:
	mkdir -p $(BUILDDIR)

# Step 1: 从内核 BTF 生成 vmlinux.h
$(VMLINUX_H):
	@echo "[1/3] bpftool gen vmlinux.h ..."
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

# Step 2: 编译 BPF 内核程序
$(BPF_OBJ): $(SRCDIR)/skb_observer.bpf.c $(VMLINUX_H) $(SRCDIR)/skb_observer.h
	@echo "[2/3] clang -target bpf ..."
	$(CLANG) $(BPF_CFLAGS) \
		-I $(BUILDDIR) \
		-I $(SRCDIR) \
		-c $< -o $@
	@# 兼容 libbpf 0.5: 移除旧版不认识的 rodata section
	@if readelf -S $@ 2>/dev/null | grep -q '.rodata.str'; then \
		echo "[2a] strip .rodata.str* for libbpf 0.5 compat"; \
		llvm-objcopy --remove-section=.rodata.str1.1 $@ 2>/dev/null || true; \
	fi

# Step 3: 编译 userspace 程序 (使用原生 libbpf API，不需要 skeleton)
$(TARGET): $(SRCDIR)/skb_observer.c $(BPF_OBJ) $(SRCDIR)/skb_observer.h
	@echo "[3/3] gcc userspace ..."
	$(CC) $(CFLAGS) \
		-I $(BUILDDIR) \
		-I $(SRCDIR) \
		$< $(LDFLAGS) -o $@
	@echo ""
	@echo "Build OK → $(TARGET)"
	@echo "Run: sudo $(TARGET) -v -d 10"

clean:
	rm -rf $(BUILDDIR)
