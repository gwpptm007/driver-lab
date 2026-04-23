# virtio_net_patch_candidates_seed

> 这不是最终 patch 计划，而是一份“在当前 Lab 完成后，最适合继续落地的切入点种子”。

## 候选方向 1：ethtool / stats 小改动
优点：
- 风险低
- 行为容易验证
- 非常适合作为真实驱动小 patch 第一站

## 候选方向 2：增加/强化 trace 观测脚本
优点：
- 不直接改驱动语义
- 有助于把 Round2/Round3 的理解变成运行期证据
- 适合先做函数级或 tracepoint 级观测

## 候选方向 3：围绕 queue / poll 的小型观测增强
优点：
- 和当前 Lab 的主线连续性强
- 适合验证 queue / callback / poll 的事件推进理解

## 当前不建议作为第一站的方向
- 直接挑过重的主路径语义 patch
- 一开始就并行拉上多个真实 NIC 驱动做复杂对比
- 一上来就把 virtio front-end/back-end/QEMU/vhost 全链条一起啃完
