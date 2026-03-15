# Day20 回归项清单

## 1. 启动类

这些项决定回归有没有基本运行环境。

### 1.1 启动到 shell prompt

判定建议：

- 串口日志里出现 shell prompt
- 宿主机脚本能向 guest 发送命令并获得返回码

### 1.2 基础挂载成功

至少检查：

- `/proc`
- `/sys`
- `/dev`
- `/sys/kernel/debug`

建议保留：

- `mount.txt`
- `filesystems.txt`

### 1.3 debugfs 可用

最直接的验证方式：

- `mount -t debugfs debugfs /sys/kernel/debug`
- `/sys/kernel/debug/tracing` 存在

---

## 2. 驱动 demo 类

D20 当前最适合继续使用 `demo_regmap.ko` 作为验证对象。

### 2.1 模块装载

检查：

- `insmod /demo_regmap.ko` 成功
- dmesg 中无异常报错

### 2.2 snapshot 节点可读

检查：

- `cat /sys/kernel/debug/demo_regmap/snapshot` 成功
- 输出内容非空

### 2.3 trigger 节点可写

检查：

- 向 `trigger` 节点写入命令成功
- 写后再次读取 `snapshot`，状态有变化

### 2.4 模块卸载

检查：

- `rmmod demo_regmap` 成功
- 再次装载仍成功

---

## 3. tracing / perf 类

这些项是 W3 主线不能丢的调试能力。

### 3.1 available_tracers 包含 function_graph

检查：

- `/sys/kernel/debug/tracing/available_tracers`
- 输出中存在 `function_graph`

### 3.2 trace 脚本可运行

建议直接沿用 Day13 / Day17 / Day18 已有 trace 路线，至少确认：

- trace 目录可访问
- trace 命令成功
- 有输出被归档

### 3.3 perf 工具可用

最小检查：

- `perf version`
- `perf list`
- `perf stat true`

---

## 4. 压力与稳定性类

### 4.1 模块多次装卸

建议：

- 循环装卸若干次
- 每轮都做基础节点检查

### 4.2 trigger 连续触发

建议：

- 连续写 trigger 多次
- 检查 snapshot 仍可读取

### 4.3 dmesg 扫描

重点排查：

- `Oops`
- `BUG`
- `Call trace`
- `panic`

---

## 5. 建议的最小首版范围

为了避免 Day20 一上来就太大，首版自动回归建议先覆盖：

- 启动到 prompt
- debugfs 可用
- `insmod /demo_regmap.ko`
- snapshot / trigger / `rmmod`
- `function_graph` 存在
- `perf stat true`
- dmesg 扫描

只要这一组跑通，Day20 第一版就已经有很强的工程价值。
