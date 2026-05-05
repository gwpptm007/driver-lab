# lab-virtio-user-vhost

## 定位

这是 `track-dpdk` 的第三站，用于验证 **DPDK virtio-user frontend 与 vhost-user backend 的本机互联闭环**。

上一站 `lab-vhost-user-basic` 只启动了 vhost-user backend：

```text
dpdk-testpmd + net_vhost vdev + UNIX socket
```

本实验继续向前推进一步：

```text
backend testpmd:  net_vhost0        iface=/tmp/dpdk-vhost-user0
frontend testpmd: net_virtio_user0  path=/tmp/dpdk-vhost-user0
```

也就是不用真实网卡、不启动完整 VM，在同一台测试机上让两个 DPDK 进程通过 vhost-user socket 对接。

## 默认测试机

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理口: `ens33 / e1000 / 192.168.65.135`
- 物理 DPDK 口: `ens192 / vmxnet3 / 0000:0b:00.0`，本实验不使用它
- vhost socket: `/tmp/dpdk-vhost-user0`

## 快速执行

```bash
cd track-dpdk/lab-virtio-user-vhost

./scripts/00_check_env.sh
sudo ./scripts/01_setup_hugepages.sh
sudo ./scripts/02_run_virtio_user_vhost_pair.sh
./scripts/03_collect_stats.sh
./scripts/04_make_review_bundle.sh
```

## 通过标准

本实验的最低通过标准：

- backend `TESTPMD_BACKEND.log` 能看到 `net_vhost` / `vhost` / `Port` / stats 相关输出。
- frontend `TESTPMD_FRONTEND.log` 能看到 `virtio_user` / `net_virtio_user` / `Port` / stats 相关输出。
- `VHOST_SOCKET.txt` 中出现 `socket_ready=1`。
- `TESTPMD_COMMANDS.txt` 中同时记录 backend 和 frontend 两条命令。
- `REVIEW_BUNDLE.md` 能给出 backend/frontend/socket/stats 的评审结论。

如果 RX/TX 为 0，默认判定为 `PASS_WITH_WARN`，不直接判 FAIL。因为不同 DPDK 版本、testpmd 参数、virtio-user 协商时序下，纯本机 smoke test 可能只稳定证明链路建立，不一定稳定产生非零包计数。

## 下一站

完成后进入：

```text
track-dpdk/lab-dpdk-l2-forwarding
```

下一站开始从 testpmd 过渡到自写 DPDK C 程序：初始化 EAL、mempool、port、rx_burst/tx_burst，并逐步实现 L2 forwarding。
