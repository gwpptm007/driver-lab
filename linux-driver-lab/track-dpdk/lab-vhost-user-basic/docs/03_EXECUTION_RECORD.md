# 07_EXECUTION_RECORD

> 本文档记录 lab-vhost-user-basic 实验的完整执行过程和结果。

## 实验时间线

| 步骤 | 脚本 | 状态 | 说明 |
|------|------|------|------|
| 1 | `00_check_env.sh` | ✅ 已完成 | 环境检查 |
| 2 | `01_setup_hugepages.sh` | ✅ 已完成 | 大页配置（1024 × 2MB = 2GB） |
| 3 | `02_run_vhost_testpmd.sh` | ✅ 已完成 | 运行 vhost-user testpmd |
| 4 | `03_collect_stats.sh` | ✅ 已完成 | 收集统计信息 |
| 5 | `04_make_review_bundle.sh` | ✅ 已完成 | 生成 review bundle |

---

## 步骤 1：环境检查（00_check_env.sh）

**执行时间**：2026-05-05 16:59:41
**执行结果**：✅ 通过

**终端输出**：
```
[OK] Environment check saved:
/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vhost-user-basic/records/20260505_165941-vhost-user-basic/ENV_CHECK.txt

Next:
  sudo ./scripts/01_setup_hugepages.sh
```

**环境快照**：

| 项目 | 值 |
|------|-----|
| Lab | lab-vhost-user-basic |
| Guest | Ubuntu 22.04.5 Desktop |
| Kernel | Linux 6.8.0-110-generic |
| User | wq7 (uid=1000, gid=1000) |
| CPU | AMD Ryzen 9 7945HX (8 cores, 2 sockets) |
| NUMA | 1 node, CPUs 0-7 |
| 管理网卡 | ens33 / e1000 / 192.168.65.135 |

**DPDK 工具**：
- `testpmd`: `/usr/bin/dpdk-testpmd`
- DPDK 版本：21.11.9

**本 Lab 默认参数**：

| 参数 | 值 |
|------|-----|
| `VHOST_SOCKET` | `/tmp/dpdk-vhost-user0` |
| `VHOST_QUEUES` | 1 |
| `VHOST_CLIENT_MODE` | 0 |
| `TESTPMD_RUNTIME` | 18 秒 |
| `TESTPMD_CORES` | 0-1 |
| `NO_PCI` | 1（不使用物理网卡） |

**vhost socket 初始状态**：
```
/tmp/dpdk-vhost-user0: not exists
```

---

## 步骤 2：大页配置（01_setup_hugepages.sh）

**执行时间**：2026-05-05 17:01:00
**执行结果**：✅ 通过

**终端输出**：
```
[OK] Hugepage setup saved:
/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vhost-user-basic/records/20260505_165941-vhost-user-basic/HUGEPAGE_SETUP.txt

Next:
  sudo ./scripts/02_run_vhost_testpmd.sh
```

**配置后大页状态**：

| 指标 | 配置前 | 配置后 |
|------|--------|--------|
| HugePages_Total | 0 | 1024 |
| HugePages_Free | 0 | 1024 |
| Hugepagesize | 2048 kB | 2048 kB |
| Hugetlb | 0 kB | 2097152 kB |

> ✅ 大页配置成功：1024 × 2MB = 2GB

---

## 步骤 3：运行 vhost-user testpmd（02_run_vhost_testpmd.sh）

**执行时间**：2026-05-05 17:04:08
**执行结果**：✅ 通过

**终端输出**：
```
[OK] vhost-user testpmd smoke finished.

Evidence:
  command : /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vhost-user-basic/records/20260505_165941-vhost-user-basic/TESTPMD_COMMAND.txt
  log     : /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vhost-user-basic/records/20260505_165941-vhost-user-basic/TESTPMD_VHOST.log
  socket  : /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vhost-user-basic/records/20260505_165941-vhost-user-basic/VHOST_SOCKET.txt
  runtime : /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vhost-user-basic/records/20260505_165941-vhost-user-basic/RUNTIME_STATUS.txt

Next:
  ./scripts/03_collect_stats.sh
```

**testpmd 命令**：

```bash
/usr/bin/dpdk-testpmd -l 0-1 -n 4 \
  --file-prefix=vhost_basic \
  --vdev=net_vhost0,iface=/tmp/dpdk-vhost-user0,queues=1,client=0 \
  --no-pci \
  -- --port-topology=chained --forward-mode=io --auto-start --stats-period=2
```

**关键输出**：

```
EAL: Selected IOVA mode 'PA'
testpmd: create a new mbuf pool <mb_pool_0>: n=155456, size=2176, socket=0
VHOST_CONFIG: vhost-user server: socket created, fd: 24
VHOST_CONFIG: bind to /tmp/dpdk-vhost-user0
Configuring Port 0 (socket 0)
Port 0: 56:48:4F:53:54:00
Checking link statuses...
Done
io packet forwarding - ports=1 - cores=1 - streams=1
Logical Core 1 (socket 0) forwards packets on 1 streams:
  RX P=0/Q=0 (socket 0) -> TX P=0/Q=0 (socket 0) peer=02:00:00:00:00:00
```

**Port 0 统计**（运行 18 秒，无 virtio peer）：

```
RX-packets: 0          RX-missed: 0          RX-bytes:  0
TX-packets: 0          TX-errors: 0          TX-bytes:  0
```

> ✅ testpmd 成功启动，vhost-user socket 创建成功
> ⚠️ RX/TX=0 是因为没有 virtio peer 连接（正常现象）

---

## 步骤 4：收集统计信息（03_collect_stats.sh）

**执行时间**：2026-05-05 17:08:38
**执行结果**：✅ 通过

**终端输出**：
```
[OK] Post check saved:
/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vhost-user-basic/records/20260505_165941-vhost-user-basic/POST_CHECK.txt

Next:
  ./scripts/04_make_review_bundle.sh
```

**收集的统计文件**：

| 文件 | 说明 |
|------|------|
| `ENV_CHECK.txt` | 初始环境检查 |
| `HUGEPAGE_SETUP.txt` | 大页配置记录 |
| `POST_CHECK.txt` | 事后检查 |
| `TESTPMD_COMMAND.txt` | testpmd 命令 |
| `TESTPMD_RC.txt` | 返回码 |
| `TESTPMD_VHOST.log` | testpmd 运行日志 |
| `VHOST_SOCKET.txt` | vhost socket 状态 |
| `RUNTIME_STATUS.txt` | 运行时状态 |
| `COMMANDS.md` | 执行命令记录 |

---

## 步骤 5：生成 review bundle（04_make_review_bundle.sh）

**执行时间**：2026-05-05 17:08:50
**执行结果**：✅ 通过

**终端输出**：
```
[OK] Review bundle generated:
/home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-vhost-user-basic/records/20260505_165941-vhost-user-basic/REVIEW_BUNDLE.md
```

**Review 结论**：

| 项目 | 状态 | 证据 |
|------|------|------|
| testpmd command generated | ✅ PASS | TESTPMD_COMMAND.txt |
| vhost-user socket created | ✅ PASS | VHOST_SOCKET.txt |
| vhost/testpmd log available | ✅ PASS | TESTPMD_VHOST.log |
| port/stats command executed | ✅ PASS | TESTPMD_VHOST.log |
| physical NIC untouched | ✅ PASS_BY_DESIGN | 本实验使用 --no-pci |

---

## vhost socket 验证

**Socket 状态**：

```
socket_ready=1
srwxr-xr-x 1 root root 0  5月  5 17:04 /tmp/dpdk-vhost-user0
/tmp/dpdk-vhost-user0: socket
```

**Socket 监听**：

```
ss -xl | grep dpdk-vhost-user0
u_str LISTEN 0  128  /tmp/dpdk-vhost-user0 48452  * 0
```

> ✅ vhost-user UNIX socket 已创建并监听

---

## 总结

**lab-vhost-user-basic 实验完成！**

| 验收项 | 结果 |
|--------|------|
| EAL 启动 | ✅ IOVA mode 'PA' |
| hugepage 可用 | ✅ 1024 × 2MB |
| net_vhost vdev 初始化 | ✅ Port 0: 56:48:4F:53:54:00 |
| UNIX socket 创建 | ✅ socket_ready=1 |
| port/stats 输出 | ✅ NIC statistics 正常 |

**与 lab-vmxnet3-testpmd 的差异**：

| 项目 | lab-vmxnet3-testpmd | lab-vhost-user-basic |
|------|---------------------|---------------------|
| 物理网卡 | ens192 (vmxnet3) | 不使用（--no-pci） |
| 绑定驱动 | uio_pci_generic | 无 |
| 设备类型 | net_vmxnet3 PMD | net_vhost0 vdev |
| Socket | 无 | /tmp/dpdk-vhost-user0 |

---

## 下一步

进入 **Phase 3: `lab-virtio-user-vhost`**

目标：将 frontend (virtio-user) 与 backend (vhost-user) 对接，形成本机闭环。

**记录目录**：`records/20260505_165941-vhost-user-basic/`
