# DPDK_V17_LEGACY_REVIEW

## 一句话总结

本报告用于把 DPDK v17 旧项目经验和当前 `track-dpdk` modern DPDK 学习项目打通，形成一条可解释、可验证、可写进简历的用户态数据面能力线。

## 旧项目能力抽象

```text
DPDK v17 media-plane
  -> UDP packet RX
  -> ARP/IP/UDP classify
  -> direction/rule match
  -> MAC/IP/UDP rewrite
  -> TX forward
  -> stats/audit
  -> optional KNI/kernel assist path
```

## 当前 track 复现链路

```text
lab-vmxnet3-testpmd
  -> physical/virtual PMD 接管

lab-vhost-user-basic
  -> vhost-user backend

lab-virtio-user-vhost
  -> virtio-user frontend + vhost socket

lab-dpdk-l2-forwarding
  -> 自写 DPDK C app

project-user-space-fastpath
  -> 协议分类与 rewrite 框架

project-dpdk-media-gateway-lite
  -> 模块化媒体网关原型，当前 PASS_SMOKE
```

## 当前状态

```text
media-gateway-lite:
  PASS_SMOKE
  PASS_UDP_ONLY_DROP_PATH

待补:
  PASS_TRAFFIC
  PASS_FORWARDING
  PASS_REWRITE
```

## 核心结论

1. 旧 DPDK v17 项目经验可以抽象成用户态 UDP 媒体面 fastpath。
2. 当前 modern track 已经复现了 DPDK 环境、PMD 接管、vhost/virtio-user、自写 C app 和 media gateway 原型。
3. 当前不足在真实 traffic/rewrite records，而不是工程方向。
4. 后续补齐真实 UDP 流量后，可形成较完整的 DPDK 用户态数据面作品。
