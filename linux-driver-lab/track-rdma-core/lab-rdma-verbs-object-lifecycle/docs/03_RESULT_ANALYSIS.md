# 03_RESULT_ANALYSIS

## 本 lab 的判断标准

这个 lab 不看吞吐、不看延迟，只看一件事：

```text
RDMA verbs 基础对象能不能按顺序创建，再按反向顺序销毁
```

## 成功路径

```text
STEP 00 GET_DEVICE_LIST_OK count=1
STEP 02 OPEN_DEVICE_OK
STEP 05 ALLOC_PD_OK
STEP 07 REG_MR_OK
STEP 08 CREATE_CQ_OK
STEP 09 CREATE_QP_OK
OBJECT_LIFECYCLE_PASS
STEP 10 DESTROY_QP_OK
STEP 10 DESTROY_CQ_OK
STEP 10 DEREG_MR_OK
STEP 10 DEALLOC_PD_OK
STEP 10 CLOSE_DEVICE_OK
```

## 当前成功记录

```text
records/20260630-233918-verbs-object/
```

关键值：

| 字段 | 当前记录 | 意义 |
| --- | --- | --- |
| device | `rxe0` | Soft-RoCE verbs device |
| dev_name | `uverbs0` | 用户态 verbs 设备节点 |
| MR | `lkey=0x3a5 rkey=0x3a5` | 本地/远端访问 key |
| QP | `qp_num=18` | 后续 QP 状态机要用的本端 QP number |
| QP type | `RC` | Reliable Connected |

## 真正学到什么

- `ibv_get_device_list()` 证明 verbs 能看到 `rxe0`。
- `ibv_open_device()` 证明 context 可以打开。
- `ibv_alloc_pd()` 证明可以建立资源隔离域。
- `ibv_reg_mr()` 证明用户态 buffer 可以注册成 MR，并拿到 `lkey/rkey`。
- `ibv_create_cq()` 证明完成队列可以创建。
- `ibv_create_qp()` 证明 RC QP 可以创建。

## 下一阶段为什么是 QP 状态机

当前 QP 只是“创建成功”，但刚创建出来还不能收发。

下一阶段要学：

```text
RESET -> INIT -> RTR -> RTS
```

只有进入 RTS，QP 才真正可以 post send。
