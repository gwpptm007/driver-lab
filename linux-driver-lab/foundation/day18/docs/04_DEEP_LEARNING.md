# Day18 分类裁剪深度指南 - W3 第二轮精细裁剪

## 一、Day18 是什么？

Day18 是 W3（内核裁剪与移植）的倒数第四天，定位是**分类裁剪**。

**核心目标**：把第二轮裁剪的配置按角色分成五类（required/platform/debug/perf/trim），让"为什么保留、为什么删除"清晰可讲。

Day18 不做无脑裁剪。它的重点是：
1. **分类表达**：把配置按角色分类，每类都有明确理由
2. **等价性验证**：证明 classified 和 round2b_legacy 结果相同
3. **可解释性**：让配置选择具备"能讲清楚"的结构

---

## 二、W3 学习路径中的位置

### 2.1 W3 整体架构

```
W3 (内核裁剪与移植 - day15-21)
├── day15: baseline 冻结
├── day16: 第一轮粗裁
├── day17: perf 工具集成
├── day18: 分类裁剪     ← 今天
├── day19: 量化对比报告
├── day20: 自动回归套件
└── day21: 最终总结报告
```

### 2.2 Day18 与前后天的关系

```
Day16 vs Day18：
  - Day16：粗裁（去明显无关项，经验层面）
  - Day18：分类裁剪（分类管理、可解释、可回滚）

Day17 vs Day18：
  - Day17：perf 工具集成（加东西）
  - Day18：分类裁剪（减东西，但有分类逻辑）

Day18 vs Day19：
  - Day18：证明"分类表达方式成立"
  - Day19：输出"跨阶段量化对比"
```

---

## 三、三种 Profile 设计

### 3.1 为什么需要三种 Profile？

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day18 三种 Profile 架构                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  baseline（基线）                                                    │
│  ─────────────                                                      │
│  用途：所有 compare 的基线                                          │
│  提供：tracing / perf / demo_regmap 的最低可用环境                   │
│                                                                      │
│  round2b_legacy（legacy 对照）                                       │
│  ────────────────────────────                                        │
│  用途：保留 day17 的连续性，给 classified 提供对照                    │
│  fragment 链：trace_baseline → trim_round1 → trim_round2b           │
│                                                                      │
│  classified（分类表达）                                              │
│  ─────────────────────                                              │
│  用途：用分类方式表达第二轮裁剪，让"为什么保留/删除"清晰可讲          │
│  fragment 链：trace_baseline → 10_req → 20_plat → 30_debug →       │
│                40_perf → 90_trim                                    │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.2 为什么保留 legacy 对照？

```
round2b_legacy 和 classified 的关系：
  - 两者都是"第二轮裁剪"
  - round2b_legacy：沿用 day17 的连续写法
  - classified：用分类方式重新组织

关键验证：
  - 两者 .config sha256 是否相同
  - 如果相同 → 分类表达不是"换写法"，而是"结果不变但组织更清晰"
  - 如果不同 → 需要解释差异来自哪里
```

---

## 四、分类体系

### 4.1 五类分类矩阵

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day18 分类矩阵                                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  required（系统必须项）：                                            │
│  ─────────────────────────                                          │
│  CONFIG_BINFMT_ELF        → 用户态 ELF 依赖                         │
│  CONFIG_BLK_DEV_INITRD    → initramfs 启动依赖                     │
│  CONFIG_DEVTMPFS          → /dev 自动节点依赖                      │
│  CONFIG_DEVTMPFS_MOUNT    → guest 启动时自动挂载                    │
│  CONFIG_PROC_FS           → 采样脚本依赖 /proc                     │
│  CONFIG_SYSFS             → 设备模型依赖                            │
│  CONFIG_TMPFS             → /tmp 临时文件依赖                       │
│  CONFIG_MODULES           → insmod demo_regmap.ko 依赖             │
│                                                                      │
│  platform（平台项）：                                                │
│  ───────────────────                                                │
│  CONFIG_OF                → QEMU virt + DT 注入主链依赖             │
│  CONFIG_OF_IRQ            → 从 DT 解析 irq 依赖                    │
│  CONFIG_SERIAL_AMBA_PL011 → QEMU virt 串口控制台依赖               │
│  CONFIG_ARM_GIC / GIC_V3  → virt 平台中断控制器依赖                │
│  CONFIG_IRQ_DOMAIN         → IRQ domain 主链依赖                    │
│  CONFIG_REGMAP / MMIO     → demo_regmap 主链依赖                   │
│                                                                      │
│  debug（调试项）：                                                   │
│  ─────────────────                                                  │
│  CONFIG_DEBUG_FS           → debugfs 观测入口                      │
│  CONFIG_TRACEPOINTS        → tracing 基础能力                      │
│  CONFIG_TRACING / FTRACE   → ftrace 主开关                        │
│  CONFIG_FUNCTION_GRAPH_TRACER → day13/day17 IRQ 路径跟踪           │
│  CONFIG_FRAME_POINTER      → 回溯可读性                            │
│  CONFIG_IKCONFIG           → 配置可追溯性                          │
│                                                                      │
│  perf（性能分析项）：                                                │
│  ──────────────────                                                 │
│  CONFIG_PERF_EVENTS       → perf stat/list 基础能力               │
│  CONFIG_HW_PERF_EVENTS     → perf 事件框架能力                     │
│                                                                      │
│  trim（裁剪项）：                                                    │
│  ──────────────                                                     │
│  CONFIG_PCI   → n（当前实验路径不依赖 PCI）                         │
│  CONFIG_SCSI  → n（initramfs 启动不依赖 SCSI 磁盘）                │
│  CONFIG_NET   → n（串口采样链与 demo_regmap 不依赖网络）            │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.2 required 类详解

```bash
# CONFIG_BINFMT_ELF
why: 用户态 /init 与 BusyBox 可执行文件依赖 ELF 格式
trim risk: 删掉后 BusyBox 无法执行，系统无法启动

# CONFIG_MODULES
why: day12/day17 的 demo_regmap 是模块化驱动
trim risk: 删掉后无法 insmod，demo 功能链断裂

# CONFIG_PROC_FS
why: 采样脚本依赖 /proc/meminfo、/proc/modules 等
trim risk: 删掉后采样脚本失效，evidence 链断裂
```

### 4.3 platform 类详解

```bash
# CONFIG_OF（Device Tree）
why: QEMU virt + DT 注入主链依赖
trim risk: 删掉后 DT 注入失效，virt 平台设备无法枚举

# CONFIG_SERIAL_AMBA_PL011
why: console=ttyAMA0 依赖，QEMU virt 串口控制台
trim risk: 删掉后无串口输出，无法看到 boot log

# CONFIG_ARM_GIC_V3
why: arm64 virt 常用 GICv3，中断控制器
trim risk: 删掉后 IRQ 无法路由，platform 设备失效

# CONFIG_REGMAP
why: demo_regmap 使用 regmap 框架访问寄存器
trim risk: 删掉后 demo_regmap 的 regmap_* 调用全部失效
```

### 4.4 debug 类详解

```bash
# CONFIG_FUNCTION_GRAPH_TRACER
why: day13/day17 IRQ 路径跟踪依赖
trim risk: 删掉后无法用 function_graph 追踪 IRQ handler 调用链

# CONFIG_DEBUG_FS
why: debugfs 是内核观测入口（tracefs 依赖它）
trim risk: 删掉后 /sys/kernel/debug 失效，tracing 目录不可见

# CONFIG_IKCONFIG_PROC
why: /proc/config.gz 提供取证方便
trim risk: 不影响功能，但失去"随时查看当前配置"的能力
```

### 4.5 perf 类详解

```bash
# CONFIG_PERF_EVENTS
why: perf stat/list 基础能力
trim risk: 删掉后 perf 命令不可用，perf_ftrace 也失效

# CONFIG_HW_PERF_EVENTS
why: perf 事件框架能力
trim risk: 删掉后 perf 无法采集硬件事件（如 cpu-cycles）
```

### 4.6 trim 类详解

```bash
# CONFIG_PCI → n
why: 当前实验路径（virt + initramfs + demo_regmap）不依赖 PCI
proof: lspci 在 guest 里没有有用输出

# CONFIG_SCSI → n
why: initramfs 启动不依赖 SCSI 磁盘，没有 SCSI 设备
proof: /proc/scsi/scsi 不存在

# CONFIG_NET → n
why: 串口采样链（serial log）和 demo_regmap（MMIO）都不需要网络
proof: ifconfig 看不到 eth0，网络栈对实验无直接价值
```

---

## 五、fragment 架构

### 5.1 fragment 链对比

```
round2b_legacy 的 fragment 链：
  trace_baseline.fragment   → 保留 tracing 基础
  trim_round1.fragment       → 第一轮裁剪（来自 day16）
  trim_round2b.fragment       → 第二轮裁剪（粗粒度）

classified 的 fragment 链：
  trace_baseline.fragment     → 保留 tracing 基础
  10_required.fragment       → 系统必须项
  20_platform.fragment       → 平台相关项
  30_debug.fragment          → 调试观测项
  40_perf.fragment           → 性能分析项
  90_trim_day18.fragment     → 裁剪项（PCI/SCSI/NET=n）
```

### 5.2 fragment 设计原则

```
fragment 的设计原则：
  1. 每个 fragment 独立管理一个配置类别
  2. fragment 之间不重叠
  3. fragment 按依赖顺序排列（trace_baseline 最先）
  4. 分类 fragment 便于后续"只改某一类配置"
```

### 5.3 apply_config.sh 脚本

```bash
# 基本用法
PROFILE=classified ./apply_config.sh   # 应用 classified 配置
PROFILE=round2b_legacy ./apply_config.sh  # 应用 legacy 配置

# 只生成配置，不编译
PROFILE=classified BUILD_KERNEL=no ./apply_config.sh

# 脚本内部逻辑
  1. 读取 fragment 链顺序
  2. 对每个 fragment 执行 apply
  3. olddefconfig 自动接受默认值
  4. savedefconfig 保存最小配置
```

---

## 六、等价性验证

### 6.1 为什么等价性验证很重要？

```
如果只说"我们用分类方式重新组织了配置"，
而不证明"分类后的结果和原来一样"，
那分类就只是"看起来整齐"，没有工程价值。

等价性验证要回答：
  - classified 和 round2b_legacy 的 .config 是否完全相同？
  - 如果不同，差异是否在可接受范围内？
  - classified 的 Image/rootfs/boot/mem 是否与 round2b_legacy 一致？
```

### 6.2 验证方法

```bash
# 方法1：对比 kernel.config sha256
sha256sum round2b_legacy/build_evidence/kernel.config
sha256sum classified/build_evidence/kernel.config
# 如果相同 → 配置完全等价

# 方法2：对比 Image 大小和 sha256
sha256sum round2b_legacy/build_evidence/Image
sha256sum classified/build_evidence/Image
# 如果相同 → 编译产物等价

# 方法3：运行态对比
./run_compare_profiles.sh
# 对比 boot time、meminfo、perf 功能
```

### 6.3 验证脚本

```bash
# check_profile_equivalence.sh
# 对比两个 profile 的：
#   - kernel.config sha256
#   - Image sha256
#   - rootfs.img sha256
#   - 运行态 metrics（boot/ms/mem）
```

---

## 七、核心脚本

### 7.1 脚本一览

```
Day18 脚本链：
  apply_config.sh           → 应用 fragment 配置
  run_compare_profiles.sh   → 对比 baseline/legacy/classified
  check_profile_equivalence.sh → 验证 legacy vs classified 等价性
  export_category_view.py   → 导出分类视图
  run_profile_collect.sh    → 采集单个 profile 的 metrics

  collect/
    host_collect.sh         → 宿主机侧采集
    guest_collect.sh         → guest 侧采集
```

### 7.2 apply_config.sh 详解

```bash
# 读取 fragment 链
PROFILE=classified
# fragment 链：
#   trace_baseline.fragment
#   10_required.fragment
#   20_platform.fragment
#   30_debug.fragment
#   40_perf.fragment
#   90_trim_day18.fragment

# 执行 olddefconfig（对每个 fragment 配置项提问）
# 对已有配置的 fragment 项：使用新值
# 对未配置的 fragment 项：保持当前值

# 保存最小配置
savedefconfig
```

### 7.3 export_category_view.py

```bash
# 导出分类视图到 output/category_view.md
python3 export_category_view.py

# 输出：
#   - 五类分类矩阵
#   - 每个 fragment 的配置项
#   - 配置项的 keep/trim 决策理由
```

---

## 八、records 结构

### 8.1 records 目录结构

```
day18/records/
├── <timestamp>-day18-baseline-arm64-virt/
│   ├── build_evidence/
│   │   ├── kernel.config
│   │   ├── Image.sha256
│   │   └── fragments/
│   ├── metrics.env          # image_kib, boot_ms, memfree_kib 等
│   └── *.txt                # dmesg, lspci, perf_* 等
│
├── <timestamp>-day18-round2b_legacy-arm64-virt/
│   └── ...
│
└── <timestamp>-day18-classified-arm64-virt/
    └── ...
```

### 8.2 metrics.env 字段

```bash
# metrics.env 示例
image_kib=27417
rootfs_kib=8128
boot_ms=2054
memtotal_kib=1048576
memfree_kib=961808
slab_kib=8236
modules_loaded_count=1
function_graph_ok=yes
perf_ok=yes
```

---

## 九、与 Day17 的关系

### 9.1 Day17 做了什么

```
Day17 的核心贡献：
  1. perf 工具集成到 rootfs
  2. 构建了完整的 perf_manifest
  3. 验证了 perf stat / perf list 在 guest 可用

Day17 的局限：
  - 没有对"为什么保留 perf"做分类解释
  - round2b fragment 是粗粒度的
```

### 9.2 Day18 继承了什么

```
Day18 继承自 Day17：
  - perf bundle 和 manifest
  - rootfs 打包链
  - collect 脚本框架

Day18 新增：
  - 五类分类 fragment
  - classified profile
  - 等价性验证
```

---

## 十、面试要会讲的五句话

1. **"Day18 的核心是把第二轮裁剪的配置按 required/platform/debug/perf/trim 五类分类组织，让'为什么保留、为什么删除'清晰可讲"**
   → 理解 Day18 的分类体系

2. **"round2b_legacy 和 classified 都代表第二轮裁剪，区别是表达方式不同；等价性验证（.config sha256）证明两者结果完全相同"**
   → 理解 legacy 和 classified 的关系

3. **"required 类是系统必须项（ELF/initramfs/devtmpfs/proc），platform 类是 QEMU virt 平台依赖（DT/PL011/GIC/REGMAP）"**
   → 理解分类逻辑

4. **"debug 类保留的是 ftrace/function_graph tracer 等调试能力，perf 类保留的是 perf stat/list 等性能分析能力"**
   → 理解 debug 和 perf 分类的区别

5. **"trim 类裁掉的是 PCI/SCSI/NET，因为当前实验路径（virt + initramfs + serial log）不依赖这些"**
   → 理解裁剪决策的理由

---

## 十一、验收标准

### 11.1 分类完整性

- [ ] required 类：至少包含 ELF/INITRD/DEVTMPFS/PROC/SYSFS/TMPFS/MODULES
- [ ] platform 类：至少包含 OF/PL011/GIC/IRQ_DOMAIN/REGMAP
- [ ] debug 类：至少包含 DEBUG_FS/TRACING/FTRACE/FUNCTION_GRAPH/IKCONFIG
- [ ] perf 类：至少包含 PERF_EVENTS/HW_PERF_EVENTS
- [ ] trim 类：PCI=n, SCSI=n, NET=n

### 11.2 等价性验证

- [ ] classified 和 round2b_legacy 的 kernel.config sha256 相同
- [ ] classified 和 round2b_legacy 的 Image sha256 相同
- [ ] classified 和 round2b_legacy 的运行态 metrics 偏差在可接受范围

### 11.3 可解释性

- [ ] 每个保留项都有 why_keep_or_trim 说明
- [ ] 每个裁剪项（trim 类）都有 why_trim 说明
- [ ] 五类分类矩阵可导出为 markdown

---

## 附录：fragment 文件对应关系

```
┌─────────────────────────────────────────────────────────────────────┐
│                    fragment 与分类的对应                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  trace_baseline.fragment                                             │
│    → 保留 TRACEPOINTS / TRACING / FTRACE                           │
│                                                                      │
│  10_required.fragment                                               │
│    → BINFMT_ELF / BLK_DEV_INITRD / DEVTMPFS / PROC_FS / SYSFS /    │
│      TMPFS / MODULES                                                │
│                                                                      │
│  20_platform.fragment                                               │
│    → OF / PL011 / ARM_GIC / IRQ_DOMAIN / REGMAP                    │
│                                                                      │
│  30_debug.fragment                                                  │
│    → DEBUG_FS / FUNCTION_GRAPH_TRACER / FRAME_POINTER / IKCONFIG   │
│                                                                      │
│  40_perf.fragment                                                   │
│    → PERF_EVENTS / HW_PERF_EVENTS                                   │
│                                                                      │
│  90_trim_day18.fragment                                             │
│    → PCI=n / SCSI=n / NET=n                                        │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```
