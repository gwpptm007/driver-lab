# project-user-space-fastpath_report

> 状态更新：本报告保留最初的 VMXNET3 smoke 记录；后续 `project-fastpath-traffic-test` 已使用同一 fastpath binary 完成 `PASS_PCAP_FUNCTIONAL/FORWARDING/REWRITE`。

## 目标

把 `track-dpdk` 的 lab 收口成一个项目型用户态 fastpath：

```text
VMXNET3/testpmd → vhost-user → virtio-user → l2fwd-lite → fastpath-lite
```

## 当前实现

`fastpath-lite` 已具备：

- EAL 初始化
- mempool / mbuf
- ethdev / RXQ / TXQ
- poll-mode loop
- 单端口 smoke
- 双端口配对转发
- ARP / IPv4 / UDP 分类
- UDP-only 过滤
- MAC / IPv4 / UDP port rewrite
- 软件 stats 与 rte_eth_stats

## 当前验收层级

| 层级 | 含义 |
|---|---|
| PASS_SMOKE | 编译、初始化、loop、stats 正常 |
| PASS_PROJECT | UDP-only/rewrite 参数和记录闭环正常 |
| PASS_FORWARDING | 有外部流量或双口，RX/TX/rewrite 计数非 0 |

## 当前测试机约束

当前 VMware 测试机默认只有一个专用 VMXNET3 DPDK 口，因此第一轮以 `PASS_SMOKE` 和 `PASS_PROJECT` 为主。

## 后续增强

- 用 vhost/virtio-user 接入双端口验证
- 增加 scapy/pktgen 发包脚本
- 增加 rte_hash flow table
- 增加控制面 JSON 配置
- 增加 perf/topdown 观测
