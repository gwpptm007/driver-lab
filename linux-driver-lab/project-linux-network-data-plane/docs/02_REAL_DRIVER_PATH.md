# Real Driver Path

## 路径定位

Real driver path 用来把教学型 netdev 经验迁移到真实 Linux 网络驱动源码中。重点不是重写一个真实驱动，而是阅读、映射、观测和做低风险 patch。

这条路径要回答：

```text
netdev stage 中学到的 queue、NAPI、TX/RX、ethtool、XDP 等概念，
在真实驱动 virtio_net 和 e1000e 中分别落在哪里？
```

## 覆盖范围

对应主线：

```text
linux-driver-lab/track-real-driver/
```

主要实验：

| Lab/Project | 目标 | 状态 |
|-------------|------|------|
| `lab-virtio-net-source-dive` | 阅读 `virtio_net` 架构、probe、TX/RX、queue、NAPI、feature、XDP | 已完成 Round1~3 |
| `lab-virtio-net-runtime-observe` | 运行期观测 TX/RX 和 trace 事件 | 已测试 |
| `lab-virtio-net-ethtool-stats-mini-patch` | 做低风险 ethtool stats patch 和 before/after 对照 | 已测试 |
| `lab-virtio-net-queue-poll-observe` | 观测 `napi_poll -> netif_receive_skb` 链 | 已测试 |
| `project-virtio-net-patch-and-trace` | 整合 patch、trace、风险说明和 final note | patch 已生成 |
| `lab-e1000e-source-compare` | 对照 `virtio_net` 和 `e1000e` 的真实 NIC 差异 | 已完成 |

## 关键机制

这条路径关注：

- `virtnet_probe()` 如何创建并注册 netdev。
- virtqueue 与 netdev queue 的关系。
- RX/TX 路径如何和 skb、NAPI 结合。
- ethtool stats 如何暴露驱动内部指标。
- `napi_poll`、`netif_receive_skb` 等事件如何被 trace 验证。
- `virtio_net` 与 `e1000e` 在设备模型、DMA/ring、interrupt、offload 上的差异。

## 已证明内容

已形成的能力：

```text
教学 netdev stage -> virtio_net 源码映射
真实驱动运行期观测
低风险 stats patch 选点
before/after 对照思路
napi_poll 链路 trace 证据
virtio_net vs e1000e 对照阅读
```

## Evidence 入口

主要证据索引：

- `../../track-real-driver/README.md`
- `../../track-real-driver/project-virtio-net-patch-and-trace/reports/final_project_report.md`
- `../../track-real-driver/project-virtio-net-patch-and-trace/reports/review_bundle.md`
- `../../track-real-driver/lab-e1000e-source-compare/reports/e1000e_compare_report.md`
- [../evidence/real_driver_evidence.md](../evidence/real_driver_evidence.md)

## 当前边界

准确表述：

- 已完成真实驱动源码阅读、运行期观测和低风险 patch 练习。
- 能把教学驱动模型映射到真实驱动中的函数、结构和路径。

不要夸大：

- 不是大规模重构 `virtio_net` 或 `e1000e`。
- 当前 patch 属于 stats/control plane 类型，不是性能路径深度改造。
