# USER_GUIDE

## 快速开始

### 阅读顺序

1. `docs/01_STAGE_OVERVIEW.md` — 目标、队列模型、数据结构、virtio映射
2. `docs/03_ACCEPTANCE.md` — 验收标准和检查单
3. `driver/netdev_stage07.c` — 代码实现
4. `docs/07_DEEP_LEARNING.md` — 深度分析

### 测试流程

```bash
# 1. 编译 + 运行
./scripts/build.sh
./scripts/run.sh reload

# 2. smoke 测试
./scripts/smoke.sh

# 3. 观测验证
./scripts/stats_check.sh
./scripts/trace_smoke.sh
```

---

## scripts 规划

### `build.sh`
- 解析环境
- 编译 `driver/netdev_stage07.c`
- 生成 `output/netdev_stage07.ko`

### `run.sh`
- `load` / `unload` / `reload` / `status`

### `smoke.sh`
- 触发一轮最小 TX/RX
- 抓取 dmesg 与统计
- 判断通过/失败

### `stats_check.sh`
- 读取 debugfs 统计
- 核对 index / queue / poll / irq 计数

---

## 建议统计项

| 统计项 | 含义 |
|--------|------|
| `tx_submit_count` | TX 提交次数 |
| `tx_complete_count` | TX 完成次数 |
| `rx_post_count` | RX post 次数 |
| `rx_consume_count` | RX 消费次数 |
| `rx_refill_count` | RX refill 次数 |
| `irq_count` | irq 触发次数 |
| `napi_schedule_count` | NAPI schedule 次数 |
| `napi_poll_count` | NAPI poll 次数 |
| `napi_budget_exhaust_count` | budget 用尽次数 |
| `ring_full_count` | 队列满次数 |
| `ring_empty_count` | 队列空次数 |
| `tx_dropped` | TX 丢包数 |
| `rx_dropped` | RX 丢包数 |

---

## 阶段计划

1. 先定数据结构与 queue helper
2. 再定 notify / irq / napi / completion 分工
3. 再做最小可运行闭环
4. 再补 stats / trace / report
5. 最后输出 stage04_vs_stage07 与 virtio mapping 评审文档

### 先不做的事

多队列、offload、XDP、极限性能优化

---

## 当前版本边界

当前 v1 仍然是教学型伪设备：

- backend 仍用 `memcpy` 模拟 device DMA copy
- 不做多队列
- 不做 RSS / offload / XDP
- 不追求极限吞吐

但它已经把 stage07 最关键的队列边界落下来了。

---

## 输出建议

### `output/`
- ko、smoke log、stats dump、short report

### `records/`
- 某次 smoke 的完整日志
- 某次回归对比
- 某次 trace 样本

### `reports/`
- acceptance、stage04_vs_stage07、virtio mapping review
