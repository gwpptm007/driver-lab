# ACCEPTANCE & TASKS

## 验收标准

stage07 的验收不能只看"能不能发一个包"，而要看队列模型是否真的成立。

### 验收 1：TX submit / complete 正确

证明：
- [ ] 提交 index 正确推进
- [ ] 完成 index 正确推进
- [ ] 不出现重复回收和漏回收

### 验收 2：RX post / consume / refill 正确

证明：
- [ ] post 与 consume 不混
- [ ] consume 与 refill 不混
- [ ] queue 生命周期闭环

### 验收 3：irq / napi / completion 分界清楚

证明：
- [ ] irq 只做触发
- [ ] poll 做批处理
- [ ] completion 在 poll 路径中有清晰位置

### 验收 4：队列状态异常可观测

证明：
- [ ] ring full / empty 有统计
- [ ] drop 有统计
- [ ] budget 用尽有统计

### 验收 5：与 stage04 差异可解释

- [ ] 为什么要从 owner 模型推进到 index 模型
- [ ] 为什么 stage07 更接近真实驱动

### 验收 6：与 virtio-net 的映射站得住

- [ ] avail/used 思想映射
- [ ] kick/interrupt 思想映射
- [ ] RX buffer posting 思想映射

---

## 里程碑

### M1：queue skeleton
- [x] desc/slot/queue 数据结构定型
- [x] helper 框架建立

### M2：lifecycle first pass
- [x] TX submit/complete 打通
- [x] RX post/consume/refill 打通

### M3：notify/irq/napi 稳定
- [x] irq 触发路径清晰
- [x] NAPI budget 正常
- [x] 基本 smoke 通过

### M4：observability + review
- [x] stats 固定
- [x] trace / debug dump 项固定
- [x] stage04_vs_stage07 差异说明
- [x] virtio mapping 文档完成

---

## P0-P4 任务状态

### P0：阶段起步必须完成
- [x] 建立 `stage07_real_queue_model/` 独立目录
- [x] 明确本阶段不做多队列、不做 offload、不追求大而全
- [x] 固定单 TX queue + 单 RX queue + 单 NAPI 实例模型
- [x] 明确 `submit/notify/complete/post/consume/refill` 生命周期

### P1：核心代码与数据结构
- [x] 设计 `struct stage07_desc`
- [x] 设计 `struct stage07_queue`
- [x] 设计 TX submit/notify/complete index
- [x] 设计 RX post/device/consume index
- [x] 设计 ring full / empty 判断规则
- [x] 明确 buffer ownership 切换

### P2：中断、通知、poll 关系
- [x] 明确 doorbell/notify 入口
- [x] 明确 irq 仅做触发，不做复杂数据路径
- [x] 明确 NAPI poll 的 budget 消费规则
- [x] 明确 TX completion 回收时机
- [x] 明确 RX refill 与 RX consume 的边界

### P3：文档与可观测性
- [ ] 输出 queue model 图
- [x] 输出 stage04 -> stage07 差异说明
- [x] 输出与 `virtio-net` 的映射说明
- [x] 固定 stats 项
- [x] 固定 trace / debug dump 项

### P4：验收
- [ ] TX submit / complete 可重复验证
- [ ] RX post / consume / refill 可重复验证
- [x] irq / napi / completion 边界清楚
- [x] stage04 与 stage07 差异可解释

---

## 通过标准

> **代码、观测、文档、对照四条线同时成立，stage07 才算真正完成。**

### 验收检查单

```bash
# 1. TX 路径验证
# submit_idx == notify_idx == complete_idx → TX 生命周期无泄漏

# 2. RX 路径验证
# rx_post_count == rx_refill_count → refill 无泄漏
# rx_no_posted == 0 → backend 从不缺 posted buffer

# 3. NAPI/IRQ 验证
# irq_count == napi_schedule_count == napi_poll_count == napi_complete_count

# 4. 零异常验证
# tx_dropped == 0, rx_dropped == 0, rx_truncated == 0
```
