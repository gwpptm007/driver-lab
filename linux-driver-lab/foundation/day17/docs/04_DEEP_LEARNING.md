# Day17 perf 集成与第二轮裁剪深度指南 - W3 perf 路线确立

## 一、Day17 是什么？

Day17 是 W3（内核裁剪与移植）的中间阶段，定位是**perf 集成与第二轮裁剪收口**。

**核心目标**：把 perf 工具正式并入 Day17 的执行链，并完成第二轮裁剪（round1 → round2b）的验证。

Day17 不做新的驱动开发。它的重点是：
1. **perf 集成**：build_perf.sh + build.sh 完整打包
2. **第二轮裁剪**：PCI=n、SCSI=n（round1）→ NET=n（round2b）
3. **结果归档**：baseline/round1/round2b 三轮 records 对比

---

## 二、W3 学习路径中的位置

### 2.1 W3 整体架构

```
W3 (内核裁剪与移植 - day15-21)
├── day15: baseline 冻结
├── day16: 第一轮粗裁（round1）
├── day17: perf 集成 + 第二轮裁剪   ← 今天
├── day18: 分类裁剪（classified）
├── day19: 量化对比报告
├── day20: 自动回归套件
└── day21: 最终总结报告
```

### 2.2 Day17 与前后天的关系

```
Day16 vs Day17：
  - Day16：完成了第一轮粗裁
  - Day17：在 Day16 基础上做第二轮裁剪（round2b）

Day17 vs Day18：
  - Day17：完成 round1 → round2b 两轮裁剪验证
  - Day18：在 round2b 基础上做分类裁剪（required/platform/debug/perf）

Day17 vs Day19：
  - Day17：建立了 perf 工具路线
  - Day19：把 baseline/round1/round2b 的数据整理成对比表
```

---

## 三、perf 集成体系

### 3.1 为什么需要在 Day17 集成 perf？

```
perf 是 Linux 性能分析的基础工具：
  - perf stat：统计事件计数
  - perf list：列出可用事件
  - perf record/report：采样分析

Day17 之前：
  - Day13-16 的实验没有 perf
  - 无法做性能 profiling

Day17 集成 perf 后：
  - guest 内可以直接运行 perf stat / perf list
  - 为 Day19 的量化对比提供 perf_ok 指标
```

### 3.2 perf 集成架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day17 perf 集成架构                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  build_perf.sh                                                       │
│  ─────────────                                                       │
│  1. 编译 tools/perf/perf                                             │
│  2. 检查是否静态链接或收集动态库依赖                                   │
│  3. 输出：day17/output/perf/perf                                     │
│                                                                      │
│  build.sh                                                            │
│  ───────                                                             │
│  1. 检查 perf 是否存在（PERF_MODE=auto/ext/kernel）                   │
│  2. 解析 ELF interpreter 和 NEEDED 依赖                              │
│  3. 递归复制依赖库到 rootfs/lib                                      │
│  4. 生成 perf_bundle_manifest.txt                                    │
│  5. 把 manifest 放入 guest：/etc/day17_perf_manifest.txt            │
│                                                                      │
│  guest_collect.sh                                                    │
│  ─────────────────                                                     │
│  1. perf --version                                                   │
│  2. perf list                                                        │
│  3. perf stat -e task-clock -- /bin/true                             │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.3 build_perf.sh 详解

```bash
# 基本用法
./build_perf.sh

# 输出
day17/output/perf/perf              # 最终二进制
day17/output/perf/perf.file.txt     # ELF 类型检查
day17/output/perf/perf.dynamic.txt   # NEEDED 依赖列表
```

### 3.4 build.sh 的 perf 打包逻辑

```bash
# PERF_MODE=auto 的查找顺序
1. PERF_PATH 环境变量
2. day17/output/perf/perf
3. KERNEL_SRC/tools/perf/perf
4. 自动执行 ./build_perf.sh

# 依赖收集流程
1. 解析 ELF interpreter（获取动态链接器）
2. 解析 NEEDED 依赖库
3. 根据 PERF_LIB_DIRS / PERF_SYSROOT 查找库文件
4. 递归复制依赖库的依赖
5. 生成 day17/output/perf/perf_bundle_manifest.txt
6. 把 manifest 放入 guest：/etc/day17_perf_manifest.txt
```

### 3.5 perf smoke 验证

```bash
# Day17 最终版的 perf smoke
perf stat -e task-clock -- /bin/true

# 为什么用 /bin/true？
# - 最小 rootfs 可能没有其他可执行文件
# - /bin/true 是 BusyBox 的 applet，必须存在
# - 避免 "Workload failed: No such file or directory"
```

### 3.6 perf 验收标准

```bash
# 验收标志
perf_bin_ok=yes      # perf 二进制存在且可执行
perf_smoke_ok=yes    # perf stat -e task-clock -- /bin/true 成功

# records 归档
perf_version.txt     # perf --version 输出
perf_list.txt       # perf list 输出
perf_stat.txt       # perf stat -e task-clock -- /bin/true 输出
perf_manifest.txt   # /etc/day17_perf_manifest.txt
```

---

## 四、第二轮裁剪

### 4.1 为什么需要第二轮裁剪？

```
Day16 第一轮粗裁：
  - 关闭了部分明显无关的驱动
  - 但 round1/round2b 命名不一致

Day17 第二轮裁剪（round1 → round2b）：
  - round1：PCI=n, SCSI=n
  - round2b：在 round1 基础上 NET=n
  - 目标：继续减少 Image 大小，同时保持功能链完整
```

### 4.2 trim_round1.fragment 详解

```bash
# round1 裁剪项：PCI=n, SCSI=n

# 为什么裁剪 PCI？
# - QEMU virt 当前实验不挂 PCI 设备
# - guest shell / tracing / perf / demo_regmap 都不依赖 PCI 枚举
# - CONFIG_PCI is not set

# 为什么裁剪 SCSI？
# - 当前使用 initramfs 启动，没有依赖 SCSI 磁盘
# - virtio-mmio + initramfs 不需要 SCSI
# - CONFIG_SCSI is not set

# 预期结果
# - kernel.config 发生变化
# - Image sha256 发生变化
# - 功能链继续 PASS
```

### 4.3 trim_round2b.fragment 详解

```bash
# round2b 裁剪项：在 round1 基础上 NET=n

# 为什么裁剪 NET？
# - host_collect 走串口，guest 不需要 SSH/DHCP/TCP/IP
# - perf/ftrace/debugfs/demo_regmap/initramfs 启动链都不依赖 NET
# - CONFIG_NET is not set

# 预期结果
# - kernel.config 再次发生变化
# - Image sha256 再次发生变化
# - 功能链继续 PASS
```

### 4.4 裁剪决策的理由链

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day17 裁剪决策理由链                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  round1: PCI=n, SCSI=n                                              │
│  ────────────────────────────                                        │
│  理由：QEMU virt + initramfs + demo_regmap + tracing/perf 链        │
│        不依赖 PCI/SCSI 子系统                                        │
│                                                                      │
│  round2b: NET=n                                                    │
│  ─────────────────────                                              │
│  理由：host_collect 走串口，guest 不需要网络栈                       │
│        perf/ftrace/debugfs 都不依赖 NET                             │
│                                                                      │
│  验证方法：                                                          │
│    - lspci 在 guest 里没有有用输出（PCI=n）                         │
│    - /proc/scsi/scsi 不存在（SCSI=n）                               │
│    - ifconfig 看不到 eth0（NET=n）                                  │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 五、profile 执行流程

### 5.1 两种执行方式

```bash
# 方式1：一次性对比（推荐）
./run_compare_rounds.sh
# 内部执行：baseline → round1 → round2b 三轮对比

# 方式2：分步执行
./run_profile_collect.sh baseline
./run_profile_collect.sh round1
./run_profile_collect.sh round2b
python3 ./compare_results.py
```

### 5.2 apply_config.sh 用法

```bash
# 应用 baseline 配置
PROFILE=baseline ./apply_config.sh

# 应用 round1 配置
PROFILE=round1 ./apply_config.sh

# 应用 round2b 配置
PROFILE=round2b ./apply_config.sh

# 只生成配置，不编译
PROFILE=round1 BUILD_KERNEL=no ./apply_config.sh
```

### 5.3 build.sh 用法

```bash
# 完整构建并启动
./build.sh

# 只构建，不启动
QEMU_AUTO_BOOT=no ./build.sh

# perf 相关选项
PERF_REQUIRED=yes              # perf 必须存在
PERF_MODE=auto                 # 自动查找/构建 perf
PERF_MODE=external            # 使用外部 perf
PERF_PATH=/path/to/perf       # 指定外部 perf 路径
```

---

## 六、records 结构

### 6.1 三轮 records 对比

```
day17/records/
├── <timestamp>-day17-baseline-arm64-virt/
│   ├── build_evidence/
│   │   ├── kernel.config
│   │   ├── Image.sha256
│   │   └── fragments/trace_baseline.fragment
│   ├── metrics.env           # image_kib, boot_ms, memfree_kib, perf_ok 等
│   ├── perf_version.txt
│   ├── perf_list.txt
│   ├── perf_stat.txt
│   └── *.txt                # dmesg, serial, modules 等
│
├── <timestamp>-day17-round1-arm64-virt/
│   └── ...                   # trim_round1.fragment 已应用
│
└── <timestamp>-day17-round2b-arm64-virt/
    └── ...                   # trim_round2b.fragment 已应用
```

### 6.2 metrics.env 字段

```bash
# 核心指标
image_kib              # 内核 Image 大小（KiB）
rootfs_kib             # rootfs.img 大小（KiB）
boot_ms                # 启动时间（ms）
memfree_kib            # MemFree（KiB）
slab_kib               # Slab 大小（KiB）

# perf 相关
perf_bin_ok=yes/no     # perf 二进制是否存在
perf_smoke_ok=yes/no   # perf stat 是否成功

# 功能指标
function_graph_ok=yes/no  # function_graph tracer 是否可用
modules_loaded_count=1    # 运行时模块数
```

---

## 七、与 Day16 的关系

### 7.1 Day16 做了什么

```
Day16 第一轮粗裁：
  - 关闭了部分明显无关的驱动
  - 但命名和路径不统一

Day16 的局限：
  - 没有 perf 工具
  - 没有建立标准化的 records 结构
```

### 7.2 Day17 继承了什么

```
Day17 继承自 Day16：
  - demo_regmap.ko 模块
  - DT 注入流程
  - guest/host 采集框架

Day17 新增：
  - perf 工具集成
  - round1 (PCI=n, SCSI=n)
  - round2b (NET=n)
  - 标准化的 records 对比结构
```

---

## 八、与 Day18 的关系

### 8.1 Day18 做什么

```
Day18 在 Day17 round2b 基础上：
  - 引入分类 fragment（required/platform/debug/perf/trim）
  - 验证 classified 和 round2b_legacy 等价性

Day18 的分类是对 Day17 裁剪结果的"分类解释"
```

### 8.2 核心区别

```
Day17：回答"第二轮裁剪（round1 → round2b）是否有收益"
Day18：回答"round2b 的结果用分类方式表达是否成立"

Day17 的价值：
  - 证明了 PCI=n, SCSI=n, NET=n 功能链仍完整

Day18 的价值：
  - 把这些裁剪决策按"为什么删"分类解释
```

---

## 九、面试要会讲的五句话

1. **"Day17 的核心是把 perf 工具正式并入 rootfs 执行链，并通过 build_perf.sh + build.sh 完整打包"**
   → 理解 Day17 的 perf 集成

2. **"round1 裁剪 PCI=n 和 SCSI=n，因为 QEMU virt + initramfs + demo_regmap 链不依赖这些子系统"**
   → 理解 round1 裁剪逻辑

3. **"round2b 在 round1 基础上继续裁剪 NET=n，因为 host_collect 走串口，guest 不需要网络栈"**
   → 理解 round2b 裁剪逻辑

4. **"perf smoke 用 perf stat -e task-clock -- /bin/true，因为最小 rootfs 必须有 /bin/true"**
   → 理解 perf smoke 的实现细节

5. **"Day17 建立的是 perf 工具路线，Day18 的分类裁剪是在这个基础上做分类解释"**
   → 理解 Day17 和 Day18 的关系

---

## 十、验收标准

### 10.1 perf 集成验收

- [ ] `perf --version` 在 guest 内可执行
- [ ] `perf list` 输出非空
- [ ] `perf stat -e task-clock -- /bin/true` 成功
- [ ] `records/perf_version.txt` 等 4 个 perf 文件已归档

### 10.2 第二轮裁剪验收

- [ ] round1 (PCI=n, SCSI=n) 后功能链继续 PASS
- [ ] round2b (NET=n) 后功能链继续 PASS
- [ ] 三轮 Image 大小呈下降趋势
- [ ] 三轮的 kernel.config sha256 各不相同

### 10.3 records 结构验收

- [ ] 三轮 records 都有完整的 metrics.env
- [ ] 三轮 records 都有 build_evidence/
- [ ] compare_results.py 可生成对比表

---

## 附录：完整执行命令

```
# 完整执行流程
cd ~/workspace/driver-lab/linux-driver-lab/day17

export KERNEL_DIR=~/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=~/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-

# 1. baseline
PROFILE=baseline ./apply_config.sh
PERF_REQUIRED=yes PERF_MODE=auto ./build.sh

# 2. round1
PROFILE=round1 ./apply_config.sh
PERF_REQUIRED=yes PERF_MODE=auto ./build.sh

# 3. round2b
PROFILE=round2b ./apply_config.sh
PERF_REQUIRED=yes PERF_MODE=auto ./build.sh

# 4. 对比三轮结果
python3 ./compare_results.py
```
