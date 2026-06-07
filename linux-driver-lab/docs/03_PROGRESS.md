# 03_PROGRESS

> 当前进度与完成度矩阵

## 当前完成度总览

| 阶段 | 目录 | 完成度 | 关键里程碑 |
|------|------|--------|-----------|
| W1 字符设备 | foundation/day01~day07 | ✅ 完成 | 字符设备驱动闭环 |
| W2 平台/DT/IRQ | foundation/day08~day14 | ✅ 完成 | platform_driver + ftrace |
| W3 baseline/裁剪 | foundation/day15~day21 | ✅ 完成 | 工程化 baseline + 回归 |
| W4 PCIe | foundation/day22~day28 | ✅ 完成 | ivshmem-doorbell + MSI |
| W5 DMA/性能 | foundation/day29~day35 | ✅ 完成 | DMA/mmap/perf/ftrace/stability |
| netdev 主线 | netdev/stage00~stage14 | ✅ 完成 | stage14 XDP 入口收口 |
| track-real-driver | 4 labs + 1 project | ✅ 完成 | virtio_net 源码深潜 |
| track-virtual-net | 3 labs + 1 project | ✅ 完成 | vhost/kick/notify + L2 转发 |
| track-af-xdp | 4 phases | ✅ 完成 | 全部 PASS (2026-06-07) |
| track-dpdk | 9 phases | ✅ 完成 | media-gateway-lite PASS_TRAFFIC (pcap PMD) |
| track-ebpf-observability | 5 phases | ✅ 完成 | 全部 COMPLETED (2026-06-07) |

---

## 当前最推荐的下一步

### 第一个真实驱动 Lab

`track-real-driver/lab-virtio-net-source-dive/`

- 承接 netdev/stage00~stage14
- 把"自己写教学驱动"推进到"阅读真实 Linux NIC 驱动源码"
- 进入方式：`track-real-driver/lab-virtio-net-source-dive/START_HERE.md`

### 当前推荐的实验入口

1. `track-real-driver/lab-virtio-net-runtime-observe/README.md` — 运行期观测基线
2. `track-real-driver/lab-virtio-net-ethtool-stats-mini-patch/README.md` — 第一个真实 ethtool/stats 小 patch 实验

---

## 后续主题优先级

### 第一优先级：netdev 主线深化

继续 netdev 主线到 stage14，把 Linux NIC 驱动关键机制系统吃透

### 第二优先级：真实 NIC 驱动源码与 patch 线

把教学项目能力迁移到真实工业驱动

### 第三优先级：块层 / 存储线

作为第二高价值主线，与 netdev 构成高性能 I/O 双主线

### 第四优先级：虚拟化网络 / 宿主机协同线

与云环境、虚拟化网络高度相关

---

## 完成后期望形成的能力结构

### 网络驱动能力
- netdev / NAPI / multi-queue / MSI-X / page_pool / XDP
- ethtool / offload / IRQ affinity
- real NIC driver source dive / patch

### 高性能 I/O 驱动能力
- network I/O + block I/O
- queue/completion 统一理解
- DMA / MSI-X / 多队列 / 生命周期管理

### 系统协同能力
- 虚拟化网络 / 宿主机网络路径
- qdisc / tc / bridge / veth / tap
- front-end / back-end 模型

### 调试与问题定位能力
- trace / perf / ftrace / tcpdump
- interrupts / softnet_stat / ethtool -S
- queue imbalance / drop root cause / budget exhaustion