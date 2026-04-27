# lab-vhost-user-basic

## 定位

这是 `track-dpdk` 的第二站，用于验证 **DPDK vhost-user backend 最小闭环**。

上一站 `lab-vmxnet3-testpmd` 已经验证：

- hugepage 可用
- `dpdk-testpmd` 可运行
- VMware `vmxnet3` PMD 可初始化
- records/review 留证流程可用

本实验不再操作物理网卡，也不 bind/unbind `ens192`。它只做一件事：

```text
使用 testpmd 通过 net_vhost vdev 创建 vhost-user UNIX socket，并输出 port/stats 证据。
```

## 默认测试机

- Guest: Ubuntu 22.04.5 Desktop
- Kernel: Linux 6.8.0-110-generic
- 管理口: `ens33 / e1000 / 192.168.65.135`
- DPDK 物理口: `ens192 / vmxnet3 / 0000:0b:00.0`，本实验不使用它
- vhost socket: `/tmp/dpdk-vhost-user0`

## 快速执行

```bash
cd track-dpdk/lab-vhost-user-basic
./scripts/00_check_env.sh
sudo ./scripts/01_setup_hugepages.sh
sudo ./scripts/02_run_vhost_testpmd.sh
./scripts/03_collect_stats.sh
./scripts/04_make_review_bundle.sh
```

## 通过标准

本阶段不要求有 virtio peer，也不要求 RX/TX 非 0。满足下面条件即可通过：

- `TESTPMD_COMMAND.txt` 记录了 `--vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,...`
- `VHOST_SOCKET.txt` 出现 `socket_ready=1`
- `TESTPMD_VHOST.log` 有 testpmd port/stats 输出
- `REVIEW_BUNDLE.md` 对 socket/log/stats 给出 PASS 或明确可解释 WARN
- 没有修改 `ens33`，也没有对物理网卡执行 bind/unbind

## 下一站

完成后进入：

```text
track-dpdk/lab-virtio-user-vhost
```

下一站会把 frontend 接进来，形成 `virtio-user <-> vhost-user` 的本机闭环。
