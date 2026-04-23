# virtio_net_next_step_exec_board

| 优先级 | 方向 | 目标 | 当前建议 |
|---|---|---|---|
| P1 | ethtool / stats 小 patch | 低风险、易验证、体现真实驱动理解 | 最先做 |
| P2 | tracing / 观测增强 | 把 Round2 / Round3 结论变成运行期证据 | 第二步做 |
| P3 | queue / poll 观测实验 | 验证事件推进模型 | 第三步做 |

## 当前不建议

- 直接改很重的 TX/RX 主路径语义
- 一上来并行多个真实 NIC 专题
- 一开始就把 QEMU/vhost/back-end 全链条一起吃掉
