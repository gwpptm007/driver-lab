# lab-rdma-verbs-object-lifecycle

这个目录只学一件事：

```text
RDMA verbs 的基础对象生命周期
```

也就是这条链：

```text
device -> context -> PD -> MR -> CQ -> QP -> destroy
```

先把这条链看懂，再学 QP 状态机、RC ping-pong、RDMA READ/WRITE。

## 先看哪个文件

先看：

```text
app/main.c
```

这个文件就是本 lab 的主体。代码已经按 `STEP 00` 到 `STEP 10` 分段，每一步都有中文注释。

不要一开始就钻 `scripts/`。脚本只是帮你编译、运行、保存结果。

## 只跑一个命令

在测试机上执行：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle
bash scripts/run_all.sh
```

运行成功后看：

```bash
cat records/latest/SUMMARY.md
cat records/latest/OBJECT_LIFECYCLE.log
```

## 目录到底是干嘛的

| 目录 | 作用 | 你应该怎么看 |
| --- | --- | --- |
| `app/` | 学习代码 | 重点看 `main.c` |
| `scripts/` | 一键运行工具 | 只需要跑 `run_all.sh` |
| `docs/` | 解释文档 | 看不懂代码时再读 |
| `records/` | 每次运行的真实输出 | 用来证明你真的跑过 |
| `reports/` | 阶段总结 | 最后复盘和面试材料用 |

现在 `scripts/` 只保留一个入口：

```text
scripts/run_all.sh
```

旧的 `00_check_env.sh`、`01_build.sh`、`02_run_object_lifecycle.sh`、`03_generate_summary.sh` 已经删掉，避免入口太多。

## 你要学会什么

| STEP | API / 动作 | 要理解的问题 |
| --- | --- | --- |
| 00 | `ibv_get_device_list()` | 当前有几个 RDMA verbs 设备 |
| 01 | 打印 device | `rxe0` 是怎么被 verbs 看见的 |
| 02 | `ibv_open_device()` | context 是什么 |
| 03 | `ibv_query_device()` | device 能力上限怎么看 |
| 04 | `ibv_query_port()` | port 状态、MTU、GID 表怎么看 |
| 05 | `ibv_alloc_pd()` | PD 为什么是隔离边界 |
| 06 | 分配 buffer | RDMA 操作的内存从哪里来 |
| 07 | `ibv_reg_mr()` | MR、`lkey`、`rkey` 是什么 |
| 08 | `ibv_create_cq()` | CQ 为什么是完成通知队列 |
| 09 | `ibv_create_qp()` | QP 为什么是后续收发核心 |
| 10 | 反向销毁 | 为什么资源要按反向顺序释放 |

## 当前已经跑通

当前测试机已经启用 Soft-RoCE：

```text
rxe0 -> ens34
```

最新成功结果类似：

```text
BUILD_PASS
OBJECT_LIFECYCLE_PASS
```

关键输出类似：

```text
STEP 00 GET_DEVICE_LIST_OK count=1
STEP 01 DEVICE[0] name=rxe0
STEP 02 OPEN_DEVICE_OK name=rxe0
STEP 07 REG_MR_OK ... lkey=... rkey=...
STEP 09 CREATE_QP_OK qp_num=...
OBJECT_LIFECYCLE_PASS
```

## 下一步

下一步不是继续堆脚本，而是在新 lab 里学习：

```text
QP: RESET -> INIT -> RTR -> RTS
```

也就是把现在创建出来的 QP，真正切到可以收发的状态。
