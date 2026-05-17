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

## UDS vs Raw Socket

本实验使用 **UDS (Unix Domain Socket, AF_UNIX)** 作为 vhost-user 的通信机制。UDS 和 Raw Socket 是完全不同的概念：

| 特性 | UDS (AF_UNIX) | Raw Socket (AF_PACKET) |
|------|--------------|------------------------|
| 通信范围 | 仅限本机进程间 | 跨网络设备/跨机器 |
| 底层标识 | 文件路径（如 `/tmp/vhost.sock`） | MAC 地址 / IP 地址 |
| 协议栈参与 | 完全不走 TCP/IP 栈 | 绕过部分栈，直接操作底层帧 |
| 数据单位 | 字节流或数据报 | 原始以太网帧或 IP 包 |
| DPDK 用途 | 控制面（传文件描述符 FD） | 数据面（处理网络报文） |

### 为什么 vhost-user 选 UDS 而不是 Raw Socket

UDS 的核心优势：可以安全传输文件描述符（FD）

在 Vhost-user 场景中，前端（QEMU/virtio）需要把虚拟机内存的文件描述符传给后端（DPDK），让后端能直接访问虚机的物理内存。这意味着 UDS 传输的不仅是数据，还包括**指向内存地址的文件描述符**——这种能力是 TCP/IP Socket 完全做不到的。

Raw Socket 只能处理网络帧数据，无法实现"传钥匙"这种精细操作。

### UDS 的实现本质

UDS 在内核中的数据路径极短：

```
应用程序 → 套接字层 → 内核缓冲区（Ring Buffer）直接拷贝 → 另一个应用程序
```

没有三次握手，没有 ACK，没有路由寻址。内核通过 `scm_sendmsg` 机制转发文件描述符。

### 抽象套接字（Abstract Socket）

UDS 还有一种特殊形态：**Abstract Namespace（抽象套接字）**

| 特性 | 说明 |
|------|------|
| 路径特征 | 以 `@` 或空字符 `\0` 开头（如 `@/tmp/vhost.sock`） |
| 优势 | 不依赖文件系统，完全无视 chroot 和文件路径限制 |
| 缺陷 | 受 Network Namespace 严格隔离，跨容器 NetNS 无法通信 |

如果进程 A 和 B 在不同容器 NetNS 里，抽象套接字就死活连不上了。本实验使用文件路径式 UDS（`/tmp/dpdk-vhost-user0`），不受此限制。

### UDS 在本实验中的作用

```bash
--vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0
```

- `iface=/tmp/dpdk-vhost-user0`：指定 UDS socket 文件路径
- QEMU 作为 frontend 通过这个 socket 与 DPDK backend 通信
- `queues=1`：创建一个 virtqueue（Rx/Tx 各一）
- `client=0`（server 模式）：DPDK 创建 socket 文件，QEMU 去连接

```text
/tmp/dpdk-vhost-user0  ← 这是一个 UDS socket 文件（不是普通文件！）
```

查看方式：

```bash
# 查看 socket 文件类型
ls -l /tmp/dpdk-vhost-user0   # 显示为 socket 类型
file /tmp/dpdk-vhost-user0    # 显示为 Unix domain socket

# 查看监听中的 UDS（ss -xl 中的 x 表示 Unix socket，l 表示仅 Listening）
ss -xl | grep vhost
# 输出示例：
# Netid  State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port
# u_str  LISTEN  0       0       /tmp/dpdk-vhost-user0  *:*
```

## ss -xl 参数详解

`ss -xl` 是查询 Unix Domain Sockets 状态的常用命令：

| 参数 | 含义 |
|------|------|
| `-x` | 仅显示 Unix Domain Sockets（AF_UNIX），过滤掉 TCP/UDP |
| `-l` | 仅显示 Listening（监听）状态的套接字 |

### 输出各列含义

```text
Netid  State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port
u_str  LISTEN  0       128     /tmp/dpdk-vhost-user0  *:*
```

| 列名 | 含义 |
|------|------|
| `Netid` | `u_str` = Unix stream socket（类似 TCP可靠连接）；`u_dgr` = Unix datagram |
| `State` | `LISTEN` = 正在监听；`CONNECTED` = 已连接 |
| `Recv-Q` | 接收队列中的字节数 |
| `Send-Q` | 发送队列中的字节数。如果数值很大，说明 Backend 处理太慢，产生拥塞 |
| `Local Address:Port` | UDS socket 文件路径 |
| `Peer Address:Port` | 对端地址，`*:*` 表示未连接（等待前端接入） |

### 在 DPDK 开发中的用途

**确认服务端已启动**：如果 testpmd 作为 Vhost-user Server 已启动，但虚拟机连不上，先用 `ss -xl` 确认 socket 是否处于 `LISTEN` 状态。

**验证路径正确性**：检查输出的路径是否与脚本中定义的 `VHOST_SOCKET`（如 `/tmp/dpdk-vhost-user0`）完全一致。

**权限排查**：路径存在但 `ss` 看不到，可能是当前用户没有权限访问该套接字。

### 进阶：查看 Socket 所属进程

```bash
sudo ss -xlp
# 输出示例：
# Netid  State   Recv-Q  Send-Q  Local Address:Port   Peer Address:Port  Process
# u_str  LISTEN  0       0       /tmp/dpdk-vhost-user0  *:*               users:(("testpmd",pid=1234,fd=5))
```

加上 `-p` 参数可以看到是哪个进程（PID）持有该 Socket，可以确认是 `testpmd` 还是其他程序。

### Socket 连不上的排查

如果 `ss -xl` 没有任何输出，但你确信程序已启动：

- 可能是大页内存分配失败
- 可能是权限不足导致程序提前退出
- 用 `sudo ss -xlp` 再次确认（需要 sudo 权限才能看到进程信息）

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

## 常见问题快速排查

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
