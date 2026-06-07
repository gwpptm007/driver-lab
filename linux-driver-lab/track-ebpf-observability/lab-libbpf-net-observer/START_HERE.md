# START_HERE

当前目标：把 Phase 1-3 的 bpftrace 观测迁移到 C/libbpf。

## 阅读顺序

```text
1. README.md           — 概览与技术栈对照
2. src/skb_observer.h  — 事件类型定义 (先看这个)
3. src/skb_observer.bpf.c — BPF 内核程序 (kernel-space)
4. src/skb_observer.c  — Userspace 加载器 (user-space)
5. Makefile            — 构建流程
```

## 手动操作顺序

```bash
# 1. 从内核 BTF 生成 vmlinux.h
bpftool btf dump file /sys/kernel/btf/vmlinux format c > build/vmlinux.h

# 2. 编译 BPF 程序 (clang -target bpf)
clang -g -O2 -target bpf -D__TARGET_ARCH_x86 \
  -I build -I src \
  -c src/skb_observer.bpf.c -o build/skb_observer.bpf.o

# 3. 生成 skeleton 头文件
bpftool gen skeleton build/skb_observer.bpf.o > build/skb_observer.skel.h

# 4. 编译 userspace
gcc -g -O2 -Wall -I build -I src \
  src/skb_observer.c -lbpf -lelf -lz -o build/skb_observer

# 5. 运行 (需 root)
sudo build/skb_observer -v -d 10
```

## 一键编译 + 运行

```bash
bash scripts/01_build.sh                    # make
sudo EBPF_DURATION=10 bash scripts/02_run_observer.sh  # run
```

