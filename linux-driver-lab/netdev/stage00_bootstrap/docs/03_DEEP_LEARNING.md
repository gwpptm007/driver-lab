# stage00_bootstrap 深度指南 - 架构中立启动骨架与平台可配置设计

## 一、stage00_bootstrap 是什么？

stage00_bootstrap 是 netdev 主线的第零步，定位是**基础设施准备 + 环境验证**，而不是开始写网络驱动功能。

**核心目标**：
1. **架构中立**：不默认 ARM64，默认 `TARGET_ARCH=host`
2. **平台可参数化**：所有路径、工具链、架构通过变量注入
3. **环境验证**：在进入 stage01 之前，确认工具链和路径可用
4. **可复现**：在不同机器上能跑出相同结果

stage00 不写一行驱动代码，它解决的是"环境对了，后面才能对"这个问题。

---

## 二、netdev 学习路径中的位置

### 2.1 netdev 整体架构

```
netdev/
├── stage00_bootstrap/        ← 今天：基础设施 + 环境验证
├── stage01_netdev_skeleton/  → 最小 net_device 骨架
├── stage02_skb_path/         → skb 生命周期与 TX/RX 软件路径
├── stage03_napi_poll/        → NAPI / poll / 中断抑制
├── stage04_ring_dma/         → ring / DMA / RX replenishment
├── stage05_virtio_param/     → virtio-net 对照 + 平台参数化
└── stage06_arm64_migration/   → ARM64 迁移与跨平台收口
```

### 2.2 stage00 与前后阶段的关系

```
stage00 的目标不是"功能最完整"，而是"基础设施最干净"

stage00 vs stage01：
  - stage00：验证环境工具链、固化路径变量、建立 records 格式
  - stage01：在验证过的环境上，开始写第一个 net_device 驱动

stage00 的产出是整个 netdev 的"底座"：
  - env/stage00.env（架构变量）
  - output/discovered_paths.env（自动发现的路径）
  - output/host_tools.txt（工具可用性）
  - output/stage00_report.md（环境验收结果）
```

---

## 三、为什么 stage00 默认不用 ARM64？

### 3.1 两条路线的选择

```
ARM64 优先路线：
  - 优点：最接近真实嵌入式/汽车/手机场景
  - 缺点：交叉编译、QEMU 模拟、rootfs 复杂度高
  - 风险：环境问题会吞噬早期学习时间

x86 优先路线（stage00 选择的方案）：
  - 优点：native 编译、native QEMU、调试工具全
  - 缺点：与真实 ARM64 场景有距离
  - 收益：先把 netdev 概念学透，再迁移到 ARM64

架构中立路线：
  - 所有架构相关变量通过 env 注入
  - stage06 做一次完整 ARM64 迁移
  - 迁移本身就是独立成果
```

### 3.2 平台可配置的核心理念

```
不写死的内容：
  - TARGET_ARCH（x86_64 / arm64 / host）
  - QEMU_BIN（qemu-system-x86_64 / qemu-system-aarch64）
  - CROSS_COMPILE（gcc / aarch64-linux-gnu-gcc）
  - KERNEL_IMAGE（bzImage / Image）
  - ROOTFS_IMAGE（x86 rootfs / arm64 rootfs）

stage00 验证的是"TARGET_ARCH=host"这条路
stage06 把 TARGET_ARCH=arm64 作为独立迁移任务
```

---

## 四、stage00 做了哪些事

### 4.1 路径自动发现机制

```
discover_paths.sh 的逻辑：

1. 扫描固定可能的路径
   REPO_ROOT/kernel-src/linux-5.15.10/
   REPO_ROOT/kernel-src/linux/
   REPO_ROOT/kernel-src/busybox-1.36.1/

2. 对每个路径检查是否存在子目录
   KERNEL_BUILD_ARM64/ = build/arm64 或 output/arm64
   KERNEL_IMAGE_ARM64  = build/arm64/arch/arm64/boot/Image
   BUSYBOX_BIN_ARM64  = busybox-1.36.1/output/arm64/bin/busybox

3. 如果自动发现失败，允许 local.env 手动覆盖
   → cp env/local.example.env env/local.env
   → vim env/local.env  # 填入实际路径
```

### 4.2 工具链检查

```
check_host_tools.sh 检查：

必须项（arm64 路线）：
  - qemu-system-aarch64
  - aarch64-linux-gnu-gcc
  - arm64 BusyBox 静态链接

可选项（x86 优先路线）：
  - qemu-system-x86_64
  - gcc
  - x86 BusyBox

输出到 output/host_tools.txt：
  OK  gcc -> /usr/bin/gcc
  OK  make -> /usr/bin/make
  OK  qemu-system-x86_64 -> /usr/bin/qemu-system-x86_64
```

### 4.3 STAGE00_READY 判断逻辑

```
判断标准（stage00_report.sh）：

STAGE00_READY = yes 需要同时满足：
  1. KERNEL_IMAGE_ARM64 存在（或 TARGET_ARCH=host）
  2. BUSYBOX_BIN_ARM64 存在（或 TARGET_ARCH=host）
  3. QEMU_BIN 存在

如果 STAGE00_READY = no：
  → stage00_report.md 会给出具体缺项
  → 提示用户补齐后再进入 stage01
```

---

## 五、为什么需要 STAGE00_READY 这个 Gate？

### 5.1 尽早发现环境问题

```
没有 stage00 的情况：
  stage01 写好代码 → build.sh 失败 → 排查路径问题 → 发现交叉编译工具链缺失
  → 浪费 1-2 天在环境上

有 stage00 的情况：
  make all → stage00_report.md 直接告诉缺什么 → 补齐后再开始 stage01
  → 环境问题在 stage00 解决，不带入 stage01
```

### 5.2 统一环境基线

```
stage00 的输出物是标准化的：

output/
├── discovered_paths.env    # 所有路径变量
├── host_tools.txt          # 工具检查结果
└── stage00_report.md      # 验收报告（PASS/FAIL + 建议）

后续 stage01~06 都可以复用 discovered_paths.env：
  source ${STAGE_ROOT}/output/discovered_paths.env
  → 拿到所有 KERNEL_TREE / BUSYBOX_ROOT / QEMU_BIN
```

---

## 六、stage00 与 foundation/dayXX 的模式差异

### 6.1 foundation/dayXX 的模式

```
day01~day35 的结构：
  - 每个 day 有独立 build.sh
  - build.sh 编译驱动 + 准备 rootfs + 启动 QEMU
  - 验证重点：功能是否通

stage00 的结构：
  - 没有驱动代码，只有基础设施脚本
  - 不启动 QEMU，只做宿主机环境检查
  - 验证重点：环境是否准备好
```

### 6.2 为什么 netdev 需要 stage00 而 foundation 没有？

```
foundation/day01~35 每次都复用同一套 kernel + busybox + QEMU 环境
环境问题在最初 day01 就暴露并解决了

netdev 要做 ARM64 迁移：
  - x86 和 ARM64 的工具链不同
  - stage00 验证"当前机器的 x86 环境"
  - stage06 做"切换到 ARM64 环境"的迁移
  → stage00 是 ARM64 迁移的基准线
```

---

## 七、面试要会讲的五句话

1. **"stage00_bootstrap 的核心目标不是写驱动代码，而是做环境验证和路径固化；它通过 discover_paths.sh 自动扫描 kernel/busybox/QEMU 路径，通过 host_tools.txt 检查工具链可用性，通过 stage00_report.md 输出 PASS/FAIL 结果，保证进入 stage01 时环境是正确的"**
   → 理解 stage00 的定位和三个核心脚本的分工

2. **"stage00 默认 TARGET_ARCH=host 而不是 ARM64，是因为 x86 native 环境最简单，能先专注于 netdev 概念本身；ARM64 迁移放在 stage06 作为独立任务，这样 stage00~stage04 可以全程不被交叉编译复杂度干扰"**
   → 理解架构中立选择的背后逻辑

3. **"平台可配置是 stage00 的核心设计原则：TARGET_ARCH、QEMU_BIN、CROSS_COMPILE、KERNEL_IMAGE 都通过 env 变量注入，不写死在脚本里；这样同一个 netdev 代码可以在 x86 和 ARM64 上复用，只需要改 env 参数"**
   → 理解平台可配置的实现方式

4. **"STAGE00_READY 是一个 Gate，它要求 KERNEL_IMAGE、BUSYBOX_BIN、QEMU_BIN 同时存在才认为环境 OK；如果不满足，stage00_report.md 会明确告诉缺哪一项，防止把环境问题带入后续 stage"**
   → 理解 Gate 机制的作用

5. **"stage00 和 foundation/day01~35 的本质区别在于：dayXX 每次验证的是'功能通不通'，stage00 验证的是'环境对不对'；netdev 需要 stage00 是因为后续要做 ARM64 迁移，需要 stage00 先建立 x86 环境基线"**
   → 理解 stage00 在整个 netdev 体系中的独特价值

---

## 八、验收标准

### 8.1 环境验收

- [ ] `make all` 执行成功，无报错
- [ ] `output/discovered_paths.env` 包含所有路径变量
- [ ] `output/host_tools.txt` 显示工具状态（OK/MISSING）
- [ ] `output/stage00_report.md` 存在且 `STAGE00_READY=yes`

### 8.2 路径验收

- [ ] `TARGET_ARCH` 正确（host/x86_64/arm64）
- [ ] `QEMU_X86_64` 或 `QEMU_AARCH64` 存在
- [ ] `CC_HOST` 指向可用 gcc

### 8.3 工具链验收

- [ ] gcc / make / ip / ethtool 可用
- [ ] QEMU x86_64 和/或 aarch64 可用
- [ ] 如果 TARGET_ARCH=arm64，cross-compiler 可用

### 8.4 迁移验收（stage06 时回看）

- [ ] stage06 能在 ARM64 环境下复现相同流程
- [ ] `TARGET_ARCH=arm64` 切换后 stage00 report 显示 ARM64 工具链状态
- [ ] 平台可配置变量全程有效，无 hardcoded 路径

---

## 附录 A：目录结构

```
stage00_bootstrap/
├── README.md
├── Makefile
├── env/
│   └── stage00.env           # 架构变量（TARGET_ARCH / RUN_MODE）
├── scripts/
│   ├── discover_paths.sh      # 自动扫描 kernel/busybox/QEMU 路径
│   ├── check_host_tools.sh   # 检查 gcc/make/qemu/ip/ethtool/perf
│   └── generate_stage00_report.sh  # 生成 stage00_report.md
├── docs/
│   ├── 01_BOOTSTRAP_GUIDE.md # 简化版指南
│   └── 02_DEEP_LEARNING.md   # 本文件：深度学习指南
└── output/
    ├── discovered_paths.env   # 自动发现的路径变量
    ├── host_tools.txt         # 工具检查结果
    └── stage00_report.md     # 验收报告
```

## 附录 B：验收清单

- [x] make all 执行成功
- [x] STAGE00_READY=yes
- [x] output/ 下三个文件齐全
- [x] 工具链检查全部 OK

## 附录 C：完整流程图

```
宿主机环境
     │
     ▼
┌─────────────────────────────┐
│  make all                    │
│    ├─ discover_paths.sh     │  自动扫描 kernel/busybox/QEMU 路径
│    ├─ check_host_tools.sh   │  检查 gcc/make/qemu/ip/ethtool/perf
│    └─ generate_report.sh    │  输出 stage00_report.md + STAGE00_READY
└─────────────────────────────┘
     │
     ▼
output/
├── discovered_paths.env    # KERNEL_TREE / BUSYBOX_ROOT / QEMU_BIN
├── host_tools.txt         # OK  gcc -> /usr/bin/gcc ...
└── stage00_report.md      # STAGE00_READY: yes/no
     │
     ▼
  yes → 进入 stage01_netdev_skeleton
  no  → 补齐缺项后重新 make all
```
