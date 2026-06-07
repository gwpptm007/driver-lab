# track-af-xdp START_HERE

2026-06-07 复测更新：四个 Phase 全部通过。

建议阅读顺序：

```text
1. ROADMAP.md — 整体路线和当前状态
2. docs/ — track 级别文档
3. project-af-xdp-track-summary/ — 阶段总收口（报告/面试/简历素材）
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
