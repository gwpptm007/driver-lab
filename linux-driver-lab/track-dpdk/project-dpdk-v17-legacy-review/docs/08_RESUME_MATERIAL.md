# 08_RESUME_MATERIAL

## 简历项目名称建议

```text
DPDK 用户态媒体面转发与现代化迁移项目
```

或者：

```text
DPDK User-space Fastpath / Media Gateway Lite
```

## 简历描述版本 1：偏工程实现

```text
基于 DPDK 构建用户态数据面实验与媒体网关原型，完成 vmxnet3 PMD 接管、hugepage/mempool/mbuf 初始化、rx_burst/tx_burst 批量收发、vhost-user/virtio-user 本机虚拟链路、自研 l2fwd-lite 与 fastpath-lite。设计 media-gateway-lite，将端口初始化、Ethernet/IPv4/UDP 解析、UDP-only 过滤、MAC/IP/UDP 头部改写、per-port/per-rule 统计拆分为独立模块，并通过 records 区分 PASS_SMOKE、PASS_TRAFFIC、PASS_FORWARDING、PASS_REWRITE 验收等级。
```

## 简历描述版本 2：结合旧项目经验

```text
结合既有 DPDK v17 媒体面项目经验，复盘并迁移 UDP 高速收发、ARP/IP/UDP 解析、头部改写和按方向转发能力到现代 DPDK 21.11 环境；完成从 testpmd、vhost-user/virtio-user 到自研 fastpath/media-gateway-lite 的可复现工程链路，沉淀 UIO/VFIO/KNI/vhost 差异、mbuf 所有权、统计验收与异常包处理等数据面设计文档。
```

## 简历 bullet 精简版

```text
- 基于 DPDK 设计用户态 fastpath / media-gateway-lite，完成 hugepage、mempool、mbuf、PMD、rx_burst/tx_burst、vhost-user/virtio-user 等关键路径验证。
- 实现 Ethernet/IPv4/UDP 分类、UDP-only 过滤、MAC/IP/UDP 头部改写框架和 per-port/per-rule/drop reason 统计，支持 vmxnet3、vdev/null、vhost/virtio 测试入口。
- 结合 DPDK v17 历史项目经验，梳理 KNI、UIO/VFIO、vhost-user、virtio-user 的使用边界，并形成现代 DPDK 迁移复盘和面试材料。
```

## 当前状态表述注意

不要写：

```text
已完成完整 DPDK 媒体网关转发系统。
```

当前更准确写法：

```text
已完成 DPDK media-gateway-lite 项目型骨架和双端口 smoke 验证，后续补真实 UDP 流量、转发和 rewrite records。
```

## 面试展开关键词

```text
hugepage
mempool
mbuf
PMD
burst
UIO/VFIO
KNI
vhost-user
virtio-user
UDP fastpath
rewrite
drop reason
records-driven acceptance
```
