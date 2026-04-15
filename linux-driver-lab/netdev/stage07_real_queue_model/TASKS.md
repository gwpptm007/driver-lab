# stage07 TASKS

## P0：阶段起步必须完成

- [x] 建立 `stage07_real_queue_model/` 独立目录
- [x] 明确本阶段不做多队列、不做 offload、不追求大而全
- [x] 固定单 TX queue + 单 RX queue + 单 NAPI 实例模型
- [x] 明确 `submit/notify/complete/post/consume/refill` 生命周期

## P1：核心代码与数据结构

- [x] 设计 `struct stage07_desc`
- [x] 设计 `struct stage07_queue`
- [x] 设计 TX submit index / complete index
- [x] 设计 RX post index / consume index
- [x] 设计 ring full / empty 判断规则
- [x] 明确 buffer ownership 切换

## P2：中断、通知、poll 关系

- [x] 明确 doorbell/notify 入口
- [x] 明确 irq 仅做触发，不做复杂数据路径
- [x] 明确 NAPI poll 的 budget 消费规则
- [x] 明确 TX completion 回收时机
- [x] 明确 RX refill 与 RX consume 的边界

## P3：文档与可观测性

- [ ] 输出 queue model 图
- [x] 输出 stage04 -> stage07 差异说明
- [x] 输出与 `virtio-net` 的映射说明
- [x] 固定 stats 项
- [x] 固定 trace / debug dump 项

## P4：验收

- [ ] TX submit / complete 可重复验证
- [ ] RX post / consume / refill 可重复验证
- [x] irq / napi / completion 边界清楚
- [x] stage04 与 stage07 差异可解释

## 当前下一步

1. 在真实测试机上用 `scripts/build.sh` + `scripts/run.sh reload` 落模块
2. 用 `scripts/smoke.sh` 跑第一轮闭环
3. 固定 `records/` 中的 smoke 证据
4. 根据结果再修订 queue helper 细节
