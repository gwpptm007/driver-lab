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
sudo TESTPMD_BIN=/path/to/dpdk-testpmd ./scripts/02_run_virtio_user_vhost_pair.sh
```

## 2. backend socket 没创建

看：

```bash
cat records/*-virtio-user-vhost/TESTPMD_BACKEND.log
cat records/*-virtio-user-vhost/VHOST_SOCKET.txt
```

常见原因：

- hugepage 没配置。
- `net_vhost` vdev 参数不被当前 DPDK 版本接受。
- socket 路径已有残留普通文件。

## 3. frontend 启动失败

看：

```bash
cat records/*-virtio-user-vhost/TESTPMD_FRONTEND.log
```

重点搜索：

```bash
grep -Ei 'virtio|vhost|failed|error|not found|invalid' records/*-virtio-user-vhost/TESTPMD_FRONTEND.log
```

常见原因：

- 当前 DPDK 包未编译 virtio-user PMD。
- `net_virtio_user0,path=...` 参数格式不兼容。
- backend 没起来，frontend 连接 socket 失败。

## 4. CPU 核心不足

默认使用：

```text
backend:  0-1
frontend: 2-3
```

如果测试机核心数较少，可覆盖：

```bash
sudo BACKEND_CORES=0 FRONTEND_CORES=1 ./scripts/02_run_virtio_user_vhost_pair.sh
```

## 5. RX/TX 仍然为 0

先不要急着判失败。本实验不是吞吐压测。

优先确认：

- backend 和 frontend 两边都有 port 初始化。
- 两边都有 stats 输出。
- 没有 obvious fatal error。
- `VHOST_SOCKET.txt` 中 `socket_ready=1`。

如果要进一步追包，可以后续单独加 `txonly/rxonly` 参数调优或用 pktgen。

## 6. 清理残留 socket

```bash
sudo ./scripts/05_clean_runtime.sh
```

脚本只会在没有相关 testpmd 进程时删除 socket。
