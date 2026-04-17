# STAGE_OVERVIEW

## stage08 核心目标

把 stage07 中偏同步的 backend 处理，推进成：

- 前端驱动 submit → 写 doorbell → backend worker 异步醒来
- backend 消费 TX / 生产 RX → backend raise irq
- NAPI poll 统一 complete / consume / refill

### 刻意不做什么

- 先不做多队列、RSS、offload
- 先不做 XDP/AF_XDP/DPDK
- 先不做复杂 tap/vhost 接入

**为什么这样收敛**：当前最值钱的不是功能广度，而是从"队列模型正确"推进到"设备边界更真实"。

---

## 前端/后端切分

### 前端（front-end）

运行在 netdev 驱动与协议栈交界处，负责：

- `ndo_start_xmit()` / TX submit / 写 doorbell
- NAPI poll 中 complete / consume / refill
- 与 Linux 网络栈交互

### 后端（back-end）

用 workqueue worker 模拟设备执行体，负责：

- 感知 doorbell（`doorbell_pending`）
- 异步消费 TX submit
- 异步生产 RX ready
- 触发 irq

### 思想映射

| stage08 | 真实设备 |
|---------|---------|
| front-end submit | 写 avail/submit ring |
| doorbell | MMIO notify/kick |
| backend worker | 设备侧执行循环 / vhost worker |
| irq raise | completion interrupt |
| NAPI poll | 驱动收割完成队列 |

---

## 异步时间线

本阶段关键是观测这条异步链路：

```
1. CPU submit (last_submit_ns)
2. CPU 写 doorbell → queue_work() (last_doorbell_ns)
3. backend worker 被调度 (last_backend_wakeup_ns)
4. backend 处理 TX→DMA_copy→RX (last_backend_done_ns)
5. backend raise irq (last_irq_ns)
6. NAPI poll 开始 (last_poll_ns)
7. TX complete / RX consume (last_complete_ns / last_consume_ns)
8. refill 完成
```

---

## 数据结构

### 6 个显式 index

**TX**: `submit_idx` → `notify_idx` → `complete_idx`
**RX**: `post_idx` → `device_idx` → `consume_idx`

### 新增状态语义

| 字段 | 含义 |
|------|------|
| `doorbell_pending` | 是否有待处理的 doorbell |
| `backend_running` | backend 是否正在运行（防重入） |
| `backend_seq` | backend 轮次计数 |
| `backend_delay_us` | 可注入的人工延迟（模拟真实设备） |

### slot 状态机

```
FREE → POSTED (RX) / SUBMITTED (TX) → DONE → FREE
```
