# stage12_ethtool_control_plane

stage12 在 stage11 RX page_pool 基础上引入 **ethtool 控制面**，让驱动从"能收发包"提升为"可配置、可查询、可操作"的真实设备形态。

## 一句话定位

> stage11 解决了 RX buffer 生命周期问题；stage12 解决驱动与用户空间的标准交互接口问题。

---

## 快速开始

```bash
cd linux-driver-lab/netdev/stage12_ethtool_control_plane
./scripts/build.sh
./scripts/run.sh reload
./scripts/smoke.sh
```

---

## 文档导航

| 文档 | 内容 |
|------|------|
| [docs/01_STAGE_OVERVIEW.md](docs/01_STAGE_OVERVIEW.md) | stage12 学习目标、ethtool 概述、与 stage11 对比 |
| [docs/02_QUEUE_MODEL.md](docs/02_QUEUE_MODEL.md) | ethtool_ops 架构、stats 导出流程、RX slot 状态机 |
| [docs/03_ACCEPTANCE.md](docs/03_ACCEPTANCE.md) | 通过标准、ethtool 验证方法、调试命令 |
| [docs/04_DEEP_LEARNING.md](docs/04_DEEP_LEARNING.md) | ethtool_ops 深度解析、真实驱动对照 |

---

## 核心架构

```
用户空间                    驱动
   │                         │
   │ ethtool -i nds12s      │
   │ ───────────────────────> │ get_drvinfo()
   │ <─────────────────────── │ 驱动信息
   │                         │
   │ ethtool -S nds12s      │
   │ ───────────────────────> │ get_strings() + get_sset_count() + get_ethtool_stats()
   │ <─────────────────────── │ 标准统计
   │                         │
   │ ethtool -G nds12s      │
   │ ───────────────────────> │ get_ringparam()
   │ <─────────────────────── │ ring_size=128
   │                         │
   │ ethtool -L nds12s      │
   │ ───────────────────────> │ get_channels()
   │ <─────────────────────── │ rx_count=2, tx_count=2
```

---

## 新增功能

| 功能 | 命令 | 状态 |
|------|------|------|
| 驱动信息 | `ethtool -i nds12s` | ✅ |
| 标准统计 | `ethtool -S nds12s` | ✅ |
| ring 参数 | `ethtool -G nds12s` | ✅ (只读) |
| channel 数 | `ethtool -L nds12s` | ✅ (只读) |
| 私有标志 | `ethtool --show-priv-flags nds12s` | ✅ |

---

## 模块参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `ifname` | `"nds12s"` | 设备名称 |
| `num_queues` | `2` | 队列数，最大 4 |
| `ring_size` | `128` | 每个 ring 的 slot 数 |
| `napi_weight` | `64` | NAPI poll budget |
| `rx_buf_size` | `2048` | RX buffer 大小 |

---

## 通过标准

1. **ethtool -i** 显示 `driver: netdev_stage12`
2. **ethtool -S** 显示所有统计项
3. **多队列分发** >= 2 队列 `tx_submit` 增量 > 0
4. **异步链路** `doorbell_to_backend_ns > 0`
5. **page_pool** 正常工作，无 build_skb_fail

---

## ethtool 验证

```bash
# 基础 ethtool 检查
./scripts/ethtool_check.sh

# 详细测试（含 channel 切换）
IFNAME=nds12s ./scripts/ethtool_check.sh . exercise-channels
```

---

## 目录结构

```
stage12_ethtool_control_plane/
├── README.md              ← 主文档
├── docs/
│   ├── 01_STAGE_OVERVIEW.md
│   ├── 02_QUEUE_MODEL.md
│   ├── 03_ACCEPTANCE.md
│   └── 04_DEEP_LEARNING.md
├── driver/
│   ├── netdev_stage12_soft.c
│   └── Makefile
├── include/
│   └── netdev_stage12_compat.h
├── scripts/
│   ├── build.sh
│   ├── run.sh
│   ├── smoke.sh
│   ├── ethtool_check.sh    ← ethtool 专项测试
│   ├── queue_dist_check.sh
│   ├── vector_check.sh
│   ├── timeline_check.sh
│   └── pp_check.sh
├── tools/
│   ├── send_stage12_frame.c
│   └── recv_stage12_frame.c
└── records/
```