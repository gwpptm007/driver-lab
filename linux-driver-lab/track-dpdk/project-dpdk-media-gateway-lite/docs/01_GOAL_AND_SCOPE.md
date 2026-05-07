# 01_GOAL_AND_SCOPE

## 目标

实现一个简化版 DPDK 媒体网关，用于承接过往 DPDK v17 媒体面经验，并形成现代 DPDK 作品。

## 功能范围

- UDP-only fast path
- ARP/IPv4/UDP 解析
- MAC/IP/UDP port rewrite
- 双方向规则
- per-rule/per-port/drop stats
- 配置文件驱动

## 不做

- 完整 SIP/RTP 协议栈
- 完整 NAT ALG
- 多进程控制面
- 高级 rte_flow offload

这些可以放到后续增强阶段。
