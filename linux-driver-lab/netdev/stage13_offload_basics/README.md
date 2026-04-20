# stage13_offload_basics

stage13 在 stage12 ethtool 控制面基础上引入 **offload 基础能力**，让驱动从"能收发包"提升为"可声明能力、可协作协议栈"的真实设备形态。

## 一句话定位

> stage12 解决了驱动与用户空间的标准交互接口问题；stage13 解决驱动与内核协议栈的 offload 协作边界问题。

---

## 快速开始

```bash
cd linux-driver-lab/netdev/stage13_offload_basics
./scripts/build.sh
./scripts/run.sh reload
./scripts/smoke.sh
```

---

## 文档导航

| 文档 | 内容 |
|------|------|
| [docs/01_STAGE_OVERVIEW.md](docs/01_STAGE_OVERVIEW.md) | stage13 学习目标、offload 概述、与 stage12 对比 |
| [docs/02_OFFLOAD_MODEL.md](docs/02_OFFLOAD_MODEL.md) | NETIF_F 架构、ip_summed 状态机、GRO vs netif_receive_skb |
| [docs/03_ACCEPTANCE.md](docs/03_ACCEPTANCE.md) | 通过标准、offload 验证方法、调试命令 |
| [docs/04_DEEP_LEARNING.md](docs/04_DEEP_LEARNING.md) | offload_ops 深度解析、真实驱动对照 |

---

## 核心架构

```
用户空间                    驱动                      内核协议栈
   │                         │                           │
   │ ethtool -k nds13s      │                           │
   │ ─────────────────────> │ get_flags()               │
   │ <───────────────────── │ rx-checksum: on          │
   │                         │ tx-checksum: on          │
   │                         │ sg: on, gso: on, gro: on  │
   │                         │                           │
   │ ethtool -K nds13s gro off                           │
   │ ─────────────────────> │ ndo_set_features()        │
   │                         │ ──────────────────────>   │
   │                         │  features updated        │
   │                         │                           │
   │                         │ RX: build_skb()          │
   │                         │  → ip_summed=UNNECESSARY │
   │                         │  → napi_gro_receive()    │
   │                         │      (或 netif_receive)  │
```

---

## 新增功能

| 功能 | 命令 | 状态 |
|------|------|------|
| offload 能力声明 | `ethtool -k nds13s` | ✅ |
| GRO 开关 | `ethtool -K nds13s gro on/off` | ✅ |
| checksum 状态标记 | `skb->ip_summed` | ✅ |
| feature 协商 | `ndo_set_features` | ✅ |
| offload 统计 | `ethtool -S nds13s` | ✅ |

---

## 模块参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `ifname` | `"nds13s"` | 设备名称 |
| `num_queues` | `2` | 队列数，最大 4 |
| `ring_size` | `128` | 每个 ring 的 slot 数 |
| `napi_weight` | `64` | NAPI poll budget |
| `rx_buf_size` | `2048` | RX buffer 大小 |

---

## 通过标准

1. **ethtool -k** 显示 `rx-checksum`, `tx-checksum`, `sg`, `gso`, `gro`
2. **ethtool -K gro off** 后 `rx_gro_packets` 不增长
3. **ethtool -K gro on** 后 `rx_gro_packets` 增长
4. **ethtool -K** 触发 `feature_set_count++`
5. **smoke test** 收发包正常

---

## offload 验证

```bash
# 基础 offload 检查
./scripts/offload_check.sh

# GRO 开关实验
./scripts/offload_experiment.sh

# 详细测试（含 channel 切换）
IFNAME=nds13s ./scripts/ethtool_check.sh
```

---

## 目录结构

```
stage13_offload_basics/
├── README.md              ← 主文档
├── docs/
│   ├── 01_STAGE_OVERVIEW.md
│   ├── 02_OFFLOAD_MODEL.md
│   ├── 03_ACCEPTANCE.md
│   └── 04_DEEP_LEARNING.md
├── driver/
│   ├── netdev_stage13_soft.c
│   └── Makefile
├── include/
│   └── netdev_stage13_compat.h
├── scripts/
│   ├── build.sh
│   ├── run.sh
│   ├── smoke.sh
│   ├── offload_check.sh    ← offload 专项测试
│   ├── offload_experiment.sh ← GRO 开关实验
│   ├── ethtool_check.sh
│   ├── queue_dist_check.sh
│   ├── vector_check.sh
│   ├── timeline_check.sh
│   └── pp_check.sh
├── tools/
│   ├── send_stage13_frame.c
│   └── recv_stage13_frame.c
└── records/
```
