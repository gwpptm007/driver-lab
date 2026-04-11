# Day20 回归自动化深度指南 - W3 工程保障

## 一、Day20 是什么？

Day20 是 W3（内核裁剪与移植）的倒数第二天，定位是**回归自动化**。

**核心目标**：把 D15-D18 已经建立的 arm64 + QEMU virt + BusyBox + `demo_regmap.ko` 主线，整理成一套：

- 能执行
- 能留档
- 能汇总
- 能快速判断
- 能用于后续继续扩展

的**自动回归套件目录**。

Day20 不做新的内核裁剪。它的重点是：
1. **回归项定义**：smoke / trace / perf / stress 四层检查
2. **脚本职责拆分**：宿主机脚本 + guest 脚本分层
3. **pass/fail 判定口径**：明确的判定标准
4. **结果归档**：records/ + output/ 两层结果组织

---

## 二、W3 学习路径中的位置

### 2.1 W3 整体架构

```
W3 (内核裁剪与移植 - day15-21)
├── day15: baseline 冻结
├── day16: 第一轮粗裁
├── day17-18: 分类裁剪
├── day19: 量化对比报告
├── day20: 自动回归套件    ← 今天
└── day21: 最终总结报告

W4 (PCIe 基础 - day22-28)
W5 (DMA + 性能 - day29-35)
```

### 2.2 Day20 与前后天的关系

```
Day19：报告型目录（把结果讲清楚）
Day20：回归型目录（让结果可重复验证）
Day21：交付型目录（压缩成 1-2 页总结）

Day19 vs Day20：
  - Day19 偏"量化"，关注对比表和数据
  - Day20 偏"自动化"，关注脚本和回归流程

Day20 vs Day21：
  - Day20 是"过程"，提供回归套件
  - Day21 是"结果"，提供总结报告
```

---

## 三、为什么需要回归自动化？

### 3.1 一次性实验 vs 可重复验证

```
一次性实验的问题：
  - 只能证明"当时成功了"
  - 不能证明"改完之后还成功"
  - 下次改配置，不知道哪里坏了

回归自动化的价值：
  - 每次改完 .config，都能快速验证
  - 能区分"哪些项过了，哪些项没过"
  - 能留档，方便对比历史结果
```

### 3.2 Day20 解决的工程问题

```
没有 Day20：
  - D15 的 baseline 配置、D18 的裁剪结果都是"一次性成功"
  - 改完 .config 后，不知道是否影响启动、demo、trace、perf
  - 需要手动一个个验证，耗时且容易遗漏

有了 Day20：
  - 每次改完跑一遍回归套件
  - 5 分钟内知道哪里坏了
  - 结果自动归档，方便对比
```

---

## 四、回归项清单

### 4.1 四层检查体系

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day20 回归检查四层                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  第1层：smoke（核心链路）                                           │
│  ─────────────────────────────────────                             │
│  - 启动到 shell prompt                                             │
│  - debugfs 可用                                                     │
│  - insmod demo_regmap.ko                                           │
│  - snapshot/trigger 节点正常                                       │
│  - rmmod 正常                                                       │
│                                                                      │
│  第2层：trace/perf（调试能力）                                     │
│  ────────────────────────────────────                              │
│  - function_graph tracer 可见                                      │
│  - trace 脚本可运行                                                │
│  - perf list / perf stat 正常                                     │
│                                                                      │
│  第3层：stress（稳定性）                                           │
│  ────────────────────────────                                      │
│  - 模块多次装卸                                                    │
│  - trigger 连续触发                                                │
│                                                                      │
│  第4层：dmesg（错误扫描）                                          │
│  ─────────────────────────                                          │
│  - 扫描 Oops / BUG / Call trace / panic                           │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.2 smoke 层详解

```bash
# 检查项 1：启动到 shell prompt
# 判定：串口日志里出现 shell prompt

# 检查项 2：debugfs 可用
mount -t debugfs debugfs /sys/kernel/debug
ls /sys/kernel/debug/tracing

# 检查项 3：insmod demo_regmap.ko
insmod /demo_regmap.ko
dmesg | grep -E "error|fail|Oops"  # 应该没有报错

# 检查项 4：snapshot 节点可读
cat /sys/kernel/debug/demo_regmap/snapshot

# 检查项 5：trigger 节点可写
echo "trigger_command" > /sys/kernel/debug/demo_regmap/trigger
cat /sys/kernel/debug/demo_regmap/snapshot  # 状态应有变化

# 检查项 6：rmmod 正常
rmmod demo_regmap
```

### 4.3 trace/perf 层详解

```bash
# 检查项 1：function_graph 可用
cat /sys/kernel/debug/tracing/available_tracers
# 应包含 "function_graph"

# 检查项 2：trace 脚本可运行
# 沿用 Day13/Day17 的 trace 路线
bash /root/run_trace.sh
# 检查归档结果

# 检查项 3：perf 工具可用
perf version
perf list
perf stat true
```

### 4.4 stress 层详解

```bash
# 检查项 1：模块多次装卸
for i in $(seq 1 10); do
    insmod /demo_regmap.ko
    cat /sys/kernel/debug/demo_regmap/snapshot > /dev/null
    rmmod demo_regmap
done

# 检查项 2：trigger 连续触发
for i in $(seq 1 100); do
    echo "trigger" > /sys/kernel/debug/demo_regmap/trigger
done
cat /sys/kernel/debug/demo_regmap/snapshot  # 应该还能读
```

### 4.5 dmesg 扫描

```bash
# 重点排查以下关键词
dmesg | grep -E "Oops|BUG|Call trace|panic|FATAL"
# 任何匹配都说明有问题
```

---

## 五、脚本架构

### 5.1 两层脚本分工

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day20 脚本分层架构                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  宿主机脚本（host）                                                  │
│  ─────────────────                                                  │
│  - 启动 QEMU                                                        │
│  - 采集串口日志                                                     │
│  - 触发 guest 命令                                                  │
│  - 收集结果到 records/                                              │
│                                                                      │
│  guest 脚本（guest/）                                               │
│  ─────────────────                                                  │
│  - guest_day20_smoke.sh    → smoke 层检查                         │
│  - guest_day20_trace.sh    → trace/perf 层检查                     │
│  - guest_day20_perf.sh     → perf 工具检查                        │
│  - guest_day20_stress.sh   → stress 层检查                        │
│  - guest_day20_common.sh   → 公共函数                             │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.2 宿主机脚本职责

```bash
# run_day20_suite.sh - 统一入口
./run_day20_suite.sh verify   # 验证套件状态
./run_day20_suite.sh latest    # 看最近一次结果
./run_day20_suite.sh summary   # 刷新历史汇总
./run_day20_suite.sh dry-run   # 检查运行件是否齐
./run_day20_suite.sh all       # 执行完整回归
```

### 5.3 guest 脚本职责

```bash
# guest/guest_day20_common.sh - 公共函数
# 提供：check_mount(), check_insmod(), check_node() 等

# guest/guest_day20_smoke.sh - smoke 层
source /root/guest_day20_common.sh
check_mount debugfs
check_insmod /demo_regmap.ko
check_node_read /sys/kernel/debug/demo_regmap/snapshot
check_node_write /sys/kernel/debug/demo_regmap/trigger "test"
check_rmmod demo_regmap
```

---

## 六、结果归档

### 6.1 两层结果组织

```
day20/
├── records/
│   └── <timestamp>-day20-all-arm64-virt/
│       ├── summary.txt              # 本轮回归摘要
│       ├── smoke.txt                # smoke 层输出
│       ├── trace.txt                # trace 层输出
│       ├── perf.txt                 # perf 层输出
│       ├── stress.txt               # stress 层输出
│       ├── dmesg.txt                # dmesg 日志
│       ├── serial.log               # 串口完整日志
│       └── host_command.txt         # 宿主机执行命令
│
└── output/
    ├── day20_records_summary.md     # 历史汇总
    ├── day20_latest_report.md       # 最近一次报告
    ├── day20_delivery_status.md     # 交付状态
    └── day20_final_summary.md       # 最终总结
```

### 6.2 交付状态判定

```bash
# SUITE_READY：套件结构是否完整
# RUNTIME_READY：运行件（Image/rootfs/dtb）是否齐全
# REGRESSION_PASS：真实回归是否通过

# 当前状态（示例）
SUITE_READY=1      # 脚本结构完整
RUNTIME_READY=0     # 缺 Image/rootfs/dtb
REGRESSION_PASS=0   # 未执行真实回归
```

---

## 七、常用命令

### 7.1 验证套件状态

```bash
./run_day20_suite.sh verify
# 输出：
# SUITE_READY=1
# DELIVERY_READY=1
# RUNTIME_READY=0
# REGRESSION_PASS=0
```

### 7.2 看最近一次结论

```bash
./run_day20_suite.sh latest
# 输出最近一次回归的结果报告
```

### 7.3 检查运行件

```bash
./run_day20_suite.sh dry-run
# 检查 Image/rootfs/dtb 是否齐全
```

### 7.4 执行完整回归

```bash
./run_day20_suite.sh all
# 完整执行 smoke/trace/perf/stress 层
```

---

## 八、与 Day19 的关系

### 8.1 互补关系

```
Day19：报告型目录
  - 关注：对比表、量化数据
  - 输出：总结报告、对比 markdown
  - 受众：面试官、管理者

Day20：回归型目录
  - 关注：脚本、自动化、pass/fail
  - 输出：回归套件、归档 records
  - 受众：开发者自己
```

### 8.2 核心区别

```
Day19：
  - "我们做到了什么程度"
  - 结论导向

Day20：
  - "怎么验证我们改完之后还保持这个程度"
  - 过程导向
```

---

## 九、验收标准

### 9.1 套件结构验收

- [ ] 有独立 day20/ 目录
- [ ] 有 smoke/trace/perf/stress 四层脚本
- [ ] 有宿主机 + guest 两层脚本分工
- [ ] 有统一入口 run_day20_suite.sh
- [ ] 有 records/ 和 output/ 两层归档

### 9.2 检查项覆盖

- [ ] smoke 层：启动、debugfs、insmod、snapshot、trigger、rmmod
- [ ] trace 层：function_graph、trace 脚本、perf list/stat
- [ ] stress 层：多次装卸、连续触发
- [ ] dmesg 层：Oops/BUG/Call trace/panic 扫描

### 9.3 交付状态

```
当前 Day20 的判定：
  SUITE_READY=1       ← 套件结构已完整
  DELIVERY_READY=1    ← 文档和脚本已就绪
  RUNTIME_READY=0     ← 缺 Image/rootfs/dtb 运行件
  REGRESSION_PASS=0   ← 未执行真实回归

结论：
  Day20 套件本身没问题，
  缺的是运行件，不是脚本问题。
```

---

## 十、面试要会讲的五句话

1. **"Day20 的目标是把 D15-D18 已经跑通的内核裁剪主线，做成一套可重复执行的自动回归套件"**
   → 理解 Day20 的定位

2. **"回归套件分四层：smoke 检查核心链路、trace/perf 检查调试能力、stress 检查稳定性、dmesg 扫描检查错误"**
   → 理解回归项清单

3. **"脚本分两层：宿主机负责启动 QEMU 和采集结果，guest 负责具体检查项"**
   → 理解分层架构

4. **"Day20 和 Day19 是互补关系：Day19 把结果讲清楚，Day20 让结果可重复验证"**
   → 理解 Day20 与 Day19 的区别

5. **"当前 Day20 的套件结构已经完整，缺的是 Image/rootfs/dtb 等运行件，这是大文件问题不是脚本问题"**
   → 理解当前状态

---

## 附录：四层检查详解

```
┌─────────────────────────────────────────────────────────────────────┐
│                    四层检查详解                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  smoke（核心链路）：                                                 │
│    - 必须能启动到 shell                                             │
│    - 必须能 insmod demo_regmap.ko                                   │
│    - 必须能读写 debugfs 节点                                        │
│    - 这是最低保障，不过这个后面都不用测了                            │
│                                                                      │
│  trace/perf（调试能力）：                                           │
│    - W3 裁剪的目标是保留调试能力                                    │
│    - function_graph 必须存在                                        │
│    - perf list/stat 必须正常                                        │
│                                                                      │
│  stress（稳定性）：                                                  │
│    - 验证不是"只成功一次"                                          │
│    - 多次装卸、连续触发                                             │
│                                                                      │
│  dmesg（错误扫描）：                                                │
│    - 扫描 Oops/BUG/Call trace/panic                                │
│    - 任何匹配都说明有问题                                           │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```
