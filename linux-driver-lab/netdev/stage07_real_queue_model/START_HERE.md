# START_HERE — stage07_real_queue_model

## 现在先看什么

1. `docs/01_STAGE_OVERVIEW.md` — 目标、队列模型、数据结构、virtio映射
2. `docs/02_USER_GUIDE.md` — 使用方式、build/run、统计项
3. `docs/03_ACCEPTANCE.md` — 验收标准和检查单
4. `driver/netdev_stage07.c` — 代码实现
5. `docs/04_DEEP_LEARNING.md` — 深度分析

## 这次已经落了什么

这次不是只给方向图纸，而是已经把第一版 stage07 核心代码落下来了：

- queue index 已经进入代码
- TX submit / notify / complete 已经进入代码
- RX post / device write / consume / refill 已经进入代码
- debugfs stats / queues 已经进入代码
- build/run/smoke 脚本已经补齐

## 现在先回答的四个问题

### Q1：为什么还要开 stage07？
因为 `stage04` 已经把 ring / DMA / refill 做出来了，但它仍偏教学模型；要想更接近真实驱动，必须把 queue 生命周期再推进一层。

### Q2：这阶段最重要的不是功能，而是什么？
是：
- index 模型
- queue 生命周期
- notify / completion 语义
- 与 `virtio-net` 的结构映射

### Q3：这次 v1 的真实落点是什么？
已经把"单队列真实化"的第一跳落出来了：
- 提交和完成分开
- post 和 consume 分开
- device/backend 和 CPU/NAPI 的边界分开

### Q4：下一步最该继续做什么？
不是立刻扩大功能面，而是：
- 跑通 smoke
- 固定观测项
- 再把 queue helper 和 virtio 映射解释打磨得更清楚

## 快速测试

```bash
./scripts/build.sh
./scripts/run.sh reload
./scripts/smoke.sh
./scripts/stats_check.sh
```
