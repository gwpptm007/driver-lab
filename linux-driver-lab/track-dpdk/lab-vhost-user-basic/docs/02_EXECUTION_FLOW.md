# 02_EXECUTION_FLOW

## 执行流程

```text
00_check_env
  ↓
01_setup_hugepages
  ↓
02_run_vhost_testpmd
  ↓
03_collect_stats
  ↓
04_make_review_bundle
```

## 核心命令模型

```bash
dpdk-testpmd -l 0-1 -n 4 \
  --file-prefix=vhost_basic \
  --vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0 \
  --no-pci \
  -- --port-topology=chained --forward-mode=io --auto-start --stats-period=2
```

## 为什么使用 FIFO

`02_run_vhost_testpmd.sh` 使用 FIFO 给 testpmd 输入命令：

```text
show port info all
show port stats all
stop
quit
```

这样可以在 testpmd 运行期间检查 socket 是否创建，然后再优雅退出。

## vhost-user 角色

在 DPDK 虚拟化数据面里，vhost-user 作为后端 backend：

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

### 为什么先只做 backend

把问题拆开：
1. DPDK 能不能启动 vhost backend？
2. socket 能不能创建？
3. testpmd 能不能管理这个 vdev port？
4. records 是否能稳定留证？

都通过后，再进入 `lab-virtio-user-vhost` 把 frontend 接进来。

## server/client 模式

| 模式 | client值 | 含义 |
|------|---------|------|
| server | `client=0` | DPDK vhost 创建 socket（默认） |
| client | `client=1` | QEMU 创建 socket，DPDK 去连接 |

## 记录目录

每次执行会生成：

```text
records/YYYYMMDD_HHMMSS-vhost-user-basic/
├── ENV_CHECK.txt
├── HUGEPAGE_SETUP.txt
├── TESTPMD_COMMAND.txt
├── TESTPMD_VHOST.log
├── VHOST_SOCKET.txt
├── RUNTIME_STATUS.txt
├── POST_CHECK.txt
└── REVIEW_BUNDLE.md
```

## PASS 标准

| 检查项 | 通过标准 | 证据 |
|---|---|---|
| testpmd 可发现 | 能找到 `dpdk-testpmd` | `ENV_CHECK.txt` |
| hugepage 可用 | `HugePages_Total` 大于 0 | `HUGEPAGE_SETUP.txt` |
| vhost-user socket 创建 | `socket_ready=1` | `VHOST_SOCKET.txt` |
| testpmd 有 vhost/port 输出 | 日志中有 `net_vhost`、`Port` 等关键字 | `TESTPMD_VHOST.log` |
| stats 命令执行 | 日志中有 port stats 输出 | `TESTPMD_VHOST.log` |
| 不影响物理 NIC | 命令中存在 `--no-pci` | `TESTPMD_COMMAND.txt` |

## 可接受的 WARN

| WARN | 是否影响通过 | 说明 |
|---|---|---|
| RX/TX 为 0 | 不影响 | 当前没有 virtio peer |
| dmesg 无权限 | 不影响 | 普通用户执行时可能出现 |
| socket 退出后不存在 | 不影响 | testpmd 退出后 socket 被清理属于正常 |

## FAIL 标准

- `socket_ready=0`
- `TESTPMD_VHOST.log` 里没有任何 vhost/testpmd/port 初始化痕迹
- `dpdk-testpmd` 无法启动，且不是路径配置问题
- 误操作物理网卡，导致管理口断开

## 故障排查

### 1. `dpdk-testpmd` 找不到

```bash
which dpdk-testpmd
which testpmd

# 指定路径
sudo TESTPMD_BIN=/path/to/dpdk-testpmd ./scripts/02_run_vhost_testpmd.sh
```

### 2. socket 没创建

```bash
cat records/*-vhost-user-basic/TESTPMD_VHOST.log
cat records/*-vhost-user-basic/VHOST_SOCKET.txt
```

常见原因：
- hugepage 没配置
- `net_vhost` vdev 参数不被当前 DPDK 版本接受
- socket 路径已有残留普通文件

### 3. `TESTPMD_VHOST.log` 里无 port

确认命令里包含：
```
--vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0
--no-pci
```

### 4. RX/TX 为 0

正常。本实验没有 frontend。下一站接入 `virtio-user` 后才要求能看到更完整的对接行为。

### 5. 清理残留 socket

```bash
sudo ./scripts/05_clean_runtime.sh
```

脚本只会在没有 testpmd 进程时删除 socket。
