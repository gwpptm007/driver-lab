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

---

## 各 Track 定位

| Track | 定位 | 当前状态 |
|-------|------|----------|
| `track-real-driver/` | 真实 Linux NIC 驱动源码与 patch | 4 labs + 1 project 完成 |
| `track-virtual-net/` | vhost/kick/notify + tap/bridge 协同 | 3 labs + 1 project 完成 |
| `track-dpdk/` | DPDK 用户态网络 | 规划中 |
| `track-af-xdp/` | AF_XDP 快速路径 | 规划中 |
| `track-ebpf-observability/` | eBPF 可观测性 | 规划中 |

---

## 下一步推荐

### 当前最推荐的下一步

**`track-real-driver/lab-virtio-net-source-dive/`**

承接你已经完成的 `netdev/stage00~stage14`，把"自己写教学驱动"推进到"阅读真实 Linux NIC 驱动源码"。

### 当前推荐的实验入口

- `track-real-driver/lab-virtio-net-runtime-observe/README.md` — 运行期观测基线
- `track-real-driver/lab-virtio-net-ethtool-stats-mini-patch/README.md` — 第一个真实 ethtool/stats 小 patch

### 进入方式

```
track-real-driver/lab-virtio-net-source-dive/START_HERE.md
```

先跑符号索引脚本，再看 probe/TX/RX，再做 stage 映射。

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