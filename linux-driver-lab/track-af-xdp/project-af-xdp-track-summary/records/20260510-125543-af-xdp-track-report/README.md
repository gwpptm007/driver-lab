# track-af-xdp

> AF_XDP / XDP 用户态快速路径主线。

## 一句话定位

承接前面的 `netdev/stage14_xdp_basics` 和 `track-dpdk`：

- `netdev` 解决内核 `net_device/NAPI/skb/XDP` 基础；
- `track-dpdk` 解决用户态 PMD/hugepage/poll mode；
- `track-af-xdp` 解决 **Linux 原生 XDP + AF_XDP socket** 的用户态收发路径。

## 当前阶段

| 阶段 | 目录 | 状态 |
|---|---|---|
| Phase 1 | `lab-xdp-redirect-basics` | PASS_BASIC_ATTACH，DROP/REDIRECT 后续补测 |
| Phase 2 | `lab-af-xdp-socket-rings` | READY_TO_TEST / 测试结果后续分析 |
| Phase 3 | `lab-af-xdp-zero-copy-vs-copy` | READY_TO_TEST / 测试结果后续分析 |
| Phase 4 | `project-af-xdp-mini-forwarder` | READY_TO_TEST |
| Phase 5 | `project-af-xdp-track-summary` | READY |

## 推荐推进顺序

```text
lab-xdp-redirect-basics
    ↓
lab-af-xdp-socket-rings
    ↓
lab-af-xdp-zero-copy-vs-copy
    ↓
project-af-xdp-mini-forwarder
    ↓
project-af-xdp-track-summary  ← 当前阶段性收口
```

## 当前第四站目标

`project-af-xdp-mini-forwarder` 把前面 lab 能力串成项目型数据面：

- XDP redirect 到 XSKMAP；
- 用户态 AF_XDP socket；
- UMEM / FILL / RX / TX / COMPLETION rings；
- drop 模式验证 RX + recycle；
- reflect 模式验证 TX + completion；
- review bundle 记录。


## 阶段性总结

AF_XDP track 的阶段性报告、作品集说明、面试材料、简历素材和 backlog 统一放在：

```text
track-af-xdp/project-af-xdp-track-summary/
```

该目录自包含 `docs/ reports/ scripts/ records/`，`track-af-xdp/` 根目录不放通用 `records/ reports/ scripts/`。
