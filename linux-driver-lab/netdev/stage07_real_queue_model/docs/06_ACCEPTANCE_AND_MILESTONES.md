# 06_ACCEPTANCE_AND_MILESTONES

## 验收目标

stage07 的验收不能只看“能不能发一个包”，而要看队列模型是否真的成立。

## 验收项

### 验收 1：TX submit / complete 正确
证明：
- 提交 index 正确推进
- 完成 index 正确推进
- 不出现重复回收和漏回收

### 验收 2：RX post / consume / refill 正确
证明：
- post 与 consume 不混
- consume 与 refill 不混
- queue 生命周期闭环

### 验收 3：irq / napi / completion 分界清楚
证明：
- irq 只做触发
- poll 做批处理
- completion 在 poll 路径中有清晰位置

### 验收 4：队列状态异常可观测
证明：
- ring full / empty 有统计
- drop 有统计
- budget 用尽有统计

### 验收 5：与 stage04 差异可解释
必须有单独文档说明：
- 为什么要从 owner 模型推进到 index 模型
- 为什么 stage07 更接近真实驱动

### 验收 6：与 virtio-net 的映射站得住
必须有文档解释：
- avail/used 思想映射
- kick/interrupt 思想映射
- RX buffer posting 思想映射

## 里程碑

### M1：queue skeleton
- desc/slot/queue 数据结构定型
- helper 框架建立

### M2：lifecycle first pass
- TX submit/complete 打通
- RX post/consume/refill 打通

### M3：notify/irq/napi 稳定
- irq 触发路径清晰
- NAPI budget 正常
- 基本 smoke 通过

### M4：observability + review
- stats 固定
- trace 样本固定
- stage04_vs_stage07 文档完成
- virtio mapping 文档完成

## 最终阶段通过标准

> 代码、观测、文档、对照四条线同时成立，stage07 才算真正完成。
