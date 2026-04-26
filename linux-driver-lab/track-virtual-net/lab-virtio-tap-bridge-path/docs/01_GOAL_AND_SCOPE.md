# 01_GOAL_AND_SCOPE

## 目标

跑通最小虚拟化网络路径：

```text
guest eth0(virtio_net)
  -> QEMU tap backend
  -> host tap
  -> host bridge
  -> host IP
```

## 当前做什么

- 创建 bridge
- 创建 tap
- QEMU 使用 virtio-net-pci
- guest 配 IP
- host bridge 配 IP
- guest ping host
- host tcpdump 观察 tap/bridge

## 当前不做什么

- 不做 vhost
- 不做 two guest
- 不做 DPDK
- 不做复杂 NAT
