# MR 实验工程架构

## 模块关系

```mermaid
classDiagram
    class Main {
      parse CLI
      open environment
      run suite
      cleanup
    }
    class DeviceModule {
      device list
      context
      port query
      PD
    }
    class ExperimentEngine {
      case table
      allocate buffer
      register MR
      compare expectation
      deregister MR
    }
    Main --> DeviceModule
    Main --> ExperimentEngine
    ExperimentEngine --> DeviceModule : uses PD
```

对象生命周期被刻意缩小为：

```mermaid
flowchart LR
    Device["verbs device"] --> Context["ibv_context"]
    Context --> PD["Protection Domain"]
    PD --> MR1["MR case 1"]
    PD --> MR2["MR case 2"]
    PD --> MRN["MR case N"]
    Buffer["userspace allocation"] --> MR1
```

每个 case 都独立分配 buffer、调用 `ibv_reg_mr()`、记录 key/errno、注销 MR 并释放 buffer。这样一个 case 的 key 和资源不会污染下一个 case。

## 调用时序

```mermaid
sequenceDiagram
    participant App
    participant Verbs as libibverbs/provider
    participant Kernel as uverbs/RDMA core/RXE

    App->>Verbs: ibv_open_device()
    App->>Verbs: ibv_alloc_pd()
    loop each MR case
        App->>App: posix_memalign()
        App->>Verbs: ibv_reg_mr(PD, address, length, flags)
        Verbs->>Kernel: register memory and permissions
        alt registration succeeds
            Kernel-->>App: MR + lkey + rkey
            App->>Verbs: ibv_dereg_mr()
        else registration rejected
            Kernel-->>App: NULL + errno
        end
        App->>App: free allocation
    end
    App->>Verbs: ibv_dealloc_pd()
    App->>Verbs: ibv_close_device()
```

## 控制面边界

```mermaid
flowchart TB
    Current["当前项目: 注册地址范围和权限"] --> Keys["获得 lkey/rkey"]
    Keys --> Later["后续 QP/WR 项目"]
    Later --> Local["SGE 使用 lkey"]
    Later --> Remote["RDMA READ/WRITE 使用 remote address + rkey"]
```

当前项目不创建 QP，所以只能证明 provider 接受或拒绝某种 MR 配置，不能证明远端操作一定成功。

## 错误与清理

```mermaid
flowchart TD
    Alloc["allocate buffer"] --> Register["ibv_reg_mr()"]
    Register -->|"success"| Observe["print addr/length/lkey/rkey"]
    Register -->|"failure"| Error["print errno/message"]
    Observe --> Dereg["ibv_dereg_mr()"]
    Dereg --> Free["free original allocation"]
    Error --> Free
```

非对齐 case 中，注册地址是 `allocation + 1`，但 `free()` 必须接收原始 `allocation`，不能释放偏移后的地址。
