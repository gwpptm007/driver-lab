# 06_TROUBLESHOOTING

## 1. `dpdk-testpmd` 找不到

检查：

```bash
which dpdk-testpmd
which testpmd
```

如果路径不在默认搜索范围：

```bash
TESTPMD_BIN=/path/to/dpdk-testpmd ./scripts/00_check_env.sh
sudo TESTPMD_BIN=/path/to/dpdk-testpmd ./scripts/02_run_vhost_testpmd.sh
```

## 2. socket 没创建

看：

```bash
cat records/*-vhost-user-basic/TESTPMD_VHOST.log
cat records/*-vhost-user-basic/VHOST_SOCKET.txt
```

常见原因：

- hugepage 没配置。
- `net_vhost` vdev 参数不被当前 DPDK 版本接受。
- socket 路径已有残留普通文件。

## 3. `TESTPMD_VHOST.log` 里无 port

确认命令里包含：

```text
--vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0
--no-pci
```

如果当前 DPDK 包不支持 `client=0` 参数，可临时改为：

```bash
sudo VHOST_CLIENT_MODE= ./scripts/02_run_vhost_testpmd.sh
```

但一般 DPDK 20+ 都支持该参数。

## 4. RX/TX 为 0

正常。本实验没有 frontend。下一站接入 `virtio-user` 后才要求能看到更完整的对接行为。

## 5. 清理残留 socket

```bash
sudo ./scripts/05_clean_runtime.sh
```

脚本只会在没有 testpmd 进程时删除 socket。
