# Phase 2 Verbs Object Lifecycle Report

## 状态

已完成第二轮远端执行。Soft-RoCE `rxe0` 可见后，verbs object lifecycle 已 PASS。

## 目标

- 编译最小 verbs object lifecycle 程序。
- 在没有 RDMA device 时记录边界。
- 在有 RDMA device 或 Soft-RoCE 时创建 context/PD/MR/CQ/QP。

## 预期第一轮结果

```text
BUILD_PASS
BLOCKED_NO_RDMA_DEVICE
```

## 最新记录

```text
records/20260630-232844-verbs-object/
records/20260630-233328-verbs-object/
records/20260630-233918-verbs-object/
records/20260630-234417-verbs-object/
```

## 实际结果

```text
BUILD_PASS
OBJECT_LIFECYCLE_PASS
```

## 关键证据

| 项目 | 结果 | 证据 |
| --- | --- | --- |
| 编译器 | `/usr/bin/gcc` | `ENV_CHECK.log` |
| `make` | `/usr/bin/make` | `ENV_CHECK.log` |
| verbs header | `VERBS_HEADER_PRESENT` | `ENV_CHECK.log` |
| libibverbs package | `libibverbs-dev`、`libibverbs1`、`ibverbs-providers` 已安装 | `ENV_CHECK.log` |
| build | `rdma-object-lifecycle` 编译成功 | `BUILD.log` |
| device list | `GET_DEVICE_LIST_OK count=1` | `OBJECT_LIFECYCLE.log` |
| device | `rxe0` / `uverbs0` | `OBJECT_LIFECYCLE.log` |
| context | `OPEN_DEVICE_OK name=rxe0` | `OBJECT_LIFECYCLE.log` |
| query | `max_qp=1048560`、`max_cq=1048576`、`max_mr=524287` | `OBJECT_LIFECYCLE.log` |
| MR | `STEP 07 REG_MR_OK ... lkey=0x4c5 rkey=0x4c5` | `OBJECT_LIFECYCLE.log` |
| CQ | `CREATE_CQ_OK depth=16` | `OBJECT_LIFECYCLE.log` |
| QP | `STEP 09 CREATE_QP_OK qp_num=19 qp_type=RC` | `OBJECT_LIFECYCLE.log` |
| lifecycle | `OBJECT_LIFECYCLE_PASS` | `OBJECT_LIFECYCLE.log` |

## 结论

这轮证明了：

- C verbs 程序可以编译和运行，`libibverbs` 基础开发链路可用。
- Soft-RoCE `rxe0` 可以被 `ibv_get_device_list()` 枚举。
- context/PD/MR/CQ/QP 可以创建并按反向顺序销毁。
- MR 注册能拿到 `lkey/rkey`，QP 创建能拿到 `qp_num`。

下一步进入 Phase 3：QP 状态机，重点验证 `RESET -> INIT -> RTR -> RTS`。
