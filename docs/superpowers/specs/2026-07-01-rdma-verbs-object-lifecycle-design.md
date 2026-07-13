# RDMA Verbs Object Lifecycle Professionalization Design

## 1. Goal

将 `lab-rdma-verbs-object-lifecycle` 从按执行阶段堆叠脚本、记录和报告的实验目录，重构为一个可阅读、可扩展、可测试的专业 C 工程。

本项目只负责 RDMA verbs 基础对象的创建、查询和销毁：

```text
device -> context -> PD -> buffer/MR -> CQ -> RC QP -> reverse cleanup
```

QP 状态迁移、消息收发和 RDMA READ/WRITE 不进入本项目，分别由后续项目承载。

## 2. Success Criteria

- 根目录提供标准 `Makefile`，支持 `make`、`make test` 和 `make clean`。
- 程序支持 `--list`、`--device NAME`、`--port N` 和 `--help`。
- 设备、内存、队列资源由独立 C 模块管理，公共接口集中在头文件中。
- 所有失败路径进入统一的反向资源回收流程。
- 自动化测试检查参数行为、构建结果和真实 verbs 生命周期结果。
- `docs/` 只保留两篇深度文档，并包含可渲染的 Mermaid UML、时序图、流程图和状态关系图。
- 不再维护时间戳 `records/`、阶段 `reports/` 或多段执行脚本。

## 3. Directory Structure

```text
lab-rdma-verbs-object-lifecycle/
|-- README.md
|-- Makefile
|-- include/
|   `-- rdma_resources.h
|-- src/
|   |-- main.c
|   |-- rdma_device.c
|   |-- rdma_memory.c
|   `-- rdma_queue.c
|-- tests/
|   `-- lifecycle_test.sh
`-- docs/
    |-- ARCHITECTURE.md
    `-- VERBS_OBJECT_MODEL.md
```

构建生成物放入 `build/`，不作为源码结构的一部分。

## 4. Module Design

### 4.1 Public resource model

`include/rdma_resources.h` 定义：

- `struct rdma_options`：设备名、端口号和命令模式。
- `struct rdma_resources`：device list、device、context、device/port attributes、PD、buffer、MR、CQ 和 QP。
- 设备层、内存层、队列层的创建与销毁接口。
- 稳定的返回值约定：成功为 `0`，失败为负 errno 风格值。

资源结构显式保存所有 ownership，调用者无需猜测哪个模块持有什么对象。

### 4.2 Device module

`src/rdma_device.c` 负责：

- 枚举 verbs device。
- 按名称选择设备；未指定时选择第一个设备。
- 打开 context。
- 查询 device 和指定 port。
- 校验 port 范围与状态。
- 关闭 context 并释放 device list。

### 4.3 Memory module

`src/rdma_memory.c` 负责：

- 创建 PD。
- 分配页对齐 buffer。
- 注册具有本地写、远端读和远端写权限的 MR。
- 注销 MR、释放 buffer、销毁 PD。

文档明确区分虚拟地址、页锁定、DMA 映射、`lkey` 与 `rkey` 的作用。

### 4.4 Queue module

`src/rdma_queue.c` 负责：

- 创建 CQ。
- 创建 RC QP。
- 查询并输出初始 QP 状态。
- 按 `QP -> CQ` 顺序销毁队列资源。

本模块不执行 `RESET -> INIT -> RTR -> RTS`，只证明 QP 对象可以建立且初始状态符合预期。

### 4.5 Main program

`src/main.c` 只负责：

1. 解析参数。
2. 执行 list 模式，或按顺序调用三个资源模块。
3. 输出稳定的 `key=value` 结果。
4. 无论在哪一步失败，都调用统一清理入口。

核心输出包括：

```text
device=rxe0
port=1
context=ready
pd=ready
mr=ready lkey=0x... rkey=0x...
cq=ready
qp=ready qp_num=...
cleanup=complete
result=pass
```

## 5. Error Handling And Ownership

每个创建函数只提交完整成功的资源；失败时返回明确错误。顶层清理函数允许字段为空，因此可以安全处理任何中途失败。

销毁顺序固定为：

```text
QP -> CQ -> MR -> buffer -> PD -> context -> device list
```

清理操作记录首个错误，但继续尝试释放剩余资源，避免一个销毁失败阻断整个回收过程。

## 6. Testing Strategy

`tests/lifecycle_test.sh` 是唯一测试入口，由 `make test` 调用：

1. `--help` 返回成功并包含 usage。
2. 非法参数返回失败。
3. `--list` 可执行，并正确表达有设备或无设备状态。
4. 指定不存在的 device 必须返回失败。
5. 有 verbs device 时，完整运行必须输出 `result=pass` 和 `cleanup=complete`。
6. 无 verbs device 时，测试明确输出 skip，而不是伪造通过。

真实 Soft-RoCE 测试仍在 `192.168.65.135` 的 `rxe0` 上执行。

## 7. Documentation Design

### `docs/ARCHITECTURE.md`

- 工程模块及依赖边界。
- 程序调用流程。
- ownership 与错误回滚。
- 用户态应用、libibverbs、provider、uverbs、RDMA core 和 RXE 的系统位置。
- Mermaid component、sequence、flowchart 图。

### `docs/VERBS_OBJECT_MODEL.md`

- device/context/PD/MR/CQ/QP 的定义和依赖关系。
- PD 隔离模型与 key 授权模型。
- CQ、CQE、SQ、RQ、WR、WQE 的关系。
- 创建与逆序销毁原因。
- QP 初始 RESET 状态与后续项目边界。
- Mermaid classDiagram、sequenceDiagram、stateDiagram 和关系图。

Mermaid 中含路径、斜线或特殊字符的节点文本统一放在双引号中，保证渲染兼容性。

## 8. Migration

- 将原 `app/main.c` 中可复用逻辑迁入新模块，而不是保留双份实现。
- 删除 `app/`、`scripts/`、`records/` 和 `reports/`。
- 将原四篇 docs 中仍准确的知识合并、改写进两篇新文档。
- README 成为唯一入口，只说明目标、阅读顺序、构建运行和下一项目。
- 不保留旧记录编号或易过期的硬编码运行结果。

## 9. Non-Goals

- 不实现 QP 状态迁移。
- 不创建两个通信端点。
- 不 post send/recv WR。
- 不实现 RDMA READ/WRITE/atomic。
- 不引入 CMake、Meson、日志框架或第三方测试框架。
