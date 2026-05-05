# 02_EXECUTION_FLOW

## 执行流程

```text
00_check_env
  ↓
01_setup_hugepages
  ↓
02_run_virtio_user_vhost_pair
  ├── start backend testpmd: net_vhost0
  ├── wait socket: /tmp/dpdk-vhost-user0
  ├── start frontend testpmd: net_virtio_user0
  ├── run short smoke interval
  ├── show backend/frontend port info
  ├── show backend/frontend stats
  └── stop/quit both
  ↓
03_collect_stats
  ↓
04_make_review_bundle
```

## 测试机环境

```text
Guest OS: Ubuntu 22.04.5 Desktop
Kernel:   Linux 6.8.0-110-generic
User:     wq7
SSH IP:   192.168.65.135
```

## 核心命令模型

backend：
```bash
dpdk-testpmd -l 0-1 -n 4 \
  --file-prefix=vhost_backend \
  --vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0 \
  --no-pci -- \
  --port-topology=chained --forward-mode=rxonly --auto-start --stats-period=2
```

frontend：
```bash
dpdk-testpmd -l 2-3 -n 4 \
  --file-prefix=virtio_frontend \
  --vdev=net_virtio_user0,path=/tmp/dpdk-vhost-user0,queues=1 \
  --no-pci -- \
  --port-topology=chained --forward-mode=txonly --auto-start --stats-period=2
```

## 为什么需要两个 file-prefix

两个 testpmd 进程同时运行，使用不同 prefix 避免争用 hugepage 文件和 runtime 目录。

## vhost-user 与 virtio-user 的关系

```text
virtio-user frontend (net_virtio_user0)
        │
        │ vhost-user protocol over UNIX socket
        ↓
vhost-user backend (net_vhost0)
```

- `net_vhost`：DPDK vhost PMD，通常扮演 backend，创建 socket。
- `net_virtio_user`：DPDK virtio-user PMD，扮演 frontend，连接 socket。

## 为什么 backend 用 rxonly，frontend 用 txonly

frontend 尝试发包，backend 尝试收包。RX/TX 为 0 不直接等于失败，重要的是两边 PMD 初始化成功、socket 创建成功、port info/stats 能输出。

## records 目录

```text
records/YYYYMMDD_HHMMSS-virtio-user-vhost/
├── ENV_CHECK.txt
├── HUGEPAGE_SETUP.txt
├── TESTPMD_COMMANDS.txt
├── TESTPMD_BACKEND.log
├── TESTPMD_FRONTEND.log
├── VHOST_SOCKET.txt
├── RUNTIME_STATUS.txt
├── POST_CHECK.txt
└── REVIEW_BUNDLE.md
```