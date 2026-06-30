# 02_TEST_AND_VERIFY

## 唯一运行入口

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle
bash scripts/run_all.sh
```

不要再分开跑多个脚本。这个 lab 现在只保留一个入口，避免流程分散。

## 运行后看什么

```bash
cat records/latest/SUMMARY.md
cat records/latest/OBJECT_LIFECYCLE.log
```

`SUMMARY.md` 看结论：

```text
BUILD_PASS
OBJECT_LIFECYCLE_PASS
```

`OBJECT_LIFECYCLE.log` 看学习步骤：

```text
STEP 00 GET_DEVICE_LIST_OK count=1
STEP 01 DEVICE[0] name=rxe0
STEP 02 OPEN_DEVICE_OK name=rxe0
STEP 07 REG_MR_OK ... lkey=... rkey=...
STEP 09 CREATE_QP_OK qp_num=...
OBJECT_LIFECYCLE_PASS
```

## 如果失败怎么办

| 现象 | 含义 | 下一步 |
| --- | --- | --- |
| `VERBS_HEADER_MISSING` | 没有 verbs 开发头文件 | 安装 `libibverbs-dev` |
| `NO_RDMA_DEVICES_FOUND` | 没有 verbs device | 回到 Phase 1 创建 `rxe0` |
| `REG_MR_FAIL` | MR 注册失败 | 看 PD、buffer、access flags |
| `CREATE_QP_FAIL` | QP 创建失败 | 看 CQ、PD、QP cap |

## 当前测试机状态

当前测试机已经创建 Soft-RoCE：

```text
rxe0 on ens34
```

所以这个 lab 的正常结果应该是：

```text
OBJECT_LIFECYCLE_PASS
```
