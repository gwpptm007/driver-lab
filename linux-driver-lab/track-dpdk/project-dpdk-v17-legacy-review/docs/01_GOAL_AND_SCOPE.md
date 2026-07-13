# 01_GOAL_AND_SCOPE

## 背景

当前 `track-dpdk` 已经完成了从 DPDK 环境到项目型 fastpath 的一条学习链：

```text
lab-vmxnet3-testpmd
  -> lab-vhost-user-basic
  -> lab-virtio-user-vhost
  -> lab-dpdk-l2-forwarding
  -> project-user-space-fastpath
  -> project-fastpath-traffic-test
  -> project-dpdk-media-gateway-lite
```

其中 `project-dpdk-media-gateway-lite` 已达到 `PASS_PCAP_FUNCTIONAL/FORWARDING/REWRITE`；外部 wire、真实 NIC 和性能证据后续继续补。

本项目的目标不是继续加 C 功能，而是做一次经验复盘：把过去 DPDK v17 项目经验和当前 modern DPDK track 对齐。

## 本项目要解决的问题

面试官通常不会只问“你跑过 testpmd 吗”，而会问：

```text
1. DPDK 为什么比内核协议栈更适合用户态数据面？
2. 你当时做的 DPDK v17 项目数据路径是什么？
3. KNI 在旧项目里解决什么问题？现在为什么不优先选它？
4. UIO / VFIO / vhost-user / virtio-user 各自处在哪一层？
5. ARP、IP、UDP、MAC rewrite 在媒体面里为什么需要？
6. 你现在这个 track 和过去项目有什么对应关系？
```

本目录就是为了把这些问题整理成系统答案。

## 范围内

```text
DPDK v17 旧项目经验复盘
现代 DPDK 21.11 环境下的接口和工程方式对照
KNI / UIO / VFIO / vhost-user / virtio-user 对比
媒体面 UDP fastpath 迁移设计
简历 bullet 与面试讲法
```

## 范围外

```text
不强制重新搭建 DPDK v17 编译环境
不强制复现旧项目私有代码
不在本阶段补 media-gateway-lite 真实流量
不把旧项目代码直接迁移成生产级实现
```

## 当前项目定位

```text
经验复盘项目
迁移设计项目
面试表达项目
简历作品化项目
```
