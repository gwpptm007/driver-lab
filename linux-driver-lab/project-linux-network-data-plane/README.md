# Linux Network Data Plane Project

> Linux 网络数据面学习与作品集总收口项目。

## 一句话定位

本项目把 `linux-driver-lab` 中已经完成的网络相关主线收束成一个完整作品：

```text
kernel netdev
  -> real driver
  -> virtual network
  -> DPDK userspace fastpath
  -> AF_XDP native fastpath
  -> eBPF observability
```

它回答的核心问题是：

```text
一个 Linux 网络包从驱动、内核协议栈、虚拟化路径、用户态 fastpath 到观测定位，
分别有哪些关键机制？这些机制如何被实验、代码、脚本和 records 证明？
```

## 项目目标

形成一个可以对外展示、可以面试讲解、可以继续补测的 Linux network data plane 作品集。

本目录不再新增一条孤立实验线，而是作为总入口，负责：

- 汇总 netdev、真实驱动、虚拟化网络、DPDK、AF_XDP、eBPF observability 六条路径。
- 解释它们在 Linux 网络数据面中的位置和边界。
- 建立 evidence 索引，指向已有 track 中的代码、脚本、records 和报告。
- 输出最终报告、简历材料和面试讲稿。

## 能力地图

| 层级 | 核心问题 | 已覆盖能力 |
|------|----------|------------|
| Kernel netdev | 内核网络驱动如何接入协议栈 | `net_device`, `skb`, NAPI, ring, multi-queue, MSI-X, page_pool, ethtool, offload, XDP |
| Real driver | 教学驱动经验如何迁移到真实驱动 | `virtio_net` source dive, runtime observe, ethtool stats patch, NAPI trace, `e1000e` compare |
| Virtual network | host/guest 网络路径如何协同 | tap, bridge, virtio frontend, vhost backend, kick/notify, L2 forwarding |
| DPDK fastpath | 如何绕过内核协议栈做用户态数据面 | hugepage, PMD, testpmd, vhost-user, virtio-user, l2fwd-lite, media-gateway-lite |
| AF_XDP path | Linux 原生 fastpath 如何进入用户态 | XDP attach, XSKMAP redirect, AF_XDP socket, UMEM, rings, mini forwarder |
| eBPF observability | 如何观测 RX/TX/drop 路径 | per-interface stats, per-CPU distribution, GRO/TX/drop reason, path invariant |

## 推荐阅读顺序

1. [docs/00_PROJECT_OVERVIEW.md](docs/00_PROJECT_OVERVIEW.md)
2. [docs/07_FINAL_ARCHITECTURE.md](docs/07_FINAL_ARCHITECTURE.md)
3. [reports/final_report.md](reports/final_report.md)
4. [reports/resume_material.md](reports/resume_material.md)
5. [docs/08_INTERVIEW_SHARE_SCRIPT.md](docs/08_INTERVIEW_SHARE_SCRIPT.md)
6. [evidence/README.md](evidence/README.md)

## 章节入口

| 文档 | 内容 |
|------|------|
| [00_PROJECT_OVERVIEW.md](docs/00_PROJECT_OVERVIEW.md) | 总体目标、路径设计、交付物 |
| [01_KERNEL_NETDEV_PATH.md](docs/01_KERNEL_NETDEV_PATH.md) | 内核 netdev 主线 |
| [02_REAL_DRIVER_PATH.md](docs/02_REAL_DRIVER_PATH.md) | 真实驱动源码和 patch 主线 |
| [03_VIRTUAL_NET_PATH.md](docs/03_VIRTUAL_NET_PATH.md) | tap/bridge/vhost 虚拟化网络主线 |
| [04_DPDK_FASTPATH.md](docs/04_DPDK_FASTPATH.md) | DPDK 用户态 fastpath 主线 |
| [05_AF_XDP_PATH.md](docs/05_AF_XDP_PATH.md) | AF_XDP 原生 fastpath 主线 |
| [06_EBPF_OBSERVABILITY.md](docs/06_EBPF_OBSERVABILITY.md) | eBPF 网络路径观测主线 |
| [07_FINAL_ARCHITECTURE.md](docs/07_FINAL_ARCHITECTURE.md) | 最终架构和横向对比 |
| [08_INTERVIEW_SHARE_SCRIPT.md](docs/08_INTERVIEW_SHARE_SCRIPT.md) | 面试讲法 |
| [09_LIMITATIONS_AND_NEXT_STEPS.md](docs/09_LIMITATIONS_AND_NEXT_STEPS.md) | 当前边界与下一步 |

## 对外交付

| 文件 | 用途 |
|------|------|
| [reports/final_report.md](reports/final_report.md) | 最终项目报告 |
| [reports/resume_material.md](reports/resume_material.md) | 简历项目描述和 bullet |
| [reports/review_checklist.md](reports/review_checklist.md) | 自查和评审清单 |
| [evidence/README.md](evidence/README.md) | 证据索引总入口 |

## 当前边界

本项目强调的是“实验型数据面能力链”和“可复现证据”，不是生产级网络栈或生产级 DPDK 网关。

准确表述：

- 已完成 Linux 网络数据面多路径学习、实验、观测和作品化收口。
- 已具备从教学驱动到真实驱动、从内核路径到用户态 fastpath、从转发到观测定位的系统理解。
- DPDK media-gateway-lite 已验证 pcap PMD 路径下的 UDP traffic/forwarding/rewrite。
- AF_XDP 已验证 XDP redirect、UMEM/rings 和 mini forwarder。

不要夸大成：

- 生产级 DPDK 媒体网关。
- 完整真实网卡大规模性能压测。
- 已覆盖所有 NIC offload、RSS、多线程调度、IOMMU/VFIO 生产部署。
