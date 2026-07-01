# RDMA Verbs Object Lifecycle

这是一个按真实 C 工程方式组织的 RDMA 基础项目。它只解决一个问题：

> 一个用户态 RDMA 程序怎样发现设备，并安全地创建、使用和销毁 verbs 基础对象？

项目覆盖以下对象链：

```text
device list -> device -> context -> PD -> buffer/MR -> CQ -> RC QP
```

QP 状态迁移、Send/Recv 和 RDMA READ/WRITE 属于后续项目，不在这里混杂实现。

## 阅读顺序

1. `include/rdma_resources.h`：先看全部资源和模块接口。
2. `src/main.c`：看程序如何编排完整生命周期。
3. `src/rdma_device.c`：理解 device、context、port。
4. `src/rdma_memory.c`：理解 PD、buffer、MR、lkey/rkey。
5. `src/rdma_queue.c`：理解 CQ、QP 和初始 RESET 状态。
6. `docs/ARCHITECTURE.md`：理解工程边界和 Linux RDMA 软件栈。
7. `docs/VERBS_OBJECT_MODEL.md`：深入理解 verbs 对象依赖和数据面语义。

## 工程结构

```text
.
|-- Makefile
|-- include/rdma_resources.h
|-- src/
|   |-- main.c
|   |-- rdma_device.c
|   |-- rdma_memory.c
|   `-- rdma_queue.c
|-- tests/lifecycle_test.sh
`-- docs/
    |-- ARCHITECTURE.md
    `-- VERBS_OBJECT_MODEL.md
```

`build/` 是 `make` 自动生成的构建目录，不属于源码。

## 构建与运行

```bash
make
build/rdma-lifecycle --list
build/rdma-lifecycle --device rxe0 --port 1
```

完整测试：

```bash
make test
```

完整的测试环境、逐步命令和真实输出记录：

```text
tests/TEST_RECORD_20260701.md
```

清理构建产物：

```bash
make clean
```

## 命令行接口

```text
usage: rdma-lifecycle [--list] [--device NAME] [--port N]
```

| 参数 | 含义 |
| --- | --- |
| `--list` | 列出 libibverbs/provider 可见的 RDMA 设备 |
| `--device NAME` | 选择设备；省略时选择第一个设备 |
| `--port N` | 选择物理端口，默认是 1 |
| `--help` | 显示帮助 |

成功输出示例：

```text
device=rxe0
port=1 state=4 active_mtu=3
context=ready
pd=ready
mr=ready address=0x... length=4096 lkey=0x... rkey=0x...
cq=ready depth=16
qp=ready qp_num=... qp_type=RC
qp_state=RESET
cleanup=complete
result=pass
```

这里最重要的不是记住数字，而是理解：谁创建它、谁依赖它、何时销毁它。

## 当前边界

本项目已经完成：

- provider device 枚举与选择。
- context、port 和 device capability 查询。
- PD、页对齐 buffer、MR 创建。
- CQ、RC QP 创建与 RESET 状态查询。
- 任意失败点的统一逆序清理。
- CLI 与真实 verbs device 自动测试。

本项目有意不做：

- `RESET -> INIT -> RTR -> RTS`。
- GID、QPN、PSN 等连接参数交换。
- `ibv_post_recv()`、`ibv_post_send()` 和 CQ polling。
- RDMA READ、WRITE、atomic。

下一项目将复用这里的资源模型，专门实现 QP 状态机。
