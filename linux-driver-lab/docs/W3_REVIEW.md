# W3 任务分析与阶段复盘（D15-D21）

## 1. 目标

W3 的主题不是继续堆一个新的驱动功能，而是把前面 W1 / W2 已经跑通的实验环境，整理成一套：

- 有基线
- 可裁剪
- 可测量
- 可回归
- 可回滚
- 可复用讲解

的最小内核实验平台。

这一步非常关键，因为到了 W2 末尾，仓库已经不再只是“一个模块能加载起来”的状态，而是已经具备：

- arm64 + QEMU virt 启动链路
- DT 注入
- platform_driver / of_match
- IRQ 注册与触发
- regmap 读写封装
- debugfs 可观测性
- ftrace / function_graph 跟踪

如果没有 W3，这些能力虽然“能跑”，但还缺少三件工程化的东西：

1. **统一基线**：到底以哪个配置、哪个 rootfs、哪个 day 作为后续对比标准
2. **统一采样**：镜像大小、启动时间、内存占用、模块数如何以一致口径采集
3. **统一回归**：裁剪后怎么快速判断“还能不能启动、功能有没有坏”

所以 W3 的真正目标可以概括成一句话：

> 把当前“能跑的教学环境”，收敛成“可裁剪、可对比、可回归、可回滚”的最小实验平台。

---

## 2. 当前代码现状判断

结合当前上传代码，W1 / W2 的主线已经比较清晰：

### W1 已完成的能力

- 字符设备骨架
- ioctl
- sysfs / debugfs
- waitqueue / workqueue
- 基础回归脚本

### W2 已完成的能力

- Day08：platform_driver + probe/remove
- Day09：Device Tree + reg/irq 解析
- Day10：IRQ 注册 + `/proc/interrupts` 计数
- Day11：bottom-half(workqueue) + 延迟统计
- Day12：regmap + debugfs 快照
- Day13：function_graph 跟踪 IRQ 路径
- Day14：bring-up checklist 文档化

也就是说，到了当前阶段，仓库已经具备一个比较合适的 W3 基础：

- 已经切换到 **arm64 / QEMU virt** 路线
- 已经有 **BusyBox initramfs** 模型
- 已经有 `/init` 启动流程
- 已经可以在 guest 中使用 `debugfs` 与 `ftrace`

但是，W3 相关的产物目前还没有正式建立：

- 还没有冻结的 baseline `.config`
- 还没有 config fragment / defconfig 管理
- 还没有统一的 size / boot / mem 采集脚本
- 还没有 perf 用户态工具进入 rootfs 的方案
- 还没有 W3 的自动回归 harness
- 还没有裁剪前后对比报告

所以 W3 的第一步不是“继续加功能”，而是“先定义标准”。

---

## 3. W3 的推荐基线选择

结合当前仓库状态，建议把 **Day13 这条 arm64 路线** 作为 W3 的统一基线。

### 推荐原因

1. Day13 已经使用了 `ftrace function_graph`
2. W3 明确要求保留 tracing / perf
3. Day09-Day13 这条链已经形成完整的 arm64 bring-up 主线
4. 后面 D20 回归时，适合继续以 `demo_regmap.ko` 作为验证对象

### 不建议的基线

不建议把 W3 主线回退到早期 x86 字符设备实验。原因是：

- x86 主线和 arm64 主线会造成两套口径
- W3 的 tracing / perf 目标和 Day13 当前成果脱节
- 后面数据表和回归脚本都会变复杂

### 建议统一口径

W3 主线建议统一为：

- 架构：`arm64`
- 机器：`QEMU virt`
- 启动：`Image + initramfs(rootfs.img)`
- rootfs：先沿用 BusyBox 模型
- 验证模块：`demo_regmap.ko`
- 调试能力：`debugfs + ftrace + function_graph + perf basic`

---

## 4. D15-D21 任务的主线逻辑

这 7 天不是彼此独立的小任务，而是一条连续的工程化收敛链：

1. D15 先冻结 baseline
2. D16 做第一轮粗裁
3. D17 确定 rootfs 路线并补工具
4. D18 做第二轮分类裁剪
5. D19 输出量化对比
6. D20 把回归自动化
7. D21 把数据和结论写成可讲、可交付的报告

可以理解为：

> 先定标准，再做变化；先能测，再优化；先能回归，再给结论。

---

## 5. D15 分析：选择 baseline defconfig；记录现状

### 5.1 D15 的核心任务

D15 不是“优化日”，而是“立标准日”。

这一天要解决的问题是：

- 后续所有对比，究竟基于哪个 `.config`
- 后续所有采样，究竟用哪个启动场景
- 后续所有报告，究竟对比哪些指标

### 5.2 D15 需要冻结的内容

建议冻结以下项目：

- 内核版本
- 交叉编译工具链版本
- 基线 `.config`
- `Image` 产物
- `rootfs.img` 产物
- QEMU 启动命令
- guest `/init` 流程
- 验证模块（建议固定为 `demo_regmap.ko`）

### 5.3 baseline 数据表建议字段

建议至少采集以下字段：

| 类别 | 字段 | 说明 |
|---|---|---|
| 内核 | `Image` 大小 | 裁剪前后最直观指标 |
| rootfs | `rootfs.img` 大小 | rootfs 选型与工具集成本 |
| 启动 | boot time | 从 QEMU 启动到 shell prompt 出现 |
| 内存 | `MemTotal` / `MemFree` | 基础内存视角 |
| 内存 | `Slab` / `SReclaimable` / `SUnreclaim` | 观察内核态开销 |
| 模块 | 构建产物模块数 | 反映内核裁剪影响范围 |
| 模块 | guest 实际加载模块数 | 反映运行态验证口径 |
| 追踪 | `function_graph` 是否可用 | 确保 Day13 能力未丢 |
| 性能 | `perf` 是否可用 | 为后续 D17 / D19 做准备 |

### 5.4 D15 难点

D15 最大的风险不是“不会采集”，而是“口径不统一”。

例如以下变化都会导致数据不可比：

- QEMU 参数变了
- rootfs 内容变了
- `/init` 行为变了
- 验证模块变了
- memory size 变了
- trace/debug 配置变了

所以 D15 一定要先把“实验场景”冻结，之后的 D16-D19 才有意义。

### 5.5 D15 产出建议

建议产出：

- `day15/README.md`
- `day15/baseline_collect.sh`
- `day15/baseline.md` 或 `baseline.csv`

---

## 6. D16 分析：第一轮裁剪

### 6.1 D16 的目标

D16 的目标不是立刻做到“最小”，而是先做**第一轮粗裁**：

- 去掉当前实验明显无关的驱动
- 去掉当前实验明显无关的子系统
- 但保留 tracing / perf / debug 相关能力

### 6.2 建议优先考虑去掉的部分

在 arm64 + QEMU virt + 当前 demo 场景下，可以优先审视：

- 与当前平台无关的大量设备驱动
- 不使用的文件系统
- 不使用的网络协议族
- 不使用的输入/显示/声音相关子系统
- 不使用的无线、蓝牙、媒体、USB 相关项
- 与当前实验完全无关的平台支持项

### 6.3 D16 必须保留的内容

#### 必须项

- initramfs / init 支持
- ELF 执行支持
- proc / sysfs / tmpfs / devtmpfs
- printk / 串口输出
- module 支持

#### 平台项

- arm64 基础架构支持
- OF / DT 支持
- irqdomain / 中断控制器支持
- QEMU virt 所需串口与中断能力
- platform bus / of_match 路线
- regmap 相关基础能力

#### 调试项

- `DEBUG_FS`
- `FTRACE`
- `FUNCTION_TRACER`
- `FUNCTION_GRAPH_TRACER`
- `TRACEPOINTS`
- `KALLSYMS`
- `FRAME_POINTER`（建议保留）

#### 性能项

- `PERF_EVENTS`
- software events
- 基础 scheduler 可观测性

### 6.4 D16 的验收方式

D16 的验收不应该只看“镜像变小了没有”，而应该优先确认：

- 系统仍能启动到 shell
- `demo_regmap.ko` 仍能加载
- `debugfs` 能挂载
- `available_tracers` 里仍然有 `function_graph`
- Day13 的 trace 脚本仍能跑通

### 6.5 D16 常见风险

第一轮裁剪最常见的问题是把调试能力一并裁掉，例如：

- `DEBUG_FS`
- `KALLSYMS`
- `FUNCTION_GRAPH_TRACER`
- `PERF_EVENTS`

这样虽然镜像可能变小，但会直接破坏 W3 的目标。

所以 D16 的原则应该是：

> 先删与当前实验完全无关的内容，绝不先删可观测性。

---

## 7. D17 分析：rootfs 方案选择

### 7.1 D17 的关键决策

D17 要解决的是：

- 继续沿用 BusyBox 最小 rootfs
- 还是切换到 Buildroot 管理 rootfs

### 7.2 当前阶段的推荐结论

结合当前仓库状态，建议 **本周主线先选 BusyBox**，不要直接把 Buildroot 作为主线切换。

### 7.3 推荐 BusyBox 的原因

当前仓库已经建立了以下路径：

- 构造 rootfs 目录
- 拷贝 BusyBox
- 写 `/init`
- 打包 `rootfs.img`
- QEMU 直接使用 `-initrd rootfs.img`

这条链已经稳定，适合作为 W3 的“对比基线”。

如果 D17 直接切 Buildroot，会同时引入很多新变量：

- rootfs 目录结构变化
- init 流程变化
- 工具与库依赖变化
- 构建流程变化
- 启动链验证点变化

这样会让“内核裁剪问题”和“rootfs 切换问题”混在一起，不利于本周收敛。

### 7.4 D17 真正的难点

当前 guest 里，`ftrace` 基本已经具备；D17 真正的新难点其实是：

> **如何把 `perf` 用户态工具纳入 rootfs。**

BusyBox 本身不提供 `perf`，因此需要额外处理：

- 在宿主机构建 `perf` 用户态程序
- 确认 arm64 目标环境可运行
- 处理依赖项或静态化方案
- 把 `perf` 放入 rootfs

### 7.5 D17 推荐路线

建议优先采用：

- BusyBox 作为主线 rootfs
- 额外补充必要工具与 `perf` 用户态二进制

### 7.6 D17 必要工具建议

除了现有 BusyBox applet，建议至少补足：

- `sh`
- `ls`
- `cat`
- `echo`
- `mount`
- `dmesg`
- `grep`
- `ps`
- `date`
- `sleep`
- `find`
- `cut`
- `awk`
- `sed`
- `cp`
- `mv`
- `rm`
- `uname`
- `hexdump` 或 `od`
- `perf`（外部加入）

### 7.7 D17 验收建议

建议把“能跑 perf/ftrace 基本命令”定义为：

- `cat /sys/kernel/debug/tracing/available_tracers`
- `echo function_graph > current_tracer`
- Day13 trace 脚本可运行
- `perf --version`
- `perf list`
- 至少能运行一个基础命令，例如：
  - `perf stat true`
  - 或 `perf stat -e task-clock sleep 1`

---

## 8. D18 分析：第二轮裁剪

### 8.1 D18 和 D16 的区别

- D16：是“去明显无关项”的**粗裁**
- D18：是按类别整理的**可解释裁剪**

D18 不只是继续改 `.config`，更重要的是把内核选项分成几个角色桶，让裁剪逻辑可以讲清楚。

### 8.2 建议的四类归档方式

#### 1. 必须项

系统启动与基本运行不可缺少，例如：

- initramfs / init
- devtmpfs
- proc / sysfs / tmpfs
- module
- ELF
- printk

#### 2. 平台项

arm64 + QEMU virt + DT + IRQ 所需，例如：

- arm64 架构项
- OF / DT
- irqdomain
- GIC
- PL011
- platform bus
- regmap 基础依赖

#### 3. 调试项

为可观测性服务，例如：

- `DEBUG_FS`
- `FTRACE`
- `FUNCTION_GRAPH_TRACER`
- `TRACEPOINTS`
- `KALLSYMS`
- `FRAME_POINTER`
- `IKCONFIG`（建议保留）

#### 4. 性能项

为 perf / profile 服务，例如：

- `PERF_EVENTS`
- software events
- 硬件 PMU 相关项（按平台情况决定）
- 需要时再考虑 kprobe / uprobe

### 8.3 D18 的价值

D18 的价值不是“再缩一点点”，而是：

- 把裁剪逻辑从“经验删项”升级成“分类管理”
- 为 D19 的风险说明做铺垫
- 为 D21 的报告与回滚方案做准备

### 8.4 D18 建议产出

建议至少同时保留：

- `.config`
- `savedefconfig`
- `trim2.fragment`
- 分类说明表（必须项 / 平台项 / 调试项 / 性能项）

---

## 9. D19 分析：输出对比数据与风险项

### 9.1 D19 的任务本质

D19 是把 D15、D16、D18 三个阶段的数据串起来，形成对比视图。

最重要的前提是：

> 所有数据必须在相同口径下采集。

### 9.2 建议至少对比三组

- baseline（D15）
- trim1（D16）
- trim2（D18）

### 9.3 建议对比表字段

- config 名称
- `Image` 大小
- `rootfs.img` 大小
- boot time
- `MemTotal`
- `MemFree`
- `Slab`
- 构建模块数
- guest 实际加载模块数
- `function_graph` 是否可用
- `perf` 是否可用
- 风险备注

### 9.4 风险项建议从这里开始沉淀

建议把风险单独列出来，例如：

- 某些选项裁掉后，后续扩展到 PCIe / virtio 可能需要补回
- 在 QEMU virt 环境中，`perf` 可能更依赖 software event，而不是完整 PMU 能力
- 为了保留 tracing / perf，镜像不一定能裁到极限
- BusyBox 路线短期最稳，但后续 rootfs 扩展性不如 Buildroot

### 9.5 D19 的价值

D19 不只是填表，而是让后面的报告不再停留在“感觉变小了”，而是有量化依据。

---

## 10. D20 分析：回归清单与自动化脚本

### 10.1 D20 的意义

如果没有 D20，那么 D16 / D18 的裁剪成果只是“一次性成功”。

有了 D20，才能把它升级成：

- 后续还能重复验证
- 改完 `.config` 能快速回归
- 出问题时知道是哪一类环节失败

### 10.2 推荐两层脚本结构

#### 宿主机侧脚本

负责：

- 启动 QEMU
- 捕获串口输出
- 等待 shell prompt
- 推送或触发 guest 命令
- 收集日志
- 判定 pass / fail

#### guest 侧脚本

负责：

- 挂载 `/proc` `/sys` `/dev` / `debugfs`
- `insmod` 模块
- 读写 debugfs 节点
- 运行 trace / perf 命令
- 做简单压力与错误检查

### 10.3 D20 建议回归项

#### 启动类

- 启动到 shell prompt
- `/proc`、`/sys`、`/dev` 挂载成功
- `debugfs` 挂载成功

#### 驱动 demo 类

- `insmod /demo_regmap.ko`
- snapshot 节点可读
- trigger 节点可写
- 触发前后 snapshot 有变化
- `rmmod` 正常

#### tracing / perf 类

- `available_tracers` 中存在 `function_graph`
- Day13 trace 脚本成功
- `perf list` 成功
- `perf stat true` 成功

#### 压力类

- 模块多次装卸
- 连续触发 trigger
- 检查 dmesg 无 `Oops` / `BUG` / `Call trace`

### 10.4 D20 与 W1 的关系

W1 的 Day06 已经有回归思路，例如：

- 装卸回归
- 压力脚本
- dmesg 扫描

D20 完全可以沿用这种方法，只是把对象切换到 arm64 + Day13 demo。

---

## 11. D21 分析：形成 1-2 页总结报告

### 11.1 D21 的目标

D21 不是去做新的实验，而是把前面六天的结果压缩成一份：

- 可以投递
- 可以讲解
- 可以回顾
- 可以给后续自己复用

的短报告。

### 11.2 推荐报告结构

#### 1. 背景

说明为什么做 W3：

- 当前实验环境可运行
- 但镜像、rootfs、调试能力、回归方法还未收敛
- 目标是在保留 tracing / perf 的前提下完成最小化

#### 2. 方法

- 选择 baseline
- 第一轮粗裁
- 选择 rootfs 路线
- 第二轮分类裁剪
- 自动回归验证

#### 3. 数据

给出 baseline / trim1 / trim2 的核心对比表。

#### 4. 结论

例如：

- 镜像下降多少
- 启动时间变化多少
- 内存变化多少
- tracing / perf 是否仍保留
- 当前最推荐的配置组合是什么

#### 5. 回滚方案

这是非常重要的一部分。

建议写清楚：

- 保留 baseline `.config`
- 每轮裁剪单独 commit / tag
- rootfs 保留 BusyBox 基线版本
- 遇到异常先回退到 baseline 的 config + rootfs + QEMU 启动参数
- 不要在 perf 失败时同时修改内核和 rootfs

---

## 12. 这 7 天之间的依赖关系

### D15 是根

没有 D15，后面的数据都不可信。

### D16 / D18 是两轮不同性质的裁剪

- D16：面向“当前能不能先跑”
- D18：面向“这套配置为什么这样保留”

### D17 是 rootfs 方案的分界点

rootfs 一旦改变，后面的 size / boot / mem / perf 数据都会受到影响，所以 D17 不能拖太晚。

### D20 是保险丝

没有自动回归，前面的裁剪都只是一次性验证。

### D21 是压缩交付

不是再新增一套内容，而是把前面的数据和结论组织成文档。

---

## 13. 当前阶段的推荐路线

结合当前代码，推荐本周按以下路线推进：

### 主线选择

- 主线架构：`arm64`
- 主线实验：`day13`
- 主线 rootfs：`BusyBox`
- 核心验证模块：`demo_regmap.ko`
- 保留能力：`debugfs + ftrace + function_graph + perf basic`

### 当前不建议做的事

- 不要在本周主线同时维护 x86 和 arm64 两套口径
- 不要 D17 就直接把 Buildroot 切成唯一主线
- 不要在 D16 第一轮就把 tracing / perf / debugfs 一起砍掉
- 不要等到 D19 才开始想怎么统一采样口径
- 不要等到 D20 才补脚本基础设施

---

## 14. 本周最可能卡住的点

### 14.1 `perf` 用户态工具进入 rootfs

这是 D17 最值得提前准备的点。`ftrace` 当前大概率已具备，但 `perf` 用户态工具、依赖项和目标架构可运行性，需要单独设计。

### 14.2 baseline 口径不统一

如果启动参数、rootfs 内容、模块版本、采样脚本在中途不断变化，那么 D19 的数据就失去比较意义。

### 14.3 误删可观测性

包括但不限于：

- `DEBUG_FS`
- `FTRACE`
- `FUNCTION_GRAPH_TRACER`
- `KALLSYMS`
- `PERF_EVENTS`

### 14.4 没有自动回归

如果每次修改 `.config` 都只能手工进 guest 验证，那么后续效率会很低，也不容易稳定复现问题。

---

## 15. 小结

W3 不是单纯的“把内核裁小一点”，而是一次很重要的工程化整理：

- 把当前实验环境从“能演示”推进到“能对比”
- 把当前实验环境从“能手工跑”推进到“能自动回归”
- 把当前实验环境从“经验型配置”推进到“可解释的配置分类”

这一周完成后，仓库的价值会明显提升：

- 对自己：后续做 PCIe / perf / ftrace / 调优时，有稳定基线
- 对面试：可以讲“内核裁剪、启动链、rootfs、回归自动化”的完整闭环
- 对项目复用：后续换平台或换 demo 时，有固定流程可迁移

---

## 16. 下一步建议

建议下一步直接进入 D15 设计，先把以下内容固定下来：

1. baseline 数据表字段
2. 每个字段的采样方法
3. 宿主机侧 / guest 侧采样脚本职责边界
4. baseline `.config`、`Image`、`rootfs.img`、QEMU 参数的归档方式

这样后面的 D16-D21 才能在同一个实验口径上推进。
