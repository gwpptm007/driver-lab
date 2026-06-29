# 01_PROGRAMS

> 当前项目阶段、各 track 定位、下一步方向

## 当前项目阶段

### 第一阶段：foundation（已收口）
- `foundation/day01~day35`
- 已冻结为第一阶段基线，不再线性追加 day36+
- 覆盖：字符设备 / 平台驱动/DT/IRQ / baseline工程化 / PCIe基本功 / DMA/mmap/性能

### 第二阶段：netdev（已完成 stage00~stage14）
- `netdev/stage00~stage14`
- 覆盖：net_device / skb / NAPI / ring / multi-queue / MSI-X / page_pool / ethtool / offload / XDP 入口
- stage14 是 netdev 线性 stage 主线的收口点

### 第三阶段：track / lab / project
- 不再继续扩成 stage15 stage16...
- 改为：`track-real-driver/`、`track-virtual-net/`、`track-af-xdp/`、`track-dpdk/` 等

### 第四阶段：network acceleration（规划中）
- `project-linux-network-data-plane` 已用 `network-data-plane-v1` 标签封版
- 当前后续主线调整为：`track-dpdk-advanced/` -> `track-rdma/` -> `track-smartnic-dpu/`
- `track-block-io/` 先作为 P2 保留支线，不作为当前主线推进

---

## 各 Track 定位

| Track | 定位 | 当前状态 |
|-------|------|----------|
| `track-real-driver/` | 真实 Linux NIC 驱动源码与 patch | 4 labs + 1 project 完成 |
| `track-virtual-net/` | vhost/kick/notify + tap/bridge 协同 | 3 labs + 1 project 完成 |
| `track-dpdk/` | DPDK 用户态网络 | 9 phases, media-gateway-lite PASS_TRAFFIC/FORWARDING/REWRITE |
| `track-dpdk-advanced/` | DPDK 进阶：mbuf/mempool、RSS、多队列、NUMA、VFIO/IOMMU | PLANNED |
| `track-af-xdp/` | AF_XDP 快速路径 | 4 phases 全部 PASS (2026-06-07) |
| `track-ebpf-observability/` | eBPF 可观测性 | 5 phases 全部 COMPLETED (2026-06-07) |
| `track-block-io/` | block layer / storage I/O | PARKED_PLANNED, P2 支线 |

---

## 下一步推荐

### 当前最推荐的下一步

**`track-dpdk-advanced/lab-dpdk-mbuf-mempool-deep-dive/`**

承接已经封版的 network data plane，把 DPDK 从“能跑通 fastpath”推进到“理解 mbuf/mempool、队列、RSS、NUMA、VFIO/IOMMU 和真实部署边界”。

### 当前推荐的阅读入口

- `docs/08_ACCELERATION_ROADMAP.md` — DPDK -> RDMA -> SmartNIC/DPU 总路线
- `track-dpdk-advanced/README.md` — DPDK 进阶主线定位
- `track-dpdk-advanced/START_HERE.md` — 第一阶段进入方式
- `track-dpdk-advanced/docs/04_RDMA_SMARTNIC_BRIDGE.md` — 后续 RDMA / SmartNIC 衔接

### 进入方式

```
track-dpdk-advanced/START_HERE.md
```

先从 mbuf/mempool metadata 深挖开始，不急着做 RSS、多队列或 VFIO 环境改造。

---

## 阶段完成度一句话版

| 阶段 | 完成度 |
|------|--------|
| W1（字符设备） | ✅ 完整收住 |
| W2（平台/DT/IRQ） | ✅ 形成嵌入式平台驱动基本功闭环 |
| W3（baseline/裁剪/回归） | ✅ 把demo拉成工程化实验平台 |
| W4（PCIe） | ✅ 已是可单独对外讲的作品线 |
| W5（DMA/性能/稳定性） | ✅ 推到第一阶段成熟上限 |
| netdev stage00~stage14 | ✅ 完成，stage14 收口 |
| track-real-driver | ✅ 第一轮完成 |
| track-virtual-net | ✅ 完成 |
| project-linux-network-data-plane | ✅ 已封版 network-data-plane-v1 |
| track-dpdk-advanced | 🧭 已规划，待启动 |
| track-block-io | 🅿️ 已规划，P2 保留 |

---

## 当前最强的地方

1. **主线连续**：day间有因果推进，不是散乱demo
2. **证据意识强**：`records/`、输出物、报告体系完整
3. **W3后工程化**：baseline、自动化、对比、风险控制都已建立
4. **W4/W5作品化**：功能闭环 + 用户态配套 + 性能指标 + 稳定性证据

---

## 当前边界与误判警惕

1. **强项边界**：强的是"实验型驱动 + PCIe/DMA作品线"，还没进真实子系统深水区（net_device、blk_mq、MSI-X、IOMMU等）
2. **表达分寸**：W5的优化收益是"用户态访问路径优化"，不要夸大成"驱动DMA引擎本身被显著重构"
3. **目录边界**：不要再堆 foundation/day36+，会模糊基础/扩展阶段边界
