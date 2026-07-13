# TEST_RECORD_20260714_RDMA_FUNDAMENTALS

## 目标

收口 `track-rdma-core/docs/fundamentals` 改造，验证：

1. 13 个主题和总入口完整、相对链接有效、Mermaid 围栏闭合。
2. `README.md`、`START_HERE.md`、`ROADMAP.md` 的 Phase 0 入口一致。
3. 六个 verbs 基础实验无功能回归。
4. RC client/server、performance tuning、one-sided KV 工程项目无功能回归。
5. RXE/GID 环境变化有明确诊断和可复现准备流程。

最终状态：`RDMA_FUNDAMENTALS_COMPLETE`。

## 测试环境

| 项目 | 值 |
| --- | --- |
| 日期 | 2026-07-14 |
| 本地工作区 | Windows，`E:\02_Learning\2026\gitcode\driver-lab` |
| Linux 测试机 | `192.168.65.135` |
| Linux 用户 | `wq7` |
| 远端仓库 | `/home/wq7/workspace/driver-lab` |
| RDMA device | `rxe0` |
| netdev | `ens34` |
| RDMA port | 1，ACTIVE/LINK_UP |
| 测试 GID | `fe80::34`，index 1 |
| 能力边界 | Soft-RoCE/RXE，只验证功能和相对行为，不代表 RNIC 性能 |

## 文档规模

```text
files=14
topic_docs=13
lines=1893
mermaid=61
```

主题覆盖硬件/内核/用户态分层、verbs 对象、MR/DMA/key、QP 状态机、WR/WQE/CQE、RC/UD/RoCEv2、one-sided/Atomic、一致性、性能/NUMA、可靠性/安全、项目映射、排障和速记卡。

## 本地快速审计

命令：

```powershell
cd E:\02_Learning\2026\gitcode\driver-lab\linux-driver-lab\track-rdma-core
py tests/check_fundamentals.py
py -m py_compile tests/check_fundamentals.py
```

结果：

```text
RDMA_FUNDAMENTALS_DOC_AUDIT_PASS files=14 lines=1893 mermaid=61 links=pass
RDMA_FUNDAMENTALS_COMPLETE
```

## 首次远端回归与问题定位

首次直接使用当时环境的 GID 表运行基础回归：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core
bash tests/software_regression.sh
```

前两项通过：

```text
RDMA_REGRESSION_PASS target=lab-rdma-verbs-object-lifecycle
RDMA_REGRESSION_PASS target=lab-rdma-memory-region-deep-dive
```

QP state machine 首次失败：

```text
transition=left_INIT_to_RTR rc=61 errno=61 message=No data available
```

检查命令：

```bash
rdma link show
ibv_devinfo -d rxe0 -i 1 -v | grep -E 'state:|link_layer:|active_mtu:|GID\['
ip -6 addr show dev ens34
```

发现 `rxe0` 重建后只剩 `GID[0]`，而四个 2026-07-01 基础测试脚本硬编码 `--gid-index 1`。改为 index 0 后，QP 仍因 provider GID 与 stable-privacy link-local 地址不一致，在 `INIT -> RTR` 返回 `ETIMEDOUT`。

根因不是编译或文档变更，而是测试依赖了历史临时地址 `fe80::34` 生成的 GID，却没有把该环境前置条件自动化。

## 修复

1. 四个基础测试脚本增加 `RDMA_GID_INDEX` 环境覆盖，并补中文注释。
2. 新增 `tests/prepare_rxe.sh`，仅在显式 `PREPARE_RXE=1` 时执行。
3. 准备脚本幂等添加 `fe80::34/64`、重建 `rxe0`、等待 ACTIVE 并校验 GID index 1。
4. 默认回归不修改网络；阶段收口命令显式声明 RXE 准备动作。

准备 marker：

```text
RDMA_RXE_PREPARE_BEGIN device=rxe0 netdev=ens34 gid_addr=fe80::34 gid_index=1
RDMA_RXE_PREPARE_PASS device=rxe0 gid_index=1
```

## 最终完整回归命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core
chmod +x tests/*.sh
bash -n tests/*.sh lab-rdma-*/tests/*.sh
python3 -m py_compile tests/check_fundamentals.py
SUDO_PASSWORD='<sudo-password>' \
PREPARE_RXE=1 \
RDMA_GID_INDEX=1 \
EXTENDED_REGRESSION=1 \
bash tests/software_regression.sh \
  > /tmp/rdma-fundamentals-extended.log 2>&1
```

日志收敛命令：

```bash
grep -n \
  -e DOC_AUDIT \
  -e RDMA_RXE_PREPARE \
  -e RDMA_REGRESSION \
  -e CURRENT_ENV_COMPLETE \
  -e SOFTWARE_REGRESSION_PASS \
  /tmp/rdma-fundamentals-extended.log
```

## 最终结果

六个基础实验：

```text
RDMA_REGRESSION_PASS target=lab-rdma-verbs-object-lifecycle
RDMA_REGRESSION_PASS target=lab-rdma-memory-region-deep-dive
RDMA_REGRESSION_PASS target=lab-rdma-qp-state-machine
RDMA_REGRESSION_PASS target=lab-rdma-rc-pingpong
RDMA_REGRESSION_PASS target=lab-rdma-one-sided-read-write
RDMA_REGRESSION_PASS target=lab-rdma-ud-rocev2-model
```

工程项目：

```text
PASS: RC client/server SEND WRITE READ wrong-rkey
PASS: wrong-addr remote address boundary
PASS: skip-recv RNR boundary
PASS: disconnect-after-rts cleanup boundary
RDMA_REGRESSION_PASS target=project-rdma-rc-client-server

PASS: RDMA SEND latency + batch WR smoke test inline=0 signal_interval=1 poll_budget=16 enable_rtt=0
RDMA_REGRESSION_PASS target=project-rdma-performance-tuning

PASS: RDMA one-sided KV rkey rotation dynamic directory CAS atomic batch WRITE READ
project_status=ONE_SIDED_KV_CURRENT_ENV_COMPLETE
RDMA_REGRESSION_PASS target=project-rdma-one-sided-kv
```

最终 marker：

```text
RDMA_FUNDAMENTALS_AND_SOFTWARE_REGRESSION_PASS extended=1 prepare_rxe=1
```

## 环境影响与边界

- 本次没有修改 PCI 绑定、hugepage 或物理 RNIC 配置。
- `PREPARE_RXE=1` 会在 `ens34` 添加测试 link-local 地址并重建软件 `rxe0`，可能终止已有 RXE QP，所以只允许在测试窗口运行。
- 所有性能数据来自 RXE，只能证明测试框架和相对参数路径有效。
- 真实 RNIC、双机 RoCEv2、跨 NUMA 和生产 fabric 拥塞仍是后续硬件能力边界。

## 结论

`track-rdma-core` 现在具备统一的项目前知识层、详细图文原理、项目知识映射、分层排障手册、自动文档审计、可选 RXE 环境准备、基础与扩展回归入口。该阶段验收通过。

