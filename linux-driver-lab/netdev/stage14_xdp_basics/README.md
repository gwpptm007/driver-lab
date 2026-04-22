# stage14_xdp_basics

stage14 在 stage13 offload 基础能力基础上引入 **XDP（eXpress Data Path）**，让驱动从"与内核协议栈协作"提升为"在内核协议栈之前处理数据包"的更高性能形态。

## 一句话定位

> stage13 解决了驱动与内核协议栈的 offload 协作边界问题；stage14 解决驱动在内核协议栈之前处理数据包的 XDP 边界问题。

---

## 快速开始

```bash
cd linux-driver-lab/netdev/stage14_xdp_basics
./scripts/build.sh
./scripts/run.sh reload
./scripts/smoke.sh
```

---

## 文档导航

| 文档 | 内容 |
|------|------|
| [docs/01_STAGE_OVERVIEW.md](docs/01_STAGE_OVERVIEW.md) | stage14 学习目标、XDP 概述、与 stage13 对比 |
| [docs/02_XDP_PROGRAM_MODEL.md](docs/02_XDP_PROGRAM_MODEL.md) | xdp_buff、xdp_action、XDP hook |
| [docs/03_XDP_REDIRECT_MAPS.md](docs/03_XDP_REDIRECT_MAPS.md) | redirect、bpf_map、stats |
| [docs/04_ACCEPTANCE.md](docs/04_ACCEPTANCE.md) | 通过标准、验证方法 |
| [docs/05_DEEP_LEARNING.md](docs/05_DEEP_LEARNING.md) | AF_XDP、真实驱动对照 |
| [docs/06_BPF_BUILD_DEBUG.md](docs/06_BPF_BUILD_DEBUG.md) | build_xdp.sh 问题排查记录（asm/types.h、BTF、bash array） |

---

## 核心架构

```
Packet arrives at NIC
        ↓
[XDP] ←── stage14 新增（最早处理点，在 build_skb 之前）
        ↓
[GRO] ←── stage13 已实现
        ↓
[netif_receive_skb] ←── stage13 已实现
        ↓
[protocol stack]
```

---

## 新增功能

| 功能 | 命令 | 状态 |
|------|------|------|
| XDP program 加载 | `ip link set dev nds14s xdp obj xdp_prog.o` | ✅ |
| XDP 统计 | `ethtool -S nds14s \| grep xdp_` | ✅ |
| XDP_DROP | XDP program 返回 DROP，page 直接归还 page_pool | ✅ |
| XDP_PASS | XDP program 返回 PASS，继续走 build_skb 路径 | ✅ |
| ndo_bpf 回调 | 驱动注册 XDP handler | ✅ |
| debugfs xdp 状态 | `cat /sys/kernel/debug/netdev_stage14_soft/xdp` | ✅ |

---

## 模块参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `ifname` | `"nds14s"` | 设备名称 |
| `num_queues` | `2` | 队列数，最大 4 |
| `ring_size` | `128` | 每个 ring 的 slot 数 |
| `napi_weight` | `64` | NAPI poll budget |
| `rx_buf_size` | `2048` | RX buffer 大小 |

---

## 通过标准

1. **smoke test PASS** — 收发包正常
2. **XDP program 加载** — `ip link set dev nds14s xdp obj xdp_prog.o` 无报错
3. **xdp_pass 计数增长** — 发送数据包后 `xdp_pass` 增长
4. **xdp_drop 计数增长** — XDP DROP 程序返回 DROP 时 `xdp_drop` 增长
5. **XDP 卸载** — `ip link set dev nds14s xdp off` 成功

---

## 依赖工具

### 编译驱动（必须）

```bash
apt install build-essential linux-headers-$(uname -r)
```

### 编译 BPF 程序（加载 XDP program 必须）

XDP program 是 `.o` 文件，需要 clang/llvm 工具链编译：

```bash
# 安装 clang + llvm
sudo apt install clang llvm

# 编译 BPF object
cd bpf/
./build_xdp.sh

# 验证 .o 文件生成
ls -la *.o
```

如果测试机没有 clang，可以用 `bpf/build_xdp.sh` 预编译好 `.o` 文件后同步到测试机。

---

## XDP 验证

```bash
# 基础 XDP 检查
./scripts/xdp_check.sh

# 编译 BPF object（需要 clang + llvm）
cd bpf/
./build_xdp.sh          # 生成 xdp_pass_kern.o 和 xdp_drop_kern.o
cd ..

# 加载 XDP program
ip link set dev nds14s xdp obj bpf/xdp_pass_kern.o sec xdp_pass

# 查看 XDP 状态
ip link show nds14s
cat /sys/kernel/debug/netdev_stage14_soft/xdp

# 查看 XDP 统计
ethtool -S nds14s | grep -E "xdp_pass|xdp_drop|xdp_tx|xdp_redirect"

# 测试 XDP_DROP（tcpdump 看不到丢弃的包）
ip link set dev nds14s xdp obj bpf/xdp_drop_kern.o sec xdp_drop
ethtool -S nds14s | grep xdp_drop   # 应增长
ping -c 3 <nds14s_ip>               # 无响应
ip link set dev nds14s xdp off

# 卸载 XDP program
ip link set dev nds14s xdp off
```

---

## 目录结构

```
stage14_xdp_basics/
├── README.md              ← 主文档
├── docs/
│   ├── 01_STAGE_OVERVIEW.md
│   ├── 02_XDP_PROGRAM_MODEL.md
│   ├── 03_XDP_REDIRECT_MAPS.md
│   ├── 04_ACCEPTANCE.md
│   └── 05_DEEP_LEARNING.md
├── driver/
│   ├── netdev_stage14_soft.c
│   └── Makefile
├── include/
│   └── netdev_stage14_compat.h
├── scripts/
│   ├── build.sh
│   ├── run.sh
│   ├── smoke.sh
│   ├── xdp_check.sh       ← XDP 功能验证
│   ├── xdp_redirect_check.sh
│   └── xdp_stats_check.sh
├── tools/
│   ├── send_tool/
│   └── xdp_loader/
└── records/
```
