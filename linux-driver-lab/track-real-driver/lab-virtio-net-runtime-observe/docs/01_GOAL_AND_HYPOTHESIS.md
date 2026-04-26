# 01_GOAL_AND_HYPOTHESIS

## 实验目标

把 `lab-virtio-net-source-dive` 中关于：

- TX 主路径
- RX 主路径
- queue / NAPI / IRQ 事件推进
- feature/stats 的观察边界

这些结论，转成一批 **可重复、可对比、可评审** 的运行期记录。

## 本实验只做什么

### 做的
- 建立最小 trace 方案
- 建立最小 workload 方案
- 建立 before/after stats 采集
- 建立一份运行期证据报告

### 不做的
- 不直接改重主路径语义
- 不一开始并行搞 QEMU/vhost/back-end 全链条
- 不把实验范围扩成“所有 virtio 能力全覆盖”

## 最小假设集

### H1：RX 事件推进链可观测
`event/callback -> napi schedule -> poll -> RX process -> refill`

### H2：TX 提交与完成闭环可观测
`start_xmit -> queue submit -> notify/kick -> completion/reclaim`

### H3：不同 workload 的 trace / stats 形态可区分
- idle
- ping
- iperf3

## 为什么只选这三条

因为它们正好对应前一个 Lab 里最重要的 3 个收获：

1. 主路径不是单个函数
2. queue 是骨架
3. 真实驱动要从事件推进和资源闭环来理解
