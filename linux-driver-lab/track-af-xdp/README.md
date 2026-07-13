# track-af-xdp

> AF_XDP / XDP 用户态快速路径主线。

## 一句话定位

承接前面的 `netdev/stage14_xdp_basics` 和 `track-dpdk`：

- `netdev` 解决内核 `net_device/NAPI/skb/XDP` 基础；
- `track-dpdk` 解决用户态 PMD/hugepage/poll mode；
- `track-af-xdp` 解决 **Linux 原生 XDP + AF_XDP socket** 的用户态收发路径。

## 第一次进入先读这里

先打开 [docs/fundamentals/README.md](docs/fundamentals/README.md)，按“内核 RX/XDP -> eBPF/XSKMAP -> UMEM -> 四环 ownership -> COPY/ZC -> 多队列/NUMA -> 排障”的顺序建立完整模型，再进入 Phase 1。

该知识层包含 13 个主题，状态：`AF_XDP_FUNDAMENTALS_COMPLETE`。

## 当前状态（2026-06-07 复测）

所有四个 Phase 均使用 veth pair 拓扑完成复测，全部通过。

| 阶段 | 目录 | 状态 | 关键数据 |
|---|---|---|---|
| Phase 0 | `docs/fundamentals/` | **PASS** | 详细知识底座、62 张原理图、项目映射与排障手册 |
| Phase 1 | `lab-xdp-redirect-basics` | **PASS** | PASS: 12 pkts, DROP: 3 pkts, REDIRECT: 3 pkts |
| Phase 2 | `lab-af-xdp-socket-rings` | **PASS** | UMEM 8MB, rx_packets=49 |
| Phase 3 | `lab-af-xdp-zero-copy-vs-copy` | **PASS** | skb+copy: 3 pkts, native+copy: 3 pkts, ZC: unsupported |
| Phase 4 | `project-af-xdp-mini-forwarder` | **PASS** | DROP: rx=3/drop=3, REFLECT: rx=3/tx=3/comp=3 |
| Phase 5 | `project-af-xdp-track-summary` | **READY** | 总报告、面试材料、backlog |

## 推荐推进顺序

```text
docs/fundamentals/README.md
    ↓
lab-xdp-redirect-basics
    ↓
lab-af-xdp-socket-rings
    ↓
lab-af-xdp-zero-copy-vs-copy
    ↓
project-af-xdp-mini-forwarder
    ↓
project-af-xdp-track-summary  ← 阶段性收口
```

完整知识入口：

```text
docs/fundamentals/00_15_MINUTE_MENTAL_MODEL.md
docs/fundamentals/01_KERNEL_RX_AND_XDP_POSITION.md
docs/fundamentals/02_EBPF_VERIFIER_MAPS_AND_LOADER.md
docs/fundamentals/03_SOCKET_UMEM_AND_FRAME_LAYOUT.md
docs/fundamentals/04_FOUR_RINGS_AND_OWNERSHIP.md
docs/fundamentals/05_XSKMAP_REDIRECT_AND_QUEUE_BINDING.md
docs/fundamentals/06_COPY_ZEROCOPY_AND_DRIVER_DMA.md
docs/fundamentals/07_TX_REFLECT_AND_NEED_WAKEUP.md
docs/fundamentals/08_MULTIQUEUE_RSS_AND_SHARED_UMEM.md
docs/fundamentals/09_CONCURRENCY_AND_MEMORY_ORDER.md
docs/fundamentals/10_PERFORMANCE_NUMA_AND_MEASUREMENT.md
docs/fundamentals/11_DEBUGGING_PLAYBOOK.md
docs/fundamentals/12_PROJECT_MAP_AND_RECALL_CARDS.md
```

## 数据路径全览

```text
XDP program (attach to veth-xdp)
    │
    ├── XDP_PASS/DROP/REDIRECT (Phase 1)
    │
    └── bpf_redirect_map(xsks_map, queue_id) → AF_XDP socket (Phase 2)
                                                   │
                                                   ├── UMEM (8MB, 4096 frames)
                                                   ├── FILL ring → 提交空闲 frame
                                                   ├── RX ring → 收到包
                                                   ├── TX ring → 发送包 (Phase 4 reflect)
                                                   └── COMPLETION ring → TX 完成回收 (Phase 4)
```

## 阶段性总结

AF_XDP track 的阶段性报告、作品集说明、面试材料、简历素材和 backlog 统一放在：

```text
track-af-xdp/project-af-xdp-track-summary/
```

该目录自包含 `docs/ reports/ scripts/ records/`，`track-af-xdp/` 根目录不放通用 `records/ reports/ scripts/`。

本次知识层测试记录见 `tests/TEST_RECORD_20260714_AF_XDP_FUNDAMENTALS.md`。
