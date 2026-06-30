# 03_RESULT_ANALYSIS

## 当前记录

最新记录目录：

```text
records/20260630-221920-rdma-env/
```

## 结果矩阵

| 项目 | 结论 | 证据 |
| --- | --- | --- |
| 系统 | Ubuntu 22.04.5，kernel `6.8.0-124-generic`，VMware 虚拟机 | `ENV_CHECK.log` |
| 网卡 | `ens192`、`ens33`、`ens34` 均存在，PCI 设备为 Intel 82545EM 与 VMware VMXNET3 | `ENV_CHECK.log` |
| verbs 工具 | `rdma` 存在，`ibv_devices` / `ibv_devinfo` 缺失 | `RDMA_CAPABILITY.log` |
| RDMA 设备 | `rdma link` / `rdma dev` 无设备输出 | `RDMA_CAPABILITY.log` |
| Soft-RoCE | `rdma_rxe.ko` 存在，`ib_core` 已加载 | `SOFT_ROCE_BOUNDARY.log` |
| 安装尝试 | `ibverbs-utils` 安装被 `unattended-upgr` 的 dpkg 锁阻塞 | `INSTALL_IBVERBS_UTILS.log` |

## 当前结论

本机当前没有发现真实 RDMA device。RDMA 用户态基础库和 `rdma` 命令存在，但 verbs 工具包缺 `ibv_devices` / `ibv_devinfo`，因此还不能完整观察 verbs device list。

内核存在 `rdma_rxe` 模块，所以后续可以走 Soft-RoCE 学习路径。这个结论只代表对象模型和 verbs API 可继续推进，不代表硬件 RDMA 性能。

## 常见结论

### 有真实 RDMA 设备

下一步直接进入 Phase 2，写 verbs object lifecycle 程序，先只创建对象，不急着收发数据。

### 无真实设备，但有 Soft-RoCE

下一步可以显式启用 rxe：

```bash
ENABLE_RXE_SETUP=1 RXE_NETDEV=<netdev> bash scripts/02_try_soft_roce_boundary.sh
```

然后重新采集 capability。

### 工具缺失

先安装用户态工具链，再复跑 Phase 1。缺工具时不要写 verbs 程序，因为失败信息会混杂“代码问题”和“环境问题”。

当前建议命令：

```bash
sudo apt-get install -y ibverbs-utils
bash scripts/00_collect_env.sh
bash scripts/01_collect_rdma_capability.sh
bash scripts/02_try_soft_roce_boundary.sh
bash scripts/03_generate_summary.sh
```
