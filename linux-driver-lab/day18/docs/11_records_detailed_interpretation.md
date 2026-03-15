# Day18 records 详细解读（基于 2026-03-15 最新结果）

## 1. 本文目的

这份文档专门回答两个问题：

1. `records/` 下面每个结果文件到底在说明什么。
2. 为什么仅靠这些文件，就可以把 Day18 当前这轮测试判定为“通过”。

本次主要依据以下三组最新 records：

- `records/20260315-142352-day18-baseline-arm64-virt`
- `records/20260315-142411-day18-round2b_legacy-arm64-virt`
- `records/20260315-142432-day18-classified-arm64-virt`

以及两组汇总文件：

- `records/compare-20260315-142441.md`
- `records/equivalence-round2b_legacy-vs-classified.txt`

## 2. 先看哪几个文件，最快判断这轮是否通过

如果只是快速判断是否通过，优先看这 6 类文件：

1. `metrics.env`
2. `guest_cmd_rc.txt`
3. `serial.log`
4. `dmesg_tail.txt`
5. `build_evidence/artifact_evidence.env`
6. `compare-*.md` 与 `equivalence-*.txt`

原因很简单：

- `metrics.env` 给出最终的 yes/no 验收项
- `guest_cmd_rc.txt` 证明 guest 采集脚本有没有异常退出
- `serial.log` 是完整串口证据，能看到系统启动、加载模块、执行采集命令的全过程
- `dmesg_tail.txt` 可以直接看模块 probe、IRQ、worker 是否正常
- `artifact_evidence.env` 说明这一轮到底产出了什么 `.config / Image / rootfs / ko`
- `compare / equivalence` 用来回答三组 profile 的关系，而不是只看单轮

## 3. `metrics.env`：最终总成绩单

以 `classified` 为例，`metrics.env` 中最重要的字段是：

- `boot_ok=yes`
- `debugfs_ok=yes`
- `tracing_ok=yes`
- `function_graph_ok=yes`
- `trace_smoke_ok=yes`
- `perf_bin_ok=yes`
- `perf_smoke_ok=yes`
- `insmod_ok=yes`
- `snapshot_ok=yes`
- `trigger_ok=yes`
- `dmesg_warn=no`

这串结果的含义是：

### 3.1 `boot_ok=yes`
说明系统已经从 QEMU 拉起，并且进入了 guest shell，不是只做到内核编译成功。

### 3.2 `debugfs_ok=yes` + `tracing_ok=yes`
说明调试文件系统和 tracing 子系统都可用。Day18 的核心目标之一就是“裁剪后仍保留观测能力”，这两个字段是最直观的证据。

### 3.3 `function_graph_ok=yes`
说明 function_graph tracer 确实可用，而不是只打开了部分 ftrace 基础开关。

### 3.4 `trace_smoke_ok=yes`
说明不只是目录存在，而是已经做过一次最小可运行验证。

### 3.5 `perf_bin_ok=yes` + `perf_smoke_ok=yes`
说明 perf 不仅被打进 rootfs，而且 guest 内能实际执行。这个点很关键，因为 Day18 中 perf 是动态链接目标，只有把解释器和依赖库也放进 rootfs，guest 内执行才会成功。

### 3.6 `insmod_ok=yes`
说明 `demo_regmap.ko` 已经真正插入成功。Day18 不是纯文档实验，它还要求带着一个实际模块跑通。

### 3.7 `snapshot_ok=yes` + `trigger_ok=yes`
说明模块的 debugfs 节点和触发链路可用，证明 demo 模块不是“只插进去”，而是能被读、被触发。

### 3.8 `dmesg_warn=no`
说明本轮采集里没有命中的告警模式。它不是绝对安全证明，但至少当前 smoke 结果很干净。

## 4. `guest_cmd_rc.txt`：确认 guest 采集脚本没有异常退出

三组最新 records 的 `guest_cmd_rc.txt` 都是：

```text
__DAY18_GUEST_CMD_RC__0
```

这里的 `0` 就是 shell 返回码。它的意义很直接：

- `0`：`/bin/day18_guest_collect.sh` 在 guest 里完整执行完了
- 非 `0`：即使 `metrics.env` 里有些内容，也要怀疑采集是否中途失败

所以，这个文件是“采集动作本身成功”的证据。

## 5. `serial.log`：最强的一手现场证据

`serial.log` 是整个启动和采集过程的原始串口日志。它的价值最高，因为很多别的文件其实都是从这条串口链上切出来的。

以 baseline/classified 为例，串口日志里能看到这些关键点：

### 5.1 出现 shell 提示符 `~ #`
这证明 guest 已经真正启动并进入交互 shell。

### 5.2 出现握手标志 `__DAY18_HOST_HANDSHAKE__1`
说明宿主机脚本和 guest 串口之间的同步成功了。

### 5.3 出现 guest 采集命令
串口里可以看到：

- 调用 `/bin/day18_guest_collect.sh ...`
- 命令执行完成后输出 `__DAY18_GUEST_CMD_RC__0`

这说明采集不是手工胡乱敲出来的，而是脚本自动驱动的。

### 5.4 能看到 `demo_regmap` 的加载与 probe
串口中明确有这些日志：

- `demo_regmap: module init`
- `probe begin`
- `probe ok`
- `debugfs: /sys/kernel/debug/demo_regmap/{snapshot,poke,trigger}`
- `top-half irq=49`
- `worker start batch=1 latency=...`

这串日志证明：

- 模块确实装上了
- 设备树匹配成功
- reg/irq 信息解析成功
- IRQ top-half 触发了
- worker 也跑了

也就是说，模块链不是“静态存在”，而是已经参与了一次最小真实工作流。

### 5.5 看到 `perf version 5.15.10`
说明 guest 里执行的是与当前内核工具链配套的 perf，不是宿主机 perf 混入。

## 6. `dmesg_tail.txt`：重点看模块和功能点是否闭环

`dmesg_tail.txt` 是从串口或 dmesg 中抽取的精简版，适合快速看模块是否跑对。

这里最关键的几行是：

- `probe ok: label=regmap-demo ... linux_irq=49 work_ms=20`
- `debugfs: /sys/kernel/debug/demo_regmap/{snapshot,poke,trigger}`
- `top-half irq=49 irq_count=1 pending=1`
- `worker start batch=1 latency=...`

解读方式如下：

- `probe ok`：设备树、platform_driver、regmap、irq 注册都已经成功
- `debugfs` 节点存在：后续 `snapshot/trigger` 有地方可以操作
- `top-half irq=49`：中断顶半部发生过
- `worker start ...`：底半部/工作队列被调度执行了

这正是 Day18 里 `demo_regmap` 这个实验模块存在的意义：

> 不是只让系统能开机，而是保留一个具体、可观测、可触发的模块实验对象。

## 7. `snapshot.txt`：证明模块状态能被读出

`snapshot.txt` 给出了 `demo_regmap` 的寄存器状态快照，例如：

- `CTRL`
- `STATUS`
- `IRQ_COUNT`
- `WORK_RUNS`
- `PENDING_EVENTS`
- `LAST_LATENCY_US`
- `WORK_MS`
- `VERSION`

这里判断通过，不是看某个数字必须多大，而是看两件事：

1. 文件能成功读取
2. 内容结构完整，和模块设计一致

只要能读出结构化状态，并且模块/label/irq/reg 信息对得上，就说明模块的 debugfs 快照路径是通的。

## 8. `mount.txt` / `filesystems.txt` / `tracing_dir.txt`：证明观测基础设施真的挂上了

### 8.1 `mount.txt`
这里最关键的是能看到：

- `debugfs on /sys/kernel/debug`
- `tracefs on /sys/kernel/tracing`

这说明不是只在 `.config` 里把功能打开，而是运行态真的挂载成功。

### 8.2 `filesystems.txt`
可以看到：

- `nodev debugfs`
- `nodev tracefs`
- `nodev devtmpfs`
- `nodev proc`
- `nodev sysfs`

说明系统镜像具备这些文件系统支持。

### 8.3 `tracing_dir.txt`
这里显示的是：

```text
/sys/kernel/debug/tracing
```

说明采集脚本找到的 tracing 入口正确。这个文件看似简单，但很有用：它证明 guest 侧不是误判了别的目录。

## 9. `available_tracers.txt`：证明 function_graph 真的在 tracer 列表里

文件内容中包含：

- `function_graph`
- `function`
- `irqsoff`
- `nop`

只要 `function_graph` 明确存在，就能支撑 `function_graph_ok=yes` 这一结论。它比只看 `.config` 更强，因为这是运行态证据。

## 10. `modules.txt`：确认只加载了预期模块

三组最新 records 都只看到：

- `demo_regmap ... Live ...`

这说明当前实验环境很干净，至少在 `lsmod` 结果里没有额外模块干扰。`modules_loaded_count=1` 与这个文件是一致的。

## 11. `perf_version.txt` / `perf_stat.txt` / `perf_manifest.txt`

这三类文件共同支撑 perf 验收。

### 11.1 `perf_version.txt`
文件内容是：

```text
perf version 5.15.10
```

说明 guest 中的 perf 可执行文件存在，而且可以正常启动到输出版本号。

### 11.2 `perf_stat.txt`
这里能看到：

- `Performance counter stats for '/bin/true'`
- `task-clock`
- `time elapsed`
- `user/sys`

这说明 perf 不只是能执行 `--version`，而是确实跑了一次最小统计命令。

### 11.3 `perf_manifest.txt`
这个文件能看到：

- perf 可执行文件路径
- 解释器 `ld-linux-aarch64.so.1`
- `libc.so.6`、`libm.so.6` 等依赖被解析到目标路径

这说明 Day18 里“把动态链接 perf 打到 rootfs”这条链也闭环了。特别是在前面调试过 `PERF_LIB_DIRS` 的背景下，这个文件很有价值。

## 12. `build_evidence/artifact_evidence.env`：构建产物层证据

这个文件回答的是：这一轮 profile 最终到底生成了什么。

最重要的字段有：

- `profile`
- `scenario_id`
- `kernel_release`
- `kernel_config_sha256`
- `kernel_image_sha256`
- `rootfs_img_sha256`
- `module_demo_sha256`
- `perf_manifest_present`
- `category_manifest_present`

怎么读：

### 12.1 `kernel_config_sha256`
它是最终 `.config` 的指纹。当前三组都是同一个 sha256，这说明三组 profile 最后收敛到了相同的最终配置。

### 12.2 `kernel_image_sha256`
它是最终 `Image` 的指纹。当前三组相同，说明最终内核镜像一致。

### 12.3 `rootfs_img_sha256`
三组 rootfs sha 不同，但 rootfs 大小相近。这里更适合解释为打包过程差异，而不是功能差异证据。

### 12.4 `module_demo_sha256`
三组相同，说明 demo 模块构建结果一致。

### 12.5 `perf_manifest_present=yes`
说明这一轮确实做了 perf 打包证据归档，而不是只靠口头判断。

### 12.6 `category_manifest_present=yes`
说明 Day18 的分类配置文件已经随证据链归档。

## 13. `applied_fragments.txt`：证明不同 profile 的“说法”确实不同

这是 Day18 很有代表性的文件。

### 13.1 baseline
只应用：

- `trace_baseline.fragment`

### 13.2 round2b_legacy
应用：

- `trace_baseline.fragment`
- `trim_round1.fragment`
- `trim_round2b.fragment`

### 13.3 classified
应用：

- `trace_baseline.fragment`
- `10_required.fragment`
- `20_platform.fragment`
- `30_debug.fragment`
- `40_perf.fragment`
- `90_trim_day18.fragment`

这说明：

- 三组 profile 的**表达方式**确实不一样
- 尤其是 `classified` 已经变成按类别组织

即使最终 `kernel.config` 相同，这个文件仍然有价值，因为它证明 Day18 的“分类表达重构”不是空话。

## 14. `compare-20260315-142441.md`：三组 profile 的总对比

这份 compare 摘要给出三类结论：

### 14.1 功能结论
三组都是 PASS，而且关键功能项全通过。

### 14.2 指标结论
本轮数据中：

- `image_kib` 相同
- `rootfs_kib` 相同
- `memfree_kib` 基本相同
- `boot_ms` baseline 更小，另外两组略大

这说明：

- Day18 当前最强的结论是“功能等价”
- 不是“trim profile 已经在体积/性能上显著优于 baseline”

### 14.3 证据结论
compare 里明确写出：

- baseline vs round2b_legacy：`changed_config_lines=0 sha_equal=yes`
- baseline vs classified：`changed_config_lines=0 sha_equal=yes`
- round2b_legacy vs classified：`changed_config_lines=0 sha_equal=yes`

这意味着：

- 三组最终 `kernel.config` 没差别
- `round2b_legacy` 与 `classified` 至少可以认定为最终结果等价
- baseline 当前这轮也收敛到了同一配置，因此这轮更适合得出“闭环通过”结论，而不是“收益量化完成”

## 15. `equivalence-round2b_legacy-vs-classified.txt`：Day18 最关键的专属证据

这份文件最重要，因为它专门回答 Day18 的原始问题：

> Day18 的分类写法，到底是不是把 day17 legacy 结果正确重构了？

文件里最重要的两句是：

- `kernel.config : identical`
- 两边 `sha256` 完全相同

这条证据能直接支撑：

> `classified` 与 `round2b_legacy` 在最终配置层面完全等价。

这正是 Day18 最核心的工程价值之一。

## 16. 对 `records/` 的最终判读

综合上面所有文件，可以把这轮结果概括成：

### 16.1 可以明确判为“通过”的部分

- 系统启动通过
- 模块装载通过
- debugfs / tracefs / tracing / function_graph 通过
- perf 集成与 smoke 通过
- 三组 profile 全部 PASS
- `round2b_legacy` 与 `classified` 等价验证通过

### 16.2 需要如实说明的部分

- 当前 baseline / round2b_legacy / classified 的最终 `kernel.config` 与 `Image` 指纹相同
- 因此本轮最强结论是“方法和表达重构通过”，不是“收益量化显著”

### 16.3 对 Day18 是否通过的建议表述

建议在 README 或最终汇报里写成：

> **Day18 通过。**
>
> 本轮 records 已证明 Day18 的独立目录、三组 profile 执行链、模块/观测/perf 闭环、以及 `round2b_legacy` → `classified` 的等价重构都成立。
>
> 同时需要说明，本轮对比更强地证明了“功能与方法成立”，而不是“baseline 与 trim profile 的收益差异已经被量化证明”。
