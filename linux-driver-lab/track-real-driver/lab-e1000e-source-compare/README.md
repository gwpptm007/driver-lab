# lab-e1000e-source-compare

> 所属：`track-real-driver/`

## 一句话定位

这是 `track-real-driver` 当前的第一站：

> **把 `e1000/e1000e` 作为第二个真实驱动专题，和前面已经建立起来的 `virtio_net` 主线做结构、路径、事件模型和控制面的对照。**

## 为什么这一步排在这里

前面已经有了：

- `foundation/day01~day35`
- `netdev/stage00~stage13`

并且前序规划已经明确把 `virtio_net` 作为真实驱动第一站。  
接下来最自然的下一步，不是继续无限深挖同一条 virtio 线，而是：

- 补一个传统 PCI NIC 视角
- 建立第二个真实驱动参照系
- 形成“virtio_net vs e1000/e1000e”的对照能力

## 本 Lab 的核心目标

1. 看清 `e1000/e1000e` 的设备模型和驱动骨架
2. 建立 TX / RX / IRQ / NAPI / stats / ethtool 的结构化理解
3. 把它和 `virtio_net` 做成明确对照
4. 把它和你自己的 `netdev/stage00~stage13` 再做一轮映射

## 当前不追求的事

- 不先做大 patch
- 不先做 DPDK
- 不先做大规模性能优化
- 不并行拉太多真实 NIC 驱动

## 推荐阅读顺序

1. `START_HERE.md`
2. `docs/01_GOAL_AND_SCOPE.md`
3. `docs/02_WHY_E1000E_AFTER_VIRTIO.md`
4. `docs/03_COMPARE_DIMENSIONS.md`
5. `docs/04_READING_ORDER.md`
6. `docs/05_STAGE_AND_VIRTIO_MAPPING.md`
7. `docs/06_OBSERVE_AND_VALIDATE.md`
8. `docs/07_ACCEPTANCE_AND_REVIEW.md`
9. `reports/e1000e_compare_exec_board.md`
