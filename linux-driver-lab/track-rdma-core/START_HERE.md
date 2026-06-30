# START_HERE

## 先读什么

RDMA 这条线先读原理，再跑命令：

```text
docs/01_TRACK_OVERVIEW.md
docs/02_RDMA_CORE_MODEL.md
docs/04_DPDK_TO_RDMA_BRIDGE.md
lab-rdma-env-capability/docs/04_DEEP_LEARNING.md
ROADMAP.md
```

重点先搞清楚：

- `rdma-core`、`libibverbs`、provider、kernel RDMA subsystem 的分工。
- device/context/PD/MR/CQ/QP/WR/CQE 的关系。
- `lkey/rkey` 的意义。
- Soft-RoCE 能学习什么，不能证明什么。

## 再跑什么

### Phase 1: capability boundary

在测试机上进入仓库：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-env-capability
bash scripts/00_collect_env.sh
bash scripts/01_collect_rdma_capability.sh
bash scripts/02_try_soft_roce_boundary.sh
bash scripts/04_install_ibverbs_utils.sh
bash scripts/03_generate_summary.sh
```

默认脚本只做采集，不会改网卡、不加载模块、不创建 rxe 设备。

### Phase 2: verbs object lifecycle

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle
REUSE_LATEST_RECORD=0 bash scripts/00_check_env.sh
bash scripts/01_build.sh
bash scripts/02_run_object_lifecycle.sh
bash scripts/03_generate_summary.sh
```

当前 `rxe0` 已创建后，预期输出是：

```text
BUILD_PASS
OBJECT_LIFECYCLE_PASS
```

如果后续确认要尝试 Soft-RoCE，再显式执行：

```bash
ENABLE_RXE_SETUP=1 RXE_NETDEV=ens192 bash scripts/02_try_soft_roce_boundary.sh
```

当前测试机已经使用 `ens34` 创建了 Soft-RoCE device `rxe0`。

## 看什么结果

优先看最新 records：

```bash
ls -td records/20* | head -1
cat "$(ls -td records/20* | head -1)/SUMMARY.md"
```

再看三类证据：

- `ENV_CHECK.log`：操作系统、内核、网卡、模块、命令是否存在。
- `RDMA_CAPABILITY.log`：`ibv_devices`、`ibv_devinfo`、`rdma link/dev/resource` 的实际输出。
- `SOFT_ROCE_BOUNDARY.log`：`rdma_rxe` 模块和 Soft-RoCE 尝试边界。
- `INSTALL_IBVERBS_UTILS.log`：补齐 `ibv_devices` / `ibv_devinfo` 的过程和阻塞原因。
- `OPERATION_LOG.md`：每一步操作的学习解释。

## 本阶段验收口径

Phase 1 成功不等于机器一定有 RDMA 硬件。成功的定义是：把硬件、工具、内核模块、Soft-RoCE 的真实状态记录完整，并能解释下一阶段能不能进入 verbs 编程。
