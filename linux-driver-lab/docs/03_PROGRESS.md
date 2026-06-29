# 03_PROGRESS

> 当前进度与完成度矩阵。

## 当前完成度总览

| 阶段 | 目录 | 完成度 | 关键里程碑 |
|---|---|---|---|
| W1 字符设备 | `foundation/day01~day07` | 已完成 | 字符设备驱动闭环 |
| W2 平台/DT/IRQ | `foundation/day08~day14` | 已完成 | platform_driver + ftrace |
| W3 baseline/裁剪 | `foundation/day15~day21` | 已完成 | 工程化 baseline + 回归 |
| W4 PCIe | `foundation/day22~day28` | 已完成 | ivshmem-doorbell + MSI |
| W5 DMA/性能 | `foundation/day29~day35` | 已完成 | DMA/mmap/perf/ftrace/stability |
| netdev 主线 | `netdev/stage00~stage14` | 已完成 | stage14 XDP 入口收口 |
| track-real-driver | 4 labs + 1 project | 已完成 | virtio_net 源码深潜 |
| track-virtual-net | 3 labs + 1 project | 已完成 | vhost/kick/notify + L2 转发 |
| track-af-xdp | 4 phases | 已完成 | 全部 PASS |
| track-dpdk | 9 phases | 已完成 | media-gateway-lite PASS_TRAFFIC |
| track-ebpf-observability | 5 phases | 已完成 | 全部 COMPLETED |
| project-linux-network-data-plane | 总收口项目 | 已封版 | network-data-plane-v1 |
| track-dpdk-advanced | 6 phases | 已收敛 | Phase 1/3/5 PASS，Phase 2/4 boundary evidence，Phase 6 final report |
| track-block-io | P2 支线 | PARKED_PLANNED | block layer / storage I/O 规划保留 |

## 当前推荐入口

```text
track-dpdk-advanced/project-dpdk-advanced-summary/reports/DPDK_ADVANCED_FINAL_REPORT.md
track-dpdk-advanced/project-dpdk-advanced-summary/reports/EVIDENCE_INDEX.md
track-dpdk-advanced/project-dpdk-l3-forwarder-lite/README.md
```

## 下一条主线建议

DPDK Advanced 已收敛，下一条高价值主线建议进入 RDMA Core：

```text
track-rdma-core
  -> verbs / MR / QP / CQ
  -> RC pingpong
  -> UD / RoCEv2 model
  -> perftest evidence when hardware is available
```

保留原则：

- 没有硬件能力时先做 boundary evidence。
- 有硬件后补真实性能记录。
- 不把模拟环境结果包装成生产硬件结果。

