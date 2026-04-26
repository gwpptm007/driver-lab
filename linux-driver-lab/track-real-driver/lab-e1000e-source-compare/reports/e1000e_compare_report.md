# e1000e_compare_report

## 目标

这是 `lab-e1000e-source-compare` 的总报告入口。

## 当前建议结构

1. 为什么在 `virtio_net` 之后看 `e1000/e1000e`
2. 设备模型对照
3. 驱动骨架对照
4. TX/RX / IRQ/NAPI 对照
5. ethtool/stats/control-plane 对照
6. 与自己 `netdev stage` 的映射
7. 最终结论

## 一句话定位

这是：
- 第二个真实驱动专题
- 传统 NIC vs 半虚拟化 NIC 的对照入口
- 进入 `track-virtual-net` 之前的重要准备
