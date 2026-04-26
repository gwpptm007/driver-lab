# 01_GOAL_AND_SCOPE

## 目标

构造并验证双 guest 二层转发路径：

```text
guest A eth0 (192.168.100.2)
  -> tap-vnet-a -> br-vnet0 -> tap-vnet-b
  -> guest B eth0 (192.168.100.3)
```

## 要回答的问题

1. 两个 guest 是否都能通过 virtio-net 接入同一个 bridge？
2. guest A ping guest B 时，host bridge FDB 是否学习到两个 guest MAC？
3. tapA / br / tapB 上能否看到同一轮 ICMP 流量？
4. guest-to-guest 路径和 guest-to-host 路径有什么区别？
5. guest-to-guest 是否经过 host IP stack？

## 拓扑

```text
                 host
        +--------------------+
        |      br-vnet0      | 192.168.100.1/24
        +----+----------+----+
             |          |
       tap-vnet-a   tap-vnet-b
             |          |
          QEMU A     QEMU B
             |          |
       guest A eth0 guest B eth0
       192.168.100.2 192.168.100.3
```

## QEMU 参数

guest A：
```text
-netdev tap,id=net0,ifname=tap-vnet-a,script=no,downscript=no
-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:a1
```

guest B：
```text
-netdev tap,id=net0,ifname=tap-vnet-b,script=no,downscript=no
-device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:b1
```

## 当前边界

不做 NAT / 路由 / 外网访问 / DPDK / vhost-user / 多队列优化。

## 验收标准

**最低通过**：
- host 有 `br-vnet0`
- host 有 `tap-vnet-a` 和 `tap-vnet-b`，都加入 bridge
- guest A / B 都能看到 virtio-net 网卡
- guest A 能 ping guest B

**标准通过**：
- bridge FDB 学到两个 guest MAC
- tapA / bridge / tapB 至少一个点有状态记录
- 有 `SUMMARY.md` 和 `FLOW_REVIEW_NOTE.md`

**优秀通过**：
- tapA / bridge / tapB 三处均有状态记录
- 能解释 L2 forwarding 和 guest-to-host path 的区别
- 能自然引出 `project-virtual-net-end-to-end`

## 推荐记录

```bash
ip -br link
bridge link
bridge fdb show
lsmod | grep -E 'vhost|tun|bridge'
```