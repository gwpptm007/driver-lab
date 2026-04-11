# Day15 baseline 冻结深度指南 - W3 基准线建立

## 一、Day15 是什么？

Day15 是 W3（内核裁剪与移植）的第一天，也是整个 W3 的**起点**。

**核心目标**：把 W3 后续所有对比要用到的 baseline 场景和采样口径固定下来。

Day15 不做裁剪、不做新功能。它的重点是：
1. **baseline 冻结**：arm64 + QEMU virt + BusyBox + demo_regmap 链路
2. **采样口径固定**：定义采哪些字段、怎么采、结果落在哪里
3. **可重复验证**：建立独立目录、可自动汇总、可演进到 Day16

---

## 二、W3 学习路径中的位置

### 2.1 W3 整体架构

```
W3 (内核裁剪与移植 - day15-21)
├── day15: baseline 冻结   ← 今天
├── day16: 第一轮粗裁
├── day17: perf 集成 + 第二轮裁剪
├── day18: 分类裁剪
├── day19: 量化对比报告
├── day20: 自动回归套件
└── day21: 最终总结报告

W4 (PCIe 基础 - day22-28)
W5 (DMA + 性能 - day29-35)
```

### 2.2 Day15 的关键词

```
Day15 的核心不是"把系统做小"，而是先回答：
  1. 当前用哪套场景做基线？
  2. 采哪些字段？
  3. 怎么采才可比？
  4. 结果落在哪里？

关键词：baseline / 冻结 / 采样 / 记录 / 可复现
```

### 2.3 Day15 与前后天的关系

```
Day13 vs Day15：
  - Day13：已有驱动 + ftrace 路径观察
  - Day15：baseline 场景冻结 + 数据采样
  - 两个主题共享教学驱动思路，但不共享默认构建入口

Day15 vs Day16：
  - Day15：建立基准（Image=38867 KiB，boot=2008ms）
  - Day16：在 baseline 基础上做第一轮粗裁
```

---

## 三、baseline 场景定义

### 3.1 统一基线场景

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day15 统一基线场景                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  架构：arm64                                                        │
│  虚拟平台：QEMU virt                                               │
│  内核产物：Image                                                    │
│  rootfs：BusyBox + initramfs (rootfs.img)                          │
│  验证模块：demo_regmap.ko                                          │
│  调试能力：debugfs + tracing/function_graph                         │
│  perf：先记录"有没有"，D17 再补用户态二进制                        │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.2 为什么选择 demo_regmap.ko 作为验证模块？

```
demo_regmap.ko 已具备：
  - /sys/kernel/debug/demo_regmap/snapshot（可读）
  - /sys/kernel/debug/demo_regmap/trigger（可写）

非常适合做 baseline 的最小冒烟动作：
  1. insmod demo_regmap.ko
  2. cat snapshot
  3. echo trigger > trigger
  4. rmmod demo_regmap

这些动作可以快速验证内核主链路是否完好
```

### 3.3 trace_baseline.fragment 配置

```bash
# Day15 trace_baseline.fragment

# debug / symbol（调试信息）
CONFIG_DEBUG_FS=y              # debugfs 观测入口
CONFIG_KALLSYMS=y              # 符号表
CONFIG_KALLSYMS_ALL=y          # 所有符号

# tracing / ftrace / function_graph（核心链路）
CONFIG_TRACEPOINTS=y           # tracepoints 基础设施
CONFIG_TRACING=y               # tracing 主开关
CONFIG_FTRACE=y                # ftrace 主功能
CONFIG_FUNCTION_TRACER=y       # function tracer
CONFIG_FUNCTION_GRAPH_TRACER=y # function_graph tracer（Day15 关键）
CONFIG_DYNAMIC_FTRACE=y        # 动态 ftrace
CONFIG_TRACEFS_FS=y            # tracefs 文件系统
CONFIG_IRQSOFF_TRACER=y        # IRQ off tracer
CONFIG_SCHED_TRACER=y          # 调度 tracer

# perf（内核侧保留）
CONFIG_PERF_EVENTS=y           # perf 事件框架
CONFIG_HW_PERF_EVENTS=y        # 硬件 perf 事件
```

---

## 四、baseline 核心数据

### 4.1 关键指标

```
| 项目 | 数值 | 说明 |
|---|---:|---|
| Image 大小 | 38,867 KiB | 启动镜像体积（用于 D19 对比）|
| rootfs 大小 | 1,181 KiB | BusyBox initramfs（用于 D17/D19 对比）|
| boot_ms | 2008 | QEMU 启动到 first shell prompt |
| MemTotal | 997,944 KiB | guest 总内存视角 |
| MemFree | 968,564 KiB | guest 空闲内存 |
| Slab | 12,252 KiB | 内核 slab 开销 |
| modules_loaded_count | 1 | demo_regmap.ko 已加载 |
| tracing_ok | yes | tracing 目录已可用 |
| function_graph_ok | yes | function_graph tracer 已可用 |
| perf_bin_ok | no | rootfs 暂未内置 perf（D17 再补）|
```

### 4.2 available_tracers 结果

```
function_graph wakeup_dl wakeup_rt wakeup irqsoff function nop
```

### 4.3 功能验收状态

```
debugfs_ok=yes
tracing_ok=yes
function_graph_ok=yes
trace_smoke_ok=yes
insmod_ok=yes
snapshot_ok=yes
trigger_ok=yes
dmesg_warn=no
```

---

## 五、脚本架构

### 5.1 三层脚本分工

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day15 脚本三层架构                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  build.sh（宿主机）                                                 │
│  ─────────────────                                                  │
│  1. 编译 demo_regmap.ko                                           │
│  2. 构造 Day15 自己的最小 rootfs                                   │
│  3. 把 guest_collect.sh 打进 rootfs                                │
│  4. 导出并注入 QEMU virt DTB                                       │
│  5. 生成 virt-day15.dtb                                           │
│  6. 直接启动 QEMU                                                  │
│                                                                      │
│  collect/guest_collect.sh（guest 内）                               │
│  ─────────────────────────────────                                  │
│  1. 挂载 proc/sys/dev/debugfs/tracefs                             │
│  2. 保存 meminfo/modules/mount/filesystems/dmesg                  │
│  3. 自动探测 tracing 路径                                          │
│  4. 检查 function_graph                                            │
│  5. insmod demo_regmap.ko                                         │
│  6. 读取 snapshot，执行 trigger                                     │
│  7. 生成 /tmp/day15-baseline/metrics.env                          │
│                                                                      │
│  collect/host_collect.sh（宿主机）                                  │
│  ─────────────────────────────────                                  │
│  1. 统计 Image/rootfs.img/.ko 大小                                 │
│  2. 启动 QEMU 并记录 serial.log                                    │
│  3. 计算 boot_ms                                                   │
│  4. 通过串口自动调用 guest_collect.sh                               │
│  5. 生成 records/.../baseline.csv                                  │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.2 推荐执行顺序

```
第一次执行（手工验证优先）：

Step 1：应用配置并编译
  cd day15
  ./apply_config.sh

Step 2：构建并启动 QEMU
  ./build.sh

Step 3：guest 手工验证
  /bin/day15_guest_collect.sh
  cat /tmp/day15-baseline/metrics.env

Step 4：退出 guest
  poweroff -f

Step 5：宿主机自动采集
  cd day15/collect
  ./host_collect.sh

Step 6：查看结果
  day15/records/<timestamp>-day15-baseline-arm64-virt/
```

### 5.3 为什么不能直接跑 host_collect.sh？

```
直接跑 host_collect.sh 的问题：
  - 如果失败，不容易判断问题在哪
  - 内核 config 没补好？
  - 内核没重编成功？
  - rootfs 里没带进 guest 采样脚本？
  - QEMU 没正确启动？

正确姿势：
  先手工 guest 验证，再上自动化
```

---

## 六、tracing 路径处理

### 6.1 为什么 tracing 路径需要专门处理？

```
典型问题：
  debugfs_ok=yes
  tracing_ok=no
  function_graph_ok=no

这不一定是"内核没有 tracing"，而是：
  - 脚本只检查了 /sys/kernel/debug/tracing
  - 但系统实际把 tracing 挂在 /sys/kernel/tracing
```

### 6.2 guest_collect.sh 的自动探测逻辑

```bash
# 1. 先尝试挂载 debugfs
mount -t debugfs debugfs /sys/kernel/debug

# 2. 再尝试挂载 tracefs
mount -t tracefs tracefs /sys/kernel/debug/tracing

# 3. 自动优先检查：
#    - /sys/kernel/debug/tracing
#    - /sys/kernel/tracing
```

### 6.3 available_tracers 的含义

```
available_tracers 包含：
  - function_graph：函数调用图（核心）
  - function：函数调用
  - nop：无操作（默认）
  - wakeup_rt / wakeup_dl / wakeup：调度延迟
  - irqsoff：IRQ 关闭时间

function_graph 是 Day15 最重要的 tracer
```

---

## 七、baseline_template.csv 字段体系

### 7.1 字段分组

```
元信息：
  scenario_id, kernel_ver, arch, machine, rootfs_type, demo_module, collector_ver

构建产物信息：
  image_bytes, image_kib, rootfs_bytes, rootfs_kib, modules_built_count

启动时间：
  boot_start_event, boot_end_event, boot_ms, boot_note

guest 内存：
  memtotal_kib, memfree_kib, memavailable_kib, slab_kib,
  sreclaimable_kib, sunreclaim_kib, kernelstack_kib, pagetables_kib

可观测性/功能状态：
  modules_loaded_count, debugfs_ok, tracing_ok, function_graph_ok,
  trace_smoke_ok, perf_bin_ok, perf_smoke_ok, boot_ok,
  insmod_ok, snapshot_ok, trigger_ok, dmesg_warn, remarks
```

### 7.2 关键字段说明

```bash
# boot_ms：QEMU 进程启动到 first shell prompt 的时间
# 这是 D19 对比报告中最核心的时间指标

# perf_bin_ok：perf 用户态程序是否存在
# Day15 = no，因为 perf 工具 D17 才集成进 rootfs

# function_graph_ok：function_graph tracer 是否可用
# 这是 Day15 可观测性的核心指标
```

---

## 八、与 Day13 的关系

### 8.1 为什么把 Day15 和 Day13 分离？

```
之前的问题：
  Day15 的 rootfs、DTB、启动路径默认复用 day13
  一旦 Day15 结果异常，不容易判断是：
    - Day15 采样脚本的问题
    - 还是 day13 构建/脚本带来的问题

分离后的优势：
  - demo_regmap.ko 由 day15/Makefile 自己编译
  - rootfs.img 由 day15/build.sh 自己生成
  - virt-day15.dtb 由 day15/build.sh 自己生成
  - baseline 采样由 day15/collect/*.sh 自己完成

Day13 和 Day15 共享教学驱动思路，但不共享默认构建入口
```

### 8.2 Day13 vs Day15 的定位

```
Day13：
  主题：已有驱动 + ftrace 路径观察
  目标：理解 ftrace 和 IRQ handler 调用链

Day15：
  主题：baseline 场景冻结 + 数据采样
  目标：建立 W3 后续所有对比的基准线
```

---

## 九、面试要会讲的五句话

1. **"Day15 的目标不是做裁剪，而是先把 W3 后续所有对比要用到的 baseline 场景和采样口径固定下来"**
   → 理解 Day15 的定位

2. **"baseline 场景是 arm64 + QEMU virt + BusyBox initramfs + demo_regmap，Image=38867 KiB，boot=2008ms"**
   → 理解 Day15 的基线数据

3. **"trace_baseline.fragment 配置了 CONFIG_FUNCTION_GRAPH_TRACER=y，这是 Day15 可观测性的核心"**
   → 理解 trace_baseline 的作用

4. **"tracing 路径可能挂在 /sys/kernel/debug/tracing 或 /sys/kernel/tracing，guest_collect.sh 会自动探测"**
   → 理解 tracing 路径处理

5. **"Day15 和 Day13 共享 demo_regmap 驱动思路，但不共享默认构建入口，Day15 是独立自包含的实验目录"**
   → 理解 Day15 和 Day13 的边界

---

## 十、验收标准

### 10.1 手工 guest 验收

```
/tmp/day15-baseline/metrics.env 中至少满足：
  boot_ok=yes
  debugfs_ok=yes
  insmod_ok=yes
  snapshot_ok=yes
  trigger_ok=yes

如果再满足：
  tracing_ok=yes
  function_graph_ok=yes

那 Day15 baseline 的可观测性就更完整
```

### 10.2 自动采集验收

```
day15/records/.../ 目录下至少有：
  serial.log
  metrics.env
  baseline.csv
  meminfo.txt
  modules.txt
  available_tracers.txt
```

### 10.3 功能状态验收

```
perf_bin_ok = no（正常，perf D17 再补）
dmesg_warn = no（内核无异常）
function_graph 在 available_tracers 中
```

---

## 附录：Day15 在 W3 中的角色

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day15 在 W3 中的角色                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Day15 = 基准线（所有对比的起点）                                    │
│                                                                      │
│  Day16：用 baseline 对比第一轮粗裁                                   │
│  Day17：在 baseline 基础上集成 perf                                │
│  Day18：用 baseline 对比分类裁剪                                    │
│  Day19：用 baseline 数据做跨阶段量化对比                            │
│  Day20：用 baseline 做自动回归验证                                  │
│  Day21：用 baseline 数据做最终总结                                  │
│                                                                      │
│  没有 Day15，后面的所有对比都无从谈起                                │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```
