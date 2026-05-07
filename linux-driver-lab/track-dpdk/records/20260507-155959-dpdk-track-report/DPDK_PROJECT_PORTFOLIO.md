# DPDK_PROJECT_PORTFOLIO

## 作品线定位

这条 DPDK track 不是单个 demo，而是一条从环境、PMD、虚拟化数据面、自写 C fastpath 到媒体网关原型和旧项目迁移复盘的作品线。

```text
vmxnet3/testpmd
  -> vhost-user
  -> virtio-user
  -> l2fwd-lite
  -> fastpath-lite
  -> traffic-test
  -> media-gateway-lite
  -> v17 legacy review
  -> track report / interview / resume material
```

## 当前状态

| 阶段 | 状态 | 说明 |
|---|---|---|
| lab-vmxnet3-testpmd | PASS | PMD 接管与 testpmd smoke |
| lab-vhost-user-basic | PASS | vhost-user socket/backend |
| lab-virtio-user-vhost | PASS_WITH_WARN | virtio-user + vhost-user 对接 |
| lab-dpdk-l2-forwarding | PASS_SMOKE | 自写 l2fwd-lite |
| project-user-space-fastpath | PASS_SMOKE | fastpath-lite 框架 |
| project-fastpath-traffic-test | READY_TO_TEST | traffic/rewrite 验证入口 |
| project-dpdk-media-gateway-lite | PASS_SMOKE | 双 vdev smoke，真实流量后补 |
| project-dpdk-v17-legacy-review | PASS_REVIEW | 旧经验迁移和面试材料 |
| DPDK_TRACK_REPORT | READY | 总结收口与作品化材料 |

## 对外表达版本

```text
基于 DPDK 构建用户态数据面实验与媒体网关原型，完成 vmxnet3 PMD 接管、vhost-user/virtio-user 对接、自研 l2fwd-lite/fastpath-lite、media-gateway-lite smoke 验证，并结合 DPDK v17 媒体面经验整理 KNI、UIO/VFIO、vhost/virtio、UDP fastpath、rewrite 与统计验收的现代化迁移复盘。
```

## 简历表达版本

```text
DPDK 用户态数据面与媒体网关原型：基于 Ubuntu/VMware/DPDK 21.11 环境完成 vmxnet3 PMD 接管、hugepage/uio 绑定、vhost-user/virtio-user 本机链路、自研 l2fwd-lite/fastpath-lite 和 media-gateway-lite 原型，支持 Ethernet/ARP/IPv4/UDP 分类、UDP-only 策略、MAC/IP/UDP 端口改写框架、per-port/per-rule/drop reason 统计，并结合 DPDK v17 媒体面经验整理 KNI/UIO/VFIO/vhost 迁移复盘。
```

## 后续补强

```text
1. media-gateway-lite PASS_TRAFFIC
2. media-gateway-lite PASS_FORWARDING
3. media-gateway-lite PASS_REWRITE
4. pcap PMD / vhost-virtio 真实 UDP 输入路径
5. 最终简历压缩版
```
