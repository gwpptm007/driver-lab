# app: AF_XDP socket rings 实验程序

## 目录结构

本目录包含两个源码文件 + Makefile：

| 文件 | 语言 | 说明 |
|------|------|------|
| `af_xdp_kern.bpf.c` | C (BPF) | 内核侧 XDP program，redirect 到 XSKMAP |
| `af_xdp_rings.c` | C | 用户态程序，创建 UMEM + 四类 ring 并 poll 收包 |
| `Makefile` | Make | 编译 BPF (.o) 和用户态程序 |
| `README.md` | — | 本文件 |

## 编译

```bash
make
```

输出：
- `build/af_xdp_kern.bpf.o` — BPF 目标文件（内核使用）
- `build/af_xdp_rings` — 用户态可执行文件

## 运行

```bash
# 基础 smoke（skb + copy 模式，15 秒）
sudo ./build/af_xdp_rings \
    --ifname ens192 \
    --queue 0 \
    --mode skb \
    --copy \
    --duration 15 \
    --interval 1 \
    --obj ./build/af_xdp_kern.bpf.o
```

## 预期输出（smoke 通过标记）

```
UMEM_READY                   — UMEM 创建成功
XSK_SOCKET_READY            — AF_XDP socket 创建成功
FILL_RING_READY             — FILL ring 填充成功
XDP_ATTACHED                — XDP program 挂载成功
XSKMAP_REGISTERED           — socket fd 已注册到 XSKMAP
AF_XDP_RINGS_READY          — 所有 ring 就绪
AF_XDP_STATS                — 周期统计（运行时每 interval 秒打印）
AF_XDP_FINAL_STATS          — 最终统计（rx_packets / rx_bytes）
bye                         — 程序正常退出
```

## 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--ifname` | 实验网卡名 | （必填） |
| `--queue` | AF_XDP 绑定的 RX 队列号 | 0 |
| `--mode` | XDP 模式：`skb`（通用）/ `native`（驱动原生） | skb |
| `--copy` | 使用 copy 模式（替代 `--zero-copy`） | copy |
| `--zero-copy` | 使用 zero-copy 模式（依赖网卡驱动支持） | — |
| `--duration` | 运行时间（秒） | 15 |
| `--interval` | 统计打印间隔（秒） | 1 |
| `--obj` | BPF object 文件路径 | ./build/af_xdp_kern.bpf.o |

## 与 lab-af-xdp-socket-rings 的关系

本 app 是 `lab-af-xdp-socket-rings` 实验的核心。
`lab-af-xdp-zero-copy-vs-copy` 在此基础上增加 `--mode` / `--zero-copy` 探测。

## 已知问题

- Ubuntu 22.04 libbpf 0.5.0 不支持 `bpf_xdp_attach` / `bpf_xdp_detach`。
  代码已通过 `#if LIBBPF_VERSION < 100` 兼容层使用 `bpf_set_link_xdp_fd()` 解决。
- `asm/types.h` 头文件路径问题已通过 Makefile 中
  `BPF_CFLAGS += -I/usr/include/x86_64-linux-gnu` 解决。