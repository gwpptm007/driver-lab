# START_HERE - lab-virtio-user-vhost

## 0. 你现在要验证什么

本实验验证 DPDK 纯用户态虚拟链路（frontend 与 backend 互联闭环）：

```text
frontend testpmd (txonly)           backend testpmd (rxonly)
┌─────────────────────────┐      ┌─────────────────────────┐
│  net_virtio_user0       │      │   net_vhost0            │
│  (virtio-user PMD)      │      │  (vhost-user PMD)       │
│         │               │      │         │               │
│    发送数据包           │ UDS   │    接收数据包           │
│         │               │socket │         │               │
│         ▼               ◄──────►│         ▼               │
└─────────────────────────┘  /tmp/ └─────────────────────────┘
                      vhost.sock
```

**核心原理**（详见 `docs/04_ARCHITECTURE_AND_PRINCIPLES.md`）：

- **frontend (net_virtio_user0)**：DPDK virtio-user PMD，模拟 virtio 前端驱动，在 txonly 模式下不断发送数据包
- **backend (net_vhost0)**：DPDK vhost-user PMD，实现 virtio backend，在 rxonly 模式下不断接收数据包
- **UDS socket**：通过 `/tmp/dpdk-vhost-user0` 连接两端，关键是能传递文件描述符（FD），实现跨进程共享内存（零拷贝）
- **virtqueue**：frontend 和 backend 之间的共享内存区域，包含 available ring 和 used ring

**承接关系**：

- 上一站 `lab-vhost-user-basic`：只证明 backend 能创建 socket
- **本站在此基础上**：证明 frontend 能接进来，形成完整数据面

## 1. 推荐执行顺序

```bash
cd track-dpdk/lab-virtio-user-vhost

./scripts/00_check_env.sh
sudo ./scripts/01_setup_hugepages.sh
sudo ./scripts/02_run_virtio_user_vhost_pair.sh
./scripts/03_collect_stats.sh
./scripts/04_make_review_bundle.sh
```

## 2. 关键结果怎么看

```bash
ls records/*-virtio-user-vhost
cat records/*-virtio-user-vhost/VHOST_SOCKET.txt
cat records/*-virtio-user-vhost/TESTPMD_COMMANDS.txt
less records/*-virtio-user-vhost/TESTPMD_BACKEND.log
less records/*-virtio-user-vhost/TESTPMD_FRONTEND.log
cat records/*-virtio-user-vhost/REVIEW_BUNDLE.md
```

重点字段：

```text
socket_ready=1
net_vhost0,iface=/tmp/dpdk-vhost-user0
net_virtio_user0,path=/tmp/dpdk-vhost-user0
show port info all
show port stats all
```

## 3. 如果失败先看哪里

优先看：

```bash
cat records/*-virtio-user-vhost/TESTPMD_BACKEND.log
cat records/*-virtio-user-vhost/TESTPMD_FRONTEND.log
cat records/*-virtio-user-vhost/RUNTIME_STATUS.txt
```

常见原因：

- `dpdk-testpmd` 未安装或路径异常，可设置 `TESTPMD_BIN=/path/to/dpdk-testpmd`。
- hugepage 没配好，重新执行 `sudo ./scripts/01_setup_hugepages.sh`。
- `/tmp/dpdk-vhost-user0` 有残留普通文件，执行 `sudo ./scripts/05_clean_runtime.sh`。
- CPU 核心不足，可以覆盖 `BACKEND_CORES=0-1 FRONTEND_CORES=2-3` 为更小范围。

## 4. 为什么本实验仍然 `--no-pci`

因为这一站关注的是虚拟化数据面，不使用真实网卡：

- 不影响 `ens33` SSH 管理口。
- 不依赖 `ens192` 当前是 kernel `vmxnet3` 还是 DPDK `uio_pci_generic`。
- 把问题聚焦在 `virtio-user <-> vhost-user` 协商和 testpmd 运行证据上。
