# stage07 PLAN

## 总目标

把 stage04 的教学型 netdev 推进成更接近真实队列驱动模型的版本。

## 执行顺序

1. 先定数据结构与 queue helper
2. 再定 notify / irq / napi / completion 分工
3. 再做最小可运行闭环
4. 再补 stats / trace / report
5. 最后输出 stage04_vs_stage07 与 virtio mapping 评审文档

## 先不做的事

- 多队列
- offload
- XDP
- 极限性能优化
