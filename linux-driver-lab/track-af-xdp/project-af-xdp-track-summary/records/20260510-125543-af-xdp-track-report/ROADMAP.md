# track-af-xdp ROADMAP

## Phase 1: lab-xdp-redirect-basics

状态：`PASS_BASIC_ATTACH`。

已证明：

- BPF 程序可编译；
- XDP 可 attach/detach 到 `ens192`；
- `skb` 模式可用。

待补测：

- `XDP_DROP.log`；
- `XDP_REDIRECT_DRYRUN.log`；
- 非 0 包统计。

## Phase 2: lab-af-xdp-socket-rings

状态：`READY_TO_TEST / 测试结果后续分析`。

目标：

- 创建 UMEM；
- 创建 AF_XDP socket；
- 初始化 FILL/RX/TX/COMPLETION rings；
- XDP redirect 到 XSKMAP；
- 用户态 poll RX ring。

## Phase 3: lab-af-xdp-zero-copy-vs-copy

状态：`READY_TO_TEST / 测试结果后续分析`。

目标：比较 `skb/copy`、`native/copy`、`native/zero-copy` 支持边界。

## Phase 4: project-af-xdp-mini-forwarder

状态：`READY_TO_TEST`。

目标：把 AF_XDP socket/rings 能力整理成 mini forwarder。

第一版验收：

- PASS_BUILD；
- PASS_DROP_SMOKE；
- PASS_REFLECT_SMOKE；
- PASS_TRAFFIC 后续补测；
- PASS_TX_REFLECT 后续补测。

## Phase 5: project-af-xdp-traffic-test

计划：给 mini forwarder 补真实 traffic 与 veth/namespace 测试闭环。


## Phase 5: project-af-xdp-track-summary

状态：`READY`。

目标：把 AF_XDP track 当前阶段收口成可读、可归档、可面试表达的项目材料。

输出：

- `AF_XDP_TRACK_REPORT.md`；
- `AF_XDP_PROJECT_PORTFOLIO.md`；
- `AF_XDP_INTERVIEW_NOTES.md`；
- `AF_XDP_RESUME_MATERIAL.md`；
- `AF_XDP_BACKLOG.md`。

原则：

```text
不把 summary 文档散放在 track-af-xdp 根目录；
summary 作为独立 project，自己维护 docs/reports/scripts/records。
```
