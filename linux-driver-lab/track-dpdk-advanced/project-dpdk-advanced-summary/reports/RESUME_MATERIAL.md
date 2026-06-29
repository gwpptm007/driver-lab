# Resume Material

## 项目条目

DPDK Advanced Data Plane Labs

- Built a staged DPDK advanced lab track covering mbuf/mempool metadata, RSS queue capability probing, burst/cache tuning methodology, VFIO/IOMMU deployment boundaries, and a lightweight L3 forwarding data plane.
- Implemented DPDK C utilities using pcap PMD and net_null PMD for reproducible packet-path validation, with structured logs, generated pcaps, and acceptance summaries.
- Developed a L3 forwarder lite with IPv4/UDP parsing, ACL drop rule, route lookup, TX burst path, and per-rule statistics.
- Documented environment limitations honestly: pcap PMD cannot validate real RSS, and the current VMware host lacks IOMMU groups for VFIO validation.

## 中文简历版

DPDK Advanced 数据面实验与小型 L3 Forwarder

- 设计并实现 DPDK 进阶实验链路，覆盖 mbuf/mempool、RSS 多队列能力探测、burst/cache 调优方法、VFIO/IOMMU 部署边界和 L3 转发数据面。
- 基于 pcap PMD / net_null PMD 构建可复现测试，保留 build/env/run/summary 证据链。
- 实现 `dpdk-l3-forwarder-lite`，支持 IPv4/UDP 解析、ACL drop、route lookup、TX burst 和 per-rule stats。
- 对 VMware/pacp PMD 环境限制做边界归档，避免把模拟环境结果夸大为真实 NIC/RSS/VFIO 生产验证。

