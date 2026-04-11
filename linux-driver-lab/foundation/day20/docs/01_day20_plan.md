# Day20 计划稿

## 一、Day20 原始需求

`docs/W3_REVIEW.md` 中对 D20 的原始指向是：

- 把 W3 当前裁剪结果从“一次性成功”升级成“可重复验证”
- 给出回归清单
- 设计自动化脚本结构
- 支持启动、demo、tracing、perf、压力项的自动检查

换成一句更贴工程的话：

> **Day20：把 Day15~Day18 已经形成的 arm64 主线，做成一套可重复运行的自动回归目录。**

---

## 二、Day20 的原始目标，翻译成当前工程里的真实含义

在当前 `linux-driver-lab` 里，Day20 不是继续裁剪 `.config`，而是把前面已经跑通的链路固定下来：

- 宿主机负责启动 QEMU、采集串口、触发 guest 命令、收集结果
- guest 负责挂载基础文件系统、装卸模块、跑 trace / perf、做简单压力与错误检查
- 结果要能进入 `records/`，后续可以快速比较成功与失败

如果没有 Day20，Day16 / Day18 的裁剪结果仍然偏“一次性实验”。
有了 Day20，后面每次改 `.config`、改 rootfs、改脚本，都能很快知道哪里坏了。

---

## 三、我建议的 Day20 定位

Day20 建议定位成：

> **一个独立的、自动回归型工程目录。**

它的职责不是输出最终总结报告，而是提供：

- 回归项定义
- 脚本职责拆分
- pass / fail 判定口径
- 回归记录沉淀方式

这和 Day19 的“报告型目录”形成互补：

- Day19 负责把结果讲清楚
- Day20 负责让结果可重复验证

---

## 四、为什么 Day20 仍然要独立目录

### 1）职责隔离

Day18 负责分类裁剪与 profile 等价性验证。  
Day20 负责自动化回归，不应该混在 Day18 里。

### 2）后续演进空间更大

自动化脚本后面很可能持续增强：

- 增加更多 smoke case
- 增加日志解析
- 增加失败分类
- 增加 summary 输出

独立目录更适合持续演进。

### 3）和当前项目风格一致

你前面的 day15/day17/day18/day19 已经都是独立目录。  
Day20 继续保持独立，阅读体验和工程结构最统一。

---

## 五、Day20 最推荐的基线与复用方案

### 1）主线基线

D20 仍然建议沿用 W3 主线统一口径：

- 架构：`arm64`
- 机器：`QEMU virt`
- 启动：`Image + rootfs.img`
- rootfs：BusyBox 路线
- 验证模块：`demo_regmap.ko`
- 调试能力：`debugfs + ftrace + function_graph + perf basic`

### 2）最值得复用的前序成果

#### 来自 Day06 的思路

- `insmod_rmmod.sh`
- `stress_rw.sh`
- `check_dmesg.sh`

这些脚本告诉我们：

- 回归项不要只看功能成功
- 还要看反复装卸、简单压力和 dmesg 是否干净

#### 来自 Day17 / Day18 的结构

- `run_profile_collect.sh`
- `run_qemu.sh`
- `records/` 目录沉淀方式
- `metrics.env` / `baseline.csv` / `serial.log` 等产物组织方式

这些内容说明：

- 宿主机脚本与 guest 结果采集已经有稳定骨架
- Day20 最适合站在这套结构上继续做回归自动化

---

## 六、Day20 最推荐的目标拆分

Day20 不建议一口气做得太大，建议拆三层：

### 第一层：smoke 回归

确认最核心链路没坏：

- 能启动到 shell prompt
- 能挂载 `debugfs`
- 能 `insmod /demo_regmap.ko`
- snapshot / trigger 节点正常
- `rmmod` 正常

### 第二层：trace / perf 回归

确认调试能力没被裁掉：

- `function_graph` tracer 可见
- Day13 那套 IRQ/函数图回归脚本仍可运行
- `perf list` 正常
- `perf stat true` 正常

### 第三层：稳定性/压力回归

确认不是“只成功一次”：

- 模块多次装卸
- trigger 连续触发
- 检查 dmesg 无 `Oops` / `BUG` / `Call trace`

---

## 七、Day20 一句话目标

> **Day20：建立独立的自动回归目录，把 W3 当前 arm64 + QEMU virt + demo_regmap + tracing/perf 主线做成可重复验证的脚本化检查流程。**

---

## 八、Day20 建议推进顺序

1. 先定回归项清单  
2. 再定宿主机 / guest 脚本分工  
3. 再定义 pass / fail 口径  
4. 再落首版自动回归脚本  
5. 最后把结果沉淀到 `records/`  

当前这版完成的是前 3 步的文档化沉淀。
