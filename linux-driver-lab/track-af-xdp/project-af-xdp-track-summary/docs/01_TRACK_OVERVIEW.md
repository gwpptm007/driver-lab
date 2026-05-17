# AF_XDP Track Overview

## 一句话定位

`track-af-xdp` 用来承接前面的 `netdev/stage14_xdp_basics` 和 `track-dpdk`，补齐 Linux 原生用户态数据面路径：

```text
XDP attach
    ↓
XDP redirect / XSKMAP
    ↓
AF_XDP socket
    ↓
UMEM + FILL/RX/TX/COMPLETION rings
    ↓
copy / zero-copy 能力探测
    ↓
mini forwarder 项目化
```

## 和 DPDK 的关系

DPDK 依赖 PMD、hugepage、轮询收发，常绕开内核网络栈；AF_XDP 则是 Linux 原生路径，通过 XDP 在驱动收包早期把包 redirect 到用户态 XSK socket。

对简历和面试来说，两条线可以合在一起表达：

```text
我既理解 DPDK 这种独立用户态 PMD 数据面，
也理解 Linux 原生 AF_XDP 如何复用内核驱动/XDP，把包导入用户态处理。
```

## 当前 track 内容

| 阶段 | 目录 | 定位 |
|---|---|---|
| Phase 1 | `lab-xdp-redirect-basics` | XDP attach、PASS/DROP/REDIRECT 模型打底 |
| Phase 2 | `lab-af-xdp-socket-rings` | AF_XDP socket、UMEM、rings 基础 |
| Phase 3 | `lab-af-xdp-zero-copy-vs-copy` | copy / zero-copy 能力探测和 fallback |
| Phase 4 | `project-af-xdp-mini-forwarder` | 项目型 drop/reflect mini forwarder |
| Phase 5 | `project-af-xdp-track-summary` | 阶段性报告、面试材料、简历素材 |
