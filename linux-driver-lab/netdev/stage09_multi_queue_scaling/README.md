# stage09_multi_queue_scaling

stage09 是 `netdev` 第二阶段在 stage08_async_backend_transport 之后的继续推进。

它的目标不是追求更高吞吐，而是把当前已经成立的：
- 单队列 queue lifecycle
- front-end / back-end 分离
- doorbell
- 异步 backend work
- IRQ -> NAPI poll

推进到：
- **多 TX / 多 RX queue**
- **每队列独立 NAPI**
- **每队列独立 backend worker**
- **简单但可解释的流量分发策略**
- **队列级统计与分布观测**

## 一句话定位

> stage08 解决"单队列异步 transport"；stage09 解决"多队列与队列分发"。

---

## 快速开始

```bash
cd linux-driver-lab/netdev/stage09_multi_queue_scaling
./scripts/build.sh
./scripts/run.sh reload
./scripts/smoke.sh
./scripts/queue_dist_check.sh records/<latest>
./scripts/timeline_check.sh records/<latest>
```

详细步骤见 [START_HERE.md](START_HERE.md)。

---

## 文档导航（6 个文件）

| 文档 | 内容 |
|------|------|
| [docs/01_STAGE_OVERVIEW.md](docs/01_STAGE_OVERVIEW.md) | 架构总览、与 stage08 的区别、核心设计决策 |
| [docs/02_QUEUE_MODEL.md](docs/02_QUEUE_MODEL.md) | per-queue 数据结构、TX/RX ring 模型、timeline、stats |
| [docs/03_FLOW_DISTRIBUTION_AND_NAPI.md](docs/03_FLOW_DISTRIBUTION_AND_NAPI.md) | 队列选择策略、per-queue NAPI、TX/RX/backend 路径详解 |
| [docs/04_DRIVER_LAYOUT.md](docs/04_DRIVER_LAYOUT.md) | struct 布局、module 参数、init/exit 流程、构建运行 |
| [docs/05_ACCEPTANCE.md](docs/05_ACCEPTANCE.md) | 通过标准、验证方法、典型失败分析 |
| [docs/06_DEEP_LEARNING.md](docs/06_DEEP_LEARNING.md) | 深层指南、queue affinity、下一步方向 |

---

## 核心架构

```
                      ┌─────────────────────────────────────────────────────────────┐
                      │                    struct stage09_priv                     │
                      │  state_lock / backend_wq / debugfs / num_queues=2           │
                      └──────────┬──────────────────────────────────────┬──────────┘
                                 │                                      │
                    ┌────────────▼────────────┐        ┌────────────────▼────────────┐
                    │    struct stage09_queue │        │    struct stage09_queue    │
                    │           q0            │        │           q1                │
                    │  ┌──────────────────┐  │        │  ┌──────────────────┐       │
                    │  │  txq: submit_idx │  │        │  │  txq: submit_idx │       │
                    │  │  rxq: post_idx   │  │        │  │  rxq: post_idx   │       │
                    │  │  napi            │  │        │  │  napi            │       │
                    │  │  backend_work    │  │        │  │  backend_work    │       │
                    │  │  timeline        │  │        │  │  timeline        │       │
                    │  │  stats (30+)     │  │        │  │  stats (30+)     │       │
                    │  └──────────────────┘  │        │  └──────────────────┘       │
                    └───────────────────────┘        └────────────────────────────┘

ndo_select_queue:
  skb_get_hash() != 0  ──→  hash % num_queues  ──→  hash-based 分发（保序）
  skb_get_hash() == 0  ──→  rr_counter++ % n    ──→  round-robin 兜底
```

---

## 模块参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `ifname` | `"nds9"` | 设备名称 |
| `num_queues` | `2` | 队列数，最大 4 |
| `ring_size` | `128` | 每个 ring 的 slot 数 |
| `napi_weight` | `64` | NAPI poll budget |
| `backend_delay_us` | `0` | backend 处理延迟（微秒），>0 观测异步阶梯 |
| `backend_batch` | `64` | backend 每次最多处理帧数 |

使用示例：
```bash
insmod netdev_stage09.ko num_queues=4 backend_delay_us=100
```

---

## 通过标准（v1）

1. **编译通过**：driver + userspace tools 无 error
2. **加载成功**：设备注册，debugfs 文件可读
3. **多队列活跃**：>= 2 个队列有 `tx_submit > 0`
4. **异步成立**：`doorbell_to_backend_ns > 0`（至少一个队列）
5. **资源回收**：测试结束后所有计数器归零

详见 [docs/06_ACCEPTANCE_AND_DEEP_LEARNING.md](docs/06_ACCEPTANCE_AND_DEEP_LEARNING.md)。

---

## 项目状态

### P0 — 已完成
- [x] stage09 目录、文档、脚本、tools、driver v1 已落地
- [x] 默认 2 queue 起步
- [x] 每 queue 独立 NAPI / backend_work / stats / timeline
- [x] 提供 queue distribution 检查脚本

### P1 — 测试中
- [ ] 第一轮测试编译加载
- [ ] 第一轮 smoke 和 queue distribution 记录
- [ ] 基于测试结果修正 driver / smoke harness

### P2 — 规划中
- [ ] queue affinity / CPU 绑核
- [ ] 更稳定的 test-flow 专属统计
- [ ] stage08 vs stage09 差异报告

---

## 目录结构

```
stage09_multi_queue_scaling/
├── README.md              ← 主文档（含完整目录结构）
├── START_HERE.md         ← 快速导航
├── docs/
│   ├── 01_STAGE_OVERVIEW.md
│   ├── 02_QUEUE_MODEL.md
│   ├── 03_FLOW_DISTRIBUTION_AND_NAPI.md
│   ├── 04_DRIVER_LAYOUT.md
│   ├── 05_BUILD_RUN_AND_OBSERVABILITY.md
│   └── 06_ACCEPTANCE_AND_DEEP_LEARNING.md
├── driver/
│   ├── netdev_stage09.c
│   └── Makefile
├── include/
│   └── netdev_stage09_compat.h
├── scripts/
│   ├── build.sh
│   ├── run.sh
│   ├── smoke.sh
│   ├── queue_dist_check.sh
│   ├── timeline_check.sh
│   └── trace_smoke.sh
├── tools/
│   ├── send_stage09_frame.c
│   └── recv_stage09_frame.c
├── records/
├── reports/
└── workdir/
```
