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
