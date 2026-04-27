# START_HERE - lab-vhost-user-basic

## 0. 你现在要验证什么

本实验验证 DPDK 的 vhost-user backend：

```text
dpdk-testpmd
  --vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0
  --no-pci
```

重点不是发包，而是确认：

```text
EAL 启动
hugepage 可用
net_vhost vdev 初始化
UNIX socket 被创建
testpmd 可以 show port info/stats
```

## 1. 推荐执行顺序

```bash
cd track-dpdk/lab-vhost-user-basic

./scripts/00_check_env.sh
sudo ./scripts/01_setup_hugepages.sh
sudo ./scripts/02_run_vhost_testpmd.sh
./scripts/03_collect_stats.sh
./scripts/04_make_review_bundle.sh
```

## 2. 关键结果怎么看

```bash
ls records/*-vhost-user-basic
cat records/*-vhost-user-basic/VHOST_SOCKET.txt
cat records/*-vhost-user-basic/TESTPMD_COMMAND.txt
less records/*-vhost-user-basic/TESTPMD_VHOST.log
cat records/*-vhost-user-basic/REVIEW_BUNDLE.md
```

核心字段：

```text
socket_ready=1
--vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0
show port info all
show port stats all
```

## 3. 如果失败

优先看：

```bash
cat records/*-vhost-user-basic/TESTPMD_VHOST.log
cat records/*-vhost-user-basic/VHOST_SOCKET.txt
```

常见原因：

- `dpdk-testpmd` 未安装或路径不是默认路径，可设置 `TESTPMD_BIN=/path/to/dpdk-testpmd`。
- hugepage 没有配置，重新执行 `sudo ./scripts/01_setup_hugepages.sh`。
- `/tmp/dpdk-vhost-user0` 有残留非 socket 文件，手工确认后删除或执行 `sudo ./scripts/05_clean_runtime.sh`。

## 4. 为什么本实验用 `--no-pci`

因为本实验只验证 vhost-user 虚拟 backend，不需要真实 NIC。这样可以保证：

- 不影响 `ens33` 管理口
- 不依赖 `ens192` 当前是否已经 bind 到 DPDK driver
- 结果更聚焦，方便下一站接 `virtio-user`
