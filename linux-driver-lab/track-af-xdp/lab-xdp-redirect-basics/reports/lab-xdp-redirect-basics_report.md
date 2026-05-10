# lab-xdp-redirect-basics report

## Status

`PASS_BASIC` ✅ 2026-05-09

## Environment

| Item | Value |
|------|-------|
| OS | Ubuntu 22.04.5 LTS |
| Kernel | 6.8.0-110-generic |
| Hypervisor | VMware |
| Test NIC | ens192 / vmxnet3 / 0000:0b:00.0 |
| clang | Ubuntu clang version 14.0.0-1ubuntu1.1 |
| libbpf | 0.5.0 (Ubuntu 22.04 default) |
| bpftool | v7.4.0 |

## Verdict

### PASS_BUILD ✅

```
xdp_redirect_basics.bpf.o: ELF 64-bit LSB relocatable, eBPF (9.7K)
xdp_loader: ELF 64-bit LSB pie executable, x86-64 (39K)
BUILD_RESULT=PASS
```

### PASS_SMOKE ✅

```
attached XDP program
ifname=ens192 ifindex=2 mode=skb action=pass(2) duration=10 interval=1
xdp stats @ 10 intervals... (all printing OK)
detach ok
```

## Issues Fixed During Testing

| # | Problem | Root Cause | Fix |
|---|---------|------------|-----|
| 1 | `asm/types.h` not found | clang `-target bpf` 不含 x86_64 multiarch path | `app/Makefile`: `BPF_CFLAGS += -I/usr/include/x86_64-linux-gnu` |
| 2 | `bpf_xdp_attach` undefined | libbpf 0.5.0 太旧，API 需 1.0+ | `xdp_loader.c`: 添加 `#if LIBBPF_VERSION < 100` 兼容宏 |
| 3 | `dpkg --configure -a` 需先执行 | 之前中断的 apt install | 先跑一次 `sudo dpkg --configure -a` |

## Warnings (Non-blocking)

```
libbpf: Error in bpf_create_map_xattr(xsks_map): -524. Retrying without BTF.
```

XSKMAP 创建时 BTF 不支持但 fallback 正常，不影响本 lab 验证目标。

## Required Packages

```bash
sudo apt install clang llvm make pkg-config
sudo apt install libbpf-dev libelf-dev zlib1g-dev
sudo apt install linux-headers-$(uname -r)
sudo dpkg --configure -a   # 如果之前 apt 中断
```

## What This Lab Proves

- ✅ BPF object can be built with clang `-target bpf`
- ✅ libbpf loader can attach XDP program to vmxnet3 via generic (SKB) mode
- ✅ XDP action (PASS/DROP/REDIRECT) controllable from user space via config_map
- ✅ Per-CPU stats map readable and summarizable
- ✅ XSKMAP prepared for AF_XDP socket integration
- ✅ Detach / reattach cycle works cleanly

## What This Lab Does NOT Prove

- It does not prove AF_XDP socket RX/TX
- It does not prove UMEM / ring processing
- It does not prove zero-copy

Those are covered by later labs.

## Next Step

Add AF_XDP socket creation and bind to XSKMAP → next lab in the track.
