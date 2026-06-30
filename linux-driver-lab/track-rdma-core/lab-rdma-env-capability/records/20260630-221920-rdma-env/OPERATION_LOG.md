# Operation Log

记录目录：

```text
records/20260630-221920-rdma-env/
```

测试机：

```text
192.168.65.135
user: wq7
repo: /home/wq7/workspace/driver-lab
```

## 1. 同步 RDMA track 到测试机

目的：把本地新建的 `track-rdma-core`、Phase 1 文档和脚本放到测试机，保证测试命令来自仓库而不是临时手敲。

操作：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-env-capability
chmod +x scripts/*.sh
```

学习点：

- RDMA 实验必须留下可复跑脚本。
- 远端手动命令只作为驱动脚本执行，不作为唯一证据。

## 2. 执行环境采集

命令：

```bash
bash scripts/00_collect_env.sh
```

输出：

```text
ENV_CHECK.log
```

采集内容：

- `uname -a`
- `/etc/os-release`
- `/proc/cmdline`
- `lscpu`
- `lspci`
- `ip -br link`
- `ip -br addr`
- RDMA 相关模块
- `modinfo rdma_rxe`
- 命令存在性检查

关键发现：

- 系统是 Ubuntu 22.04.5。
- 内核是 `6.8.0-124-generic`。
- 虚拟化环境是 VMware。
- 网卡有 `ens192`、`ens33`、`ens34`。
- PCI 设备里没有 Mellanox/Intel RDMA HCA。
- `rdma_rxe.ko` 存在。

学习点：

- `rdma_rxe.ko` 存在说明 Soft-RoCE 有继续尝试的基础。
- 没有 RDMA HCA 时，不能声称真实硬件 RDMA 可用。

## 3. 执行 RDMA capability 采集

命令：

```bash
bash scripts/01_collect_rdma_capability.sh
```

输出：

```text
RDMA_CAPABILITY.log
```

关键发现：

```text
CMD_MISSING ibv_devices
CMD_MISSING ibv_devinfo
CMD_PRESENT rdma /usr/bin/rdma
```

`rdma link`、`rdma dev`、`rdma resource show` 没有发现 RDMA 设备。

学习点：

- `rdma` 命令来自 `iproute2`，通过 RDMA netlink 看 kernel RDMA 资源。
- `ibv_devices` 和 `ibv_devinfo` 来自 `ibverbs-utils`，通过 libibverbs/provider 看 verbs 设备。
- 两个视角要互相校验：kernel 看得到不代表 verbs provider 一定正常，verbs 看不到也要查工具是否缺失。

## 4. 执行 Soft-RoCE 边界采集

命令：

```bash
bash scripts/02_try_soft_roce_boundary.sh
```

输出：

```text
SOFT_ROCE_BOUNDARY.log
```

默认行为：

```text
ENABLE_RXE_SETUP=0
```

因此脚本只检查模块，不创建 rxe 设备。

关键发现：

- `rdma_rxe.ko` 存在。
- `ib_core` 已加载。

学习点：

- Soft-RoCE setup 会改变 RDMA link 状态，所以默认不自动执行。
- 后续要显式使用 `ENABLE_RXE_SETUP=1 RXE_NETDEV=<netdev>`。

## 5. 生成 summary

命令：

```bash
bash scripts/03_generate_summary.sh
```

输出：

```text
SUMMARY.md
```

结论：

```text
BLOCKED_RDMA_TOOLS_MISSING
BLOCKED_NO_RDMA_DEVICE
PASS_SOFT_ROCE_AVAILABLE
```

学习点：

- 当前不是 RDMA data path 成功，而是环境边界已经被清楚记录。
- 下一步先补齐 `ibverbs-utils`，再复查 verbs device。

## 6. 尝试补齐 ibverbs-utils

目的：补齐 `ibv_devices` 和 `ibv_devinfo`。

尝试命令：

```bash
sudo apt-get update
sudo apt-get install -y ibverbs-utils
```

输出：

```text
INSTALL_IBVERBS_UTILS.log
```

第一次结果：

```text
E: Could not get lock /var/lib/dpkg/lock-frontend. It is held by process 16342 (unattended-upgr)
```

随后补充了可复用脚本：

```bash
bash scripts/04_install_ibverbs_utils.sh
```

第二次结果：

```text
BLOCKED_DPKG_LOCK: unattended-upgrade is running.
1118 /usr/bin/python3 /usr/share/unattended-upgrades/unattended-upgrade-shutdown --wait-for-signal
```

学习点：

- `ibverbs-utils` 是诊断工具包，缺它不等于 `libibverbs` 一定不存在。
- 测试机已安装 `rdma-core`、`libibverbs1`、`libibverbs-dev`、`ibverbs-providers`。
- 当前缺的是观察 verbs device 的命令。
- 不应该强杀 `unattended-upgrade`，否则可能破坏系统包管理状态。
- 后续用 `scripts/04_install_ibverbs_utils.sh` 重试即可，它会先检查 dpkg 锁并把结果写入 `INSTALL_IBVERBS_UTILS.log`。

## 7. 当前待办

等待 dpkg 锁释放后执行：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-env-capability
bash scripts/04_install_ibverbs_utils.sh
REUSE_LATEST_RECORD=0 bash scripts/00_collect_env.sh
bash scripts/01_collect_rdma_capability.sh
bash scripts/02_try_soft_roce_boundary.sh
bash scripts/03_generate_summary.sh
```

如果补齐工具后仍无真实 RDMA device，再选择合适网卡尝试：

```bash
ENABLE_RXE_SETUP=1 RXE_NETDEV=ens34 bash scripts/02_try_soft_roce_boundary.sh
```

优先选 `ens34` 的原因：它当前没有 IP 地址，风险低于 SSH 管理地址所在的 `ens33`。
