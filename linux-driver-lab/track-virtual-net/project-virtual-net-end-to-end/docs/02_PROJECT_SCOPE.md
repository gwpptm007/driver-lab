# 02_PROJECT_SCOPE

## 项目目标

把三个 Lab 收成一个可展示项目：

```text
guest virtio_net
  -> QEMU/vhost backend
  -> tap
  -> Linux bridge
  -> host / another guest
```

## 输入 Lab 1：lab-virtio-tap-bridge-path

应该提供：
- bridge/tap 状态
- guest ping host bridge IP
- FDB 记录
- guest-to-host 路径说明

## 输入 Lab 2：lab-virtio-vhost-kick-notify

应该提供：
- vhost=off QEMU 参数
- vhost=on QEMU 参数
- `/dev/vhost-net` + `vhost_net` 模块状态
- userspace backend vs vhost backend 对照说明

## 输入 Lab 3：lab-two-guest-bridge-flow

应该提供：
- guest A/B QEMU 参数
- guest A/B IP/MAC
- A ping B 成功
- bridge FDB 双 MAC 学习证据

## 最终拓扑

```
             host
+--------------------------------+
|              br-vnet0          |
|        192.168.100.1/24        |
+----------+-------------+-------+
           |             |
     tap-vnet-a     tap-vnet-b
           |             |
        QEMU A        QEMU B
           |             |
     guest A eth0   guest B eth0
     192.168.100.2  192.168.100.3
```

### 单 guest 到 host

```
guest A -> tap -> bridge -> host local IP stack
```

### guest-to-guest

```
guest A -> tap-vnet-a -> br-vnet0 -> tap-vnet-b -> guest B
```

### vhost 路径

```
vhost=off: QEMU userspace handles tap backend
vhost=on:  host kernel vhost_net handles datapath backend
```