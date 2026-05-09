# app: af_xdp_zero_copy_vs_copy 实验程序

## 目录结构

| 文件 | 语言 | 说明 |
|------|------|------|
| `af_xdp_kern.bpf.c` | C (BPF) | 内核侧 XDP program，redirect 到 XSKMAP |
| `af_xdp_mode_probe.c` | C | 用户态程序，支持 4 种模式组合探测 |
| `Makefile` | Make | 编译 BPF (.o) 和用户态程序 |
| `README.md` | — | 本文件 |

## 编译

```bash
make
```

输出：
- `build/af_xdp_kern.bpf.o` — BPF 目标文件（内核使用）
- `build/af_xdp_mode_probe` — 用户态可执行文件

## 模式组合

本程序支持 4 种模式组合探测：

| 模式组合 | 说明 | 预期结果（VMware vmxnet3） |
|---------|------|--------------------------|
| `skb + copy` | 通用 XDP + copy | ✅ 稳定通过（基线） |
| `native + copy` | 原生 XDP attach + copy | ⚠️ 可能失败（驱动可能不支持） |
| `native + zero-copy` | 原生 XDP + 零拷贝 | ❌ 大概率失败（vmxnet3 无 ZC 支持） |
| `skb + zero-copy` | 通用 XDP + 零拷贝 | ❌ 不成立（边界探测） |

## 运行示例

```bash
# 基线测试（skb + copy）
sudo ./build/af_xdp_mode_probe \
    --ifname ens192 --queue 0 --mode skb --copy \
    --duration 8 --interval 1 --obj ./build/af_xdp_kern.bpf.o

# 零拷贝探测（native + zero-copy）
sudo ./build/af_xdp_mode_probe \
    --ifname ens192 --queue 0 --mode native --zero-copy \
    --duration 8 --interval 1 --obj ./build/af_xdp_kern.bpf.o
```

## 预期 smoke 标记

```
UMEM_READY                   — UMEM 创建成功
XSK_SOCKET_READY            — AF_XDP socket 创建成功
FILL_RING_READY             — FILL ring 填充成功
XDP_ATTACHED                — XDP program 挂载成功
XSKMAP_REGISTERED           — socket fd 已注册到 XSKMAP
AF_XDP_RINGS_READY          — 所有 ring 就绪
AF_XDP_FINAL_STATS          — 最终统计
bye                         — 程序正常退出
```

## 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--ifname` | 实验网卡名 | （必填） |
| `--queue` | AF_XDP 绑定的 RX 队列号 | 0 |
| `--mode` | XDP 模式：`skb`（通用）/ `native`（驱动原生） | skb |
| `--copy` | 使用 copy 模式（默认） | — |
| `--zero-copy` | 使用 zero-copy 模式 | — |
| `--duration` | 运行时间（秒） | 15 |
| `--interval` | 统计打印间隔（秒） | 1 |
| `--obj` | BPF object 文件路径 | ./build/af_xdp_kern.bpf.o |

## 已知问题

- Ubuntu 22.04 libbpf 0.5.0 不支持 `bpf_xdp_attach` / `bpf_xdp_detach`。
  代码已通过 `#if LIBBPF_VERSION < 100` 兼容层使用 `bpf_set_link_xdp_fd()` 解决。
- `asm/types.h` 头文件路径问题已通过 Makefile 中
  `BPF_CFLAGS += -I/usr/include/x86_64-linux-gnu` 解决。