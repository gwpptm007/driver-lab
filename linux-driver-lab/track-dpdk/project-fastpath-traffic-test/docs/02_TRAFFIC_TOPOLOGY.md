# 02_TRAFFIC_TOPOLOGY

## 当前测试机

```text
管理口: ens33 / e1000 / 192.168.65.135
DPDK口: ens192 / vmxnet3 / 0000:0b:00.0
DPDK driver: uio_pci_generic
```

`ens33` 只用于 SSH/管理，不允许 DPDK 脚本操作。

## 拓扑 A：外部发包源

```text
Sender VM / Host
  src ip: 192.168.100.x
  dst mac: fastpath port 0 MAC
  dst ip: 192.168.100.1 或任意测试目的地址
  udp dport: 9000
        |
        v
ens192/vmxnet3 -> fastpath-lite
```

这是当前单 DPDK 口环境的优先路线。

## 拓扑 B：vhost/virtio-user

后续可把前面 `lab-vhost-user-basic` 和 `lab-virtio-user-vhost` 复用起来，构造本机虚拟流量源。

```text
virtio-user frontend -> vhost-user socket -> fastpath-lite
```

## 注意

DPDK bind 之后，Linux 内核不再拥有 `ens192`，所以不能在同一个 guest 内直接用 `ens192` 的 kernel socket 给 fastpath 发包。
