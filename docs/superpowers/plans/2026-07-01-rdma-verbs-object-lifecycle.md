# RDMA Verbs Object Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 verbs 对象生命周期实验重构为模块清晰、可测试、文档深入的专业 C 工程。

**Architecture:** 使用一个资源聚合结构保存 device list、context、PD、MR、CQ 和 QP 的 ownership。设备、内存和队列分别由独立模块创建与销毁，`main.c` 只处理 CLI 和流程编排，所有失败路径进入统一逆序清理。

**Tech Stack:** C11、libibverbs、GNU Make、Bash、Soft-RoCE RXE

---

## File Map

- `Makefile`：根目录构建、测试和清理入口。
- `include/rdma_resources.h`：公共配置、资源结构和模块 API。
- `src/main.c`：CLI 解析、list 模式、生命周期编排和稳定输出。
- `src/rdma_device.c`：设备枚举、选择、context 与 port 查询。
- `src/rdma_memory.c`：PD、buffer 和 MR 生命周期。
- `src/rdma_queue.c`：CQ 与 RC QP 生命周期。
- `tests/lifecycle_test.sh`：CLI 回归测试及真实设备集成测试。
- `README.md`：唯一使用入口。
- `docs/ARCHITECTURE.md`：工程与系统调用架构。
- `docs/VERBS_OBJECT_MODEL.md`：verbs 对象深度原理。

### Task 1: Establish failing public-interface tests

**Files:**
- Create: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/tests/lifecycle_test.sh`

- [ ] 创建测试脚本，依次验证：二进制存在、`--help`、非法参数、`--list`、不存在设备、真实设备生命周期。
- [ ] 在旧工程上运行 `bash tests/lifecycle_test.sh`。
- [ ] 确认测试因根目录二进制及新 CLI 尚不存在而失败，而不是脚本语法错误。

测试要求的稳定接口：

```text
usage: rdma-lifecycle [--list] [--device NAME] [--port N]
result=pass
cleanup=complete
error=device_not_found
```

### Task 2: Add resource model and root build

**Files:**
- Create: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/include/rdma_resources.h`
- Create: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/Makefile`

- [ ] 定义 `rdma_options` 和 `rdma_resources`，字段覆盖所有 ownership。
- [ ] 声明 device、memory、queue 三组 create/destroy API 和统一 cleanup API。
- [ ] 根 Makefile 使用 `build/*.o` 生成 `build/rdma-lifecycle`，提供 `all/test/clean`。
- [ ] 运行 `make`，确认因实现文件尚不存在而按预期失败。

公共函数签名固定为：

```c
int rdma_list_devices(void);
int rdma_device_open(struct rdma_resources *res, const char *name, uint8_t port);
int rdma_memory_create(struct rdma_resources *res, size_t length);
int rdma_queue_create(struct rdma_resources *res);
void rdma_queue_destroy(struct rdma_resources *res);
void rdma_memory_destroy(struct rdma_resources *res);
void rdma_device_close(struct rdma_resources *res);
void rdma_resources_cleanup(struct rdma_resources *res);
```

### Task 3: Implement device ownership

**Files:**
- Create: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/src/rdma_device.c`

- [ ] 实现 device list 输出，格式为 `device[index]=name`，无设备时输出 `device_count=0`。
- [ ] 实现按名称选择设备；名称为空时选择第一个设备。
- [ ] 打开 context，查询 device 和 port，并拒绝越界 port。
- [ ] 关闭 context 后释放 device list，并将指针清空。
- [ ] 构建临时最小调用方，验证不存在设备路径可辨认。

错误路径输出固定包含：

```text
error=no_rdma_device
error=device_not_found device=<name>
error=invalid_port port=<n>
```

### Task 4: Implement memory and queue ownership

**Files:**
- Create: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/src/rdma_memory.c`
- Create: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/src/rdma_queue.c`

- [ ] memory 模块按 `PD -> aligned buffer -> MR` 创建。
- [ ] MR flags 使用 `LOCAL_WRITE | REMOTE_READ | REMOTE_WRITE`。
- [ ] memory 模块按 `MR -> buffer -> PD` 销毁并清空字段。
- [ ] queue 模块创建共享 send/recv CQ 和 RC QP。
- [ ] 使用 `ibv_query_qp()` 验证新建 QP 为 RESET。
- [ ] queue 模块按 `QP -> CQ` 销毁并清空字段。
- [ ] 运行 `make`，确认所有模块在 `-Wall -Wextra -Wpedantic` 下无警告。

### Task 5: Implement CLI orchestration

**Files:**
- Create: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/src/main.c`

- [ ] 使用 `getopt_long()` 解析 `--help`、`--list`、`--device`、`--port`。
- [ ] 严格校验端口为 `1..255` 的十进制整数。
- [ ] lifecycle 模式依次调用 device、memory 和 queue 模块。
- [ ] 每一步输出 `key=value`，失败时输出明确 error。
- [ ] 单一 `out` 路径调用 `rdma_resources_cleanup()`。
- [ ] 运行 Task 1 测试，确认 CLI 测试变绿；无设备环境只允许集成项 skip。

### Task 6: Write focused deep documentation

**Files:**
- Replace: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/README.md`
- Create: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/docs/ARCHITECTURE.md`
- Create: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/docs/VERBS_OBJECT_MODEL.md`

- [ ] README 说明阅读顺序、构建、CLI、测试和项目边界。
- [ ] ARCHITECTURE 包含模块 UML、系统分层、调用时序和错误回滚 Mermaid 图。
- [ ] VERBS_OBJECT_MODEL 深入解释 device/context/PD/MR/CQ/QP、key、安全边界、ownership 和 QP RESET。
- [ ] 特殊字符节点统一使用 `Node["text/path"]` 语法。
- [ ] 扫描 Mermaid fence 成对、旧脚本引用和乱码替换字符。

### Task 7: Remove obsolete workflow and verify locally

**Files:**
- Delete: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/app/`
- Delete: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/scripts/`
- Delete: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/records/`
- Delete: `linux-driver-lab/track-rdma-core/lab-rdma-verbs-object-lifecycle/reports/`
- Delete old four numbered files under `docs/`.

- [ ] 只在新工程构建及测试通过后删除旧结构。
- [ ] 运行 `rg` 确认没有旧脚本名、旧记录编号和旧目录说明。
- [ ] 运行 UTF-8 与 Mermaid 静态检查。
- [ ] 运行 `git diff --check`。

### Task 8: Verify on Soft-RoCE host

**Files:**
- No new tracked evidence directories.

- [ ] 同步该 lab 到 `192.168.65.135:/home/wq7/workspace/driver-lab/`。
- [ ] 执行 `make clean && make`。
- [ ] 执行 `make test`。
- [ ] 执行 `build/rdma-lifecycle --device rxe0 --port 1`。
- [ ] 确认 device/context/PD/MR/CQ/QP、RESET 状态、cleanup 和 result 全部正确。
- [ ] 将关键命令与实际结果报告给用户，不创建时间戳 records/reports。

## Constraints

- 不执行 `git commit`，由用户统一提交。
- 不修改本 lab 之外的用户改动。
- 不加入 QP 状态迁移、send/recv 或 RDMA READ/WRITE。
