# DPDK_RESUME_BULLETS

## 可直接放简历的版本

```text
DPDK 用户态数据面与媒体网关原型：基于 Ubuntu 22.04 / DPDK 21.11 环境完成 vmxnet3 PMD 接管、hugepage/mempool/mbuf 初始化、vhost-user/virtio-user 本机虚拟链路、自研 l2fwd-lite 与 fastpath-lite；设计 media-gateway-lite，将端口初始化、协议解析、UDP-only 过滤、MAC/IP/UDP 改写、per-port/per-rule 统计模块化，并结合 DPDK v17 历史项目经验整理 KNI、UIO/VFIO、vhost/virtio 迁移复盘。
```

## 更保守准确版本

```text
DPDK 用户态数据面实验与迁移复盘：完成 vmxnet3/testpmd、vhost-user、virtio-user、自研 l2fwd-lite/fastpath-lite 和 media-gateway-lite smoke 验证，覆盖 hugepage、mempool、mbuf、PMD、rx_burst/tx_burst、UDP-only、rewrite 框架、drop reason stats 等关键机制；结合 DPDK v17 媒体面经验输出现代 DPDK 迁移设计和面试材料。
```

## 面试官看重的关键词

```text
DPDK
PMD
hugepage
mempool
mbuf
rx_burst / tx_burst
vhost-user
virtio-user
KNI
UIO / VFIO
UDP fastpath
media gateway
rewrite
records-driven acceptance
```
