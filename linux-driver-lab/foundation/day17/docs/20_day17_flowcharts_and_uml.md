# Day17 流程图与 UML 速览

## 1. 文档目的

本文用流程图、时序图和结构图来帮助快速理解 Day17。

适合在你已经知道 Day17 能跑的前提下，用图去建立整体感：
- 哪些脚本负责什么
- 配置链和构建链怎么串起来
- host / guest 怎么交互
- records / evidence 为什么重要

---

## 2. Day17 全局执行流

```mermaid
flowchart TD
    A[选择 Profile<br/>baseline / round1 / round2b] --> B[apply_config.sh<br/>叠加 fragment / 收敛配置]
    B --> C[build.sh<br/>模块 + rootfs + perf + dtb]
    C --> D[run_qemu.sh / QEMU 启动]
    D --> E[guest_collect.sh<br/>guest 内验证 tracing / perf / demo]
    E --> F[host_collect.sh<br/>从 serial.log 提取 env/file block]
    F --> G[records/<timestamp>-scenario/]
    G --> H[compare_results.py<br/>compare.md / compare.csv / *.diff]
```

---

## 3. profile 配置链流程图

```mermaid
flowchart LR
    P[PROFILE] --> Q{选择 profile}
    Q -->|baseline| F1[trace_baseline.fragment]
    Q -->|round1| F2[trace_baseline.fragment + trim_round1.fragment]
    Q -->|round2b| F3[trace_baseline.fragment + trim_round1.fragment + trim_round2b.fragment]
    F1 --> C[apply_symbol/apply_fragment_file]
    F2 --> C
    F3 --> C
    C --> D[olddefconfig]
    D --> E[最终 kernel.config]
```

### 理解重点
- profile 的本质不是“换一份完整 `.config`”
- 而是在 baseline 上叠加不同层的 trim

---

## 4. build.sh 构建链流程图

```mermaid
flowchart TD
    A[build.sh 开始] --> B[编译 demo_regmap.ko]
    B --> C[准备 BusyBox rootfs]
    C --> D[补最小 applet 链接]
    D --> E{是否需要 perf}
    E -->|yes| F[build_perf.sh / 发现已有 perf]
    F --> G[复制 perf 与动态依赖]
    E -->|no| H[跳过 perf]
    G --> I[注入 DT fragment]
    H --> I
    I --> J[生成 rootfs.img]
    J --> K[生成 virt-day17.dtb]
    K --> L[启动 QEMU]
```

### 理解重点
- build.sh 是 Day17 的“产物总装器”
- 它把模块、rootfs、perf、dtb 和 QEMU 启动统一收口

---

## 5. host / guest 交互时序图

```mermaid
sequenceDiagram
    participant Host as host_collect.sh
    participant QEMU as QEMU serial
    participant Guest as guest shell
    participant GC as guest_collect.sh

    Host->>QEMU: 启动 QEMU
    QEMU-->>Host: serial 输出 boot log
    QEMU-->>Host: 出现 prompt (~ #)
    Host->>Guest: 回车 + handshake token
    Guest-->>Host: 回显 handshake token
    Host->>Guest: 执行 /bin/day17_guest_collect.sh
    Guest->>GC: 进入采样脚本
    GC-->>QEMU: 输出 __DAY17_ENV_BEGIN__/END__
    GC-->>QEMU: 输出 __DAY17_FILE_BEGIN__/END__
    GC-->>QEMU: 输出 __DAY17_GUEST_CMD_RC__0
    Host->>QEMU: 读取 serial.log
    Host->>Host: 提取 env/file block
    Host->>Host: 生成 metrics.env / baseline.csv
```

### 理解重点
- guest 并不直接写宿主机文件
- host 通过 marker 从串口流里“切块”拿结果

---

## 6. guest_collect 内部流程图

```mermaid
flowchart TD
    A[guest_collect.sh 开始] --> B[准备 /tmp/day17-baseline]
    B --> C[检查 debugfs / tracing]
    C --> D[检查 function_graph]
    D --> E[insmod demo_regmap.ko]
    E --> F[读取 snapshot / trigger]
    F --> G[检查 perf 是否可执行]
    G --> H[执行 perf stat smoke]
    H --> I[生成 metrics.env]
    I --> J[通过 marker 输出 env/file block]
```

### 理解重点
- guest_collect 不是“复杂业务脚本”，而是 Day17 的最小实验验证器
- 它的价值在于把验收项格式化输出出来

---

## 7. compare 与 evidence 流程图

```mermaid
flowchart TD
    A[run_profile_collect.sh] --> B[profile 跑完]
    B --> C[保存 build_evidence]
    C --> D[kernel.config / Image.sha256 / rootfs.sha256 / applied_fragments]
    D --> E[run_compare_rounds.sh]
    E --> F[compare_results.py]
    F --> G[compare.csv]
    F --> H[compare.md]
    F --> I[baseline_vs_round1.diff]
    F --> J[round1_vs_round2b.diff]
    F --> K[baseline_vs_round2b.diff]
```

### 理解重点
- evidence 的目的是把“我觉得生效了”变成“我能证明生效了”

---

## 8. Day17 组件关系 UML（静态结构图）

```mermaid
classDiagram
    class ApplyConfig {
        +selectProfile()
        +buildFragmentChain()
        +applyFragmentFile()
        +olddefconfig()
    }

    class BuildSh {
        +buildModule()
        +prepareRootfs()
        +installPerf()
        +packRootfs()
        +injectDT()
        +launchQemu()
    }

    class BuildPerf {
        +buildPerfBinary()
        +emitManifest()
    }

    class GuestCollect {
        +checkTracing()
        +checkFunctionGraph()
        +checkDemoModule()
        +checkPerf()
        +emitMarkers()
    }

    class HostCollect {
        +waitPrompt()
        +serialHandshake()
        +runGuestCollect()
        +extractEnvBlock()
        +extractNamedBlock()
        +mergeMetrics()
    }

    class CompareResults {
        +loadMetrics()
        +loadEvidence()
        +writeCSV()
        +writeMD()
        +writeDiff()
    }

    class Records {
        +metrics.env
        +baseline.csv
        +serial.log
        +build_evidence/
    }

    ApplyConfig --> BuildSh : 提供 profile 配置
    BuildPerf --> BuildSh : 提供 perf 产物
    BuildSh --> GuestCollect : 启动 guest 并提供 rootfs
    HostCollect --> GuestCollect : 通过串口触发执行
    GuestCollect --> Records : 输出 guest markers
    HostCollect --> Records : 提取并落盘
    CompareResults --> Records : 汇总 compare 与 diff
```

---

## 9. round1 / round2b 关系图

```mermaid
flowchart LR
    A[baseline<br/>trace_baseline] --> B[round1<br/>baseline + 去 PCI/SCSI]
    B --> C[round2b<br/>round1 + 去 NET]
```

### 理解重点
- round1 不是从零开始另起一套配置
- round2b 也不是独立 profile，而是在 round1 基础上继续向下裁

---

## 10. Day17 结果解读图

```mermaid
flowchart TD
    A[先看 compare.md] --> B{status 是否 PASS}
    B -->|否| C[优先查 remarks / serial.log / metrics.env]
    B -->|是| D[看 kernel_config_sha256 是否变化]
    D -->|否| E[说明 profile 没真正改到最终 .config]
    D -->|是| F[看 kernel_image_sha256 是否变化]
    F -->|否| G[说明 config 变了，但当前产物路径未受影响]
    F -->|是| H[说明裁剪已经真实改变内核产物]
    H --> I[再结合 image_kib / memfree_kib / boot_ms 评估收益]
```

---

## 11. 一句话总结

> **Day17 最值得记住的不是某个单独脚本，而是“配置链 → 构建链 → 采样链 → evidence 链”这四条主线；一旦按这四条线理解，整个 day17 目录就会变得非常清晰。**
