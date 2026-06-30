# Phase 1 RDMA Env Capability Report

## 状态

已完成第二轮远端采集。`ibverbs-utils` 已补齐，Soft-RoCE `rxe0` 已通过 `ens34` 创建，RDMA 工具和 verbs device 均可见。

## 目标

- 确认 RDMA 工具链是否可用。
- 确认是否有 verbs device。
- 确认 Soft-RoCE 边界。
- 给 Phase 2 verbs object lifecycle 做入口判断。

## 最新记录

```text
records/20260630-221920-rdma-env/
records/20260630-233244-rdma-env/
```

操作过程记录：

```text
records/20260630-221920-rdma-env/OPERATION_LOG.md
```

## 关键结论

| 检查项 | 结果 |
| --- | --- |
| OS/kernel | Ubuntu 22.04.5，`6.8.0-124-generic` |
| 虚拟化环境 | VMware |
| 网卡 | `ens192`、`ens33`、`ens34` |
| RDMA 命令 | `rdma` 存在 |
| verbs 工具 | `ibv_devices`、`ibv_devinfo`、`rdma` 均存在 |
| RDMA 设备 | Soft-RoCE device `rxe0` 已发现 |
| Soft-RoCE | `rdma_rxe.ko` 存在 |
| 安装补齐 | `ibverbs-utils` 已安装 |
| RDMA link | `rxe0/1 state ACTIVE physical_state LINK_UP netdev ens34` |
| GID | `fe80::20c:29ff:fef8:f678, RoCE v2` |

## 缺失组件说明

此前缺失的是 `ibverbs-utils`。它主要提供：

- `ibv_devices`：列出 verbs device。
- `ibv_devinfo`：查看 device、port、GID、transport、capability。

它不是 RDMA 的全部运行时。测试机已经有：

- `rdma-core`
- `libibverbs1`
- `libibverbs-dev`
- `ibverbs-providers`

第二轮执行已经补齐该工具包，因此现在可以从 verbs 视角观察 `rxe0`。

## Summary 输出

```text
PASS_RDMA_TOOLS_PRESENT
PASS_RDMA_DEVICE_PRESENT
PASS_SOFT_ROCE_AVAILABLE
```

## 下一步

当前 Phase 1 已完成。复现命令：

```bash
REUSE_LATEST_RECORD=0 bash scripts/00_collect_env.sh
bash scripts/01_collect_rdma_capability.sh
ENABLE_RXE_SETUP=1 RXE_NETDEV=ens34 bash scripts/02_try_soft_roce_boundary.sh
bash scripts/03_generate_summary.sh
```

下一步进入 Phase 2 verbs object lifecycle，并以 `rxe0` 作为学习设备。
