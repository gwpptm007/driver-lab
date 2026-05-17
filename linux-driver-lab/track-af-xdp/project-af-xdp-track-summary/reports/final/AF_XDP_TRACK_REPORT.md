# AF_XDP Track Report

## 1. 背景

在完成 `netdev`、`track-dpdk` 后，AF_XDP track 用来补齐 Linux 原生用户态数据面能力。它关注的不是 DPDK PMD，而是 XDP + AF_XDP socket 如何把包从驱动早期路径导入用户态。

## 2. 已落地内容

| 阶段 | 内容 | 状态 |
|---|---|---|
| Phase 1 | XDP redirect basics | `PASS_BASIC_ATTACH` |
| Phase 2 | AF_XDP socket / UMEM / rings | `READY_TO_TEST` |
| Phase 3 | copy / zero-copy mode probe | `READY_TO_TEST` |
| Phase 4 | mini forwarder | `READY_TO_TEST` |
| Phase 5 | track summary | `READY` |

## 3. 技术主线

```text
netdev XDP basics
    ↓
XDP attach / PASS / DROP / REDIRECT
    ↓
XSKMAP redirect model
    ↓
AF_XDP UMEM and rings
    ↓
copy / zero-copy support boundary
    ↓
mini forwarder project
```

## 4. 当前已证明

```text
BPF/XDP 程序可以编译
XDP 可以 attach/detach 到测试网卡 ens192
skb 模式基础可用
AF_XDP socket/rings/zero-copy/mini-forwarder 项目代码和脚本已落地
目录结构已规范化，各 lab/project 自包含 docs/scripts/records/reports
```

## 5. 当前边界

```text
lab-xdp-redirect-basics 尚需补 DROP / REDIRECT dry-run
AF_XDP socket/rings 尚需测试机 records 验证
zero-copy 是否支持取决于 vmxnet3/驱动环境
mini-forwarder 尚需 smoke 和 traffic records
```

## 6. 结论

AF_XDP track 已经完成阶段性工程落地，可以作为 DPDK 之后的 Linux 原生用户态数据面学习线。后续重点不再是新增目录，而是补测试 records，把状态从 `READY_TO_TEST` 推进到 `PASS_*`。
