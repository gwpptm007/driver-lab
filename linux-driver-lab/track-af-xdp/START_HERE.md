# track-af-xdp START_HERE

2026-06-07 复测更新：四个 Phase 全部通过。

建议阅读顺序：

```text
1. docs/fundamentals/README.md — 完整项目前知识层
2. docs/fundamentals/00_15_MINUTE_MENTAL_MODEL.md — 快速心智模型
3. docs/fundamentals/01_KERNEL_RX_AND_XDP_POSITION.md — 内核 RX/XDP 位置
4. docs/fundamentals/03_SOCKET_UMEM_AND_FRAME_LAYOUT.md — UMEM/frame
5. docs/fundamentals/04_FOUR_RINGS_AND_OWNERSHIP.md — 四环 ownership
6. docs/fundamentals/06_COPY_ZEROCOPY_AND_DRIVER_DMA.md — COPY/ZC 边界
7. docs/fundamentals/12_PROJECT_MAP_AND_RECALL_CARDS.md — 项目映射
8. ROADMAP.md — 整体路线和当前状态
9. project-af-xdp-track-summary/ — 阶段总收口
```

知识层审计 marker：`AF_XDP_FUNDAMENTALS_COMPLETE`。

```bash
bash tests/check_fundamentals.sh
```

快速进入任意 Phase：

```bash
# Phase 1: XDP redirect basics
cd track-af-xdp/lab-xdp-redirect-basics
cat docs/01_LAB_OVERVIEW.md

# Phase 2: AF_XDP socket rings
cd track-af-xdp/lab-af-xdp-socket-rings
cat docs/01_LAB_OVERVIEW.md

# Phase 3: zero-copy vs copy
cd track-af-xdp/lab-af-xdp-zero-copy-vs-copy
cat docs/01_LAB_OVERVIEW.md

# Phase 4: mini forwarder
cd track-af-xdp/project-af-xdp-mini-forwarder
cat docs/01_LAB_OVERVIEW.md

# Track summary
cd track-af-xdp/project-af-xdp-track-summary
cat README.md
```

veth pair 测试拓扑（所有 Phase 通用）：

```bash
sudo ip link add veth-xdp type veth peer name veth-peer
sudo ip link set veth-xdp up && sudo ip link set veth-peer up
sudo ip addr add 10.99.0.2/24 dev veth-peer
```

详见 Phase 1 的 `docs/05_VETH_PAIR_DEEP_DIVE.md`
