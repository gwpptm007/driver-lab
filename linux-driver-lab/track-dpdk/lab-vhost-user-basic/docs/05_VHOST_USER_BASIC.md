# 05_VHOST_USER_BASIC

## vhost-user 在这里扮演什么角色

在 DPDK 虚拟化数据面里，vhost-user 通常作为后端 backend：

```text
VM / virtio frontend
        │
        │ vhost-user socket
        ↓
DPDK vhost backend
        │
        ↓
用户态转发/处理逻辑
```

本实验只启动 backend，不启动 frontend。

## 为什么先只做 backend

这样可以把问题拆开：

1. DPDK 能不能启动 vhost backend？
2. socket 能不能创建？
3. testpmd 能不能管理这个 vdev port？
4. records 是否能稳定留证？

这些都通过后，再进入下一站 `lab-virtio-user-vhost`，把 frontend 接进来。

## server/client 模式

默认：

```text
client=0
```

含义是 DPDK vhost-user backend 作为 server 创建 socket。

后续如果由 QEMU 创建 socket、DPDK 去连接，才会考虑：

```text
client=1
```

## 当前默认 socket

```text
/tmp/dpdk-vhost-user0
```

可以通过环境变量覆盖：

```bash
sudo VHOST_SOCKET=/tmp/my-vhost.sock ./scripts/02_run_vhost_testpmd.sh
```
