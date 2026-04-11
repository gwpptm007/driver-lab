# Day13 - ftrace function_graph 跟踪一次 IRQ 路径 + 归档 trace 文本与截图

## 1. 本课目标

Day13 不再新增新的驱动功能点，而是在 day12 已经跑通的教学驱动上，完成下面这件事：

- 使用 `ftrace function_graph` 跟踪一次软件触发 IRQ 的完整调用路径
- 区分 top-half 路径与 workqueue bottom-half 路径
- 保存 trace 文本
- 按实验步骤完成截图归档
- 最终做到：**trace 输出可解释**

---

## 2. 这次为什么继续复用 day12 的 `demo_regmap`

Day12 已经具备下面这些稳定入口：

- `/sys/kernel/debug/demo_regmap/trigger`
- `/sys/kernel/debug/demo_regmap/snapshot`
- `/sys/kernel/debug/demo_regmap/poke`

因此 Day13 直接复用同一个模块，重点放在“观察路径”而不是“改驱动功能”：

- `trigger`：作为可重复、可控的软件触发入口
- `snapshot`：帮助你在 trace 前后对照寄存器状态
- `poke`：帮助你调整 `WORK_MS`，观察 worker 在 trace 中的时长变化

---

## 3. Day13 相比 day12 的一个关键小改动

在 day12 中，`trigger` 是直接从 debugfs `.write` 调用 `generic_handle_irq()`：

```c
for (i = 0; i < times; i++)
    generic_handle_irq(priv->linux_irq);
```

这种写法能工作，但你前面实际测试时看到了 warning：

```text
irq 49 handler demo_regmap_handler enabled interrupts
```

Day13 为了让 `function_graph` 抓到更干净的 IRQ 派发路径，把它改成：

```c
for (i = 0; i < times; i++) {
    unsigned long flags;

    local_irq_save(flags);
    generic_handle_irq(priv->linux_irq);
    local_irq_restore(flags);
}
```

这不是说你的 handler 逻辑变了，而是让 **software-trigger 的执行现场更接近 hardirq 进入时的约束环境**。

---

## 4. 目录说明

```text
day13/
├── Makefile
├── prereadme.md
├── README.md
├── build.sh
├── demo_regmap.c
├── demo_regmap.fragment.dtsi
├── inject_virt_dt.py
├── function_graph_targets.txt
├── guest_trace_irq_path.sh
└── guest_archive_trace.sh
```

其中：

- `demo_regmap.c`：继续复用 day12 的 regmap + debugfs 驱动，并对 software-trigger 做了小修正
- `guest_trace_irq_path.sh`：在 guest 内一键完成 function_graph 配置、触发、保存 trace
- `guest_archive_trace.sh`：把 trace 文本、snapshot、dmesg 等材料归档到 `/tmp/day13-archive-*`

---

## 5. Day13 最推荐追踪的路径

### 5.1 trigger 到 top-half

```text
sh(write trigger)
  -> demo_regmap_trigger_write()
  -> generic_handle_irq()
  -> handle_fasteoi_irq()
  -> handle_irq_event()
  -> __handle_irq_event_percpu()
  -> demo_regmap_handler()
  -> queue_work()
```

### 5.2 worker 执行路径

```text
kworker/*
  -> worker_thread()
  -> process_one_work()
  -> demo_regmap_workfn()
```

这两段结合起来，正好对应：

- top-half 抢现场
- bottom-half 在 process context 里做重活

---

## 6. 编译前环境变量说明

这套 day13 `build.sh` 延续前几天的风格：

- 支持脚本默认路径
- 也支持通过环境变量覆盖

### 6.1 推荐做法

在执行 `./build.sh` 前，先导出：

```bash
export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-
```

然后执行：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day13
./build.sh
```

### 6.2 什么时候更需要这三个变量

- **ARM 场景里更常见三者都要配**
- **非 ARM 场景下，前两个可能仍然要配，第三个则看你是不是交叉编译**

也就是说：

- `KERNEL_DIR`、`BUSYBOX_DIR` 是路径问题
- `CROSS_COMPILE` 是交叉编译问题

你当前这套 `qemu-system-aarch64 + arm64 Image + arm64 busybox` 教学环境，最稳的做法就是先把三者都导出。

---

## 7. build.sh 会做什么

`./build.sh` 会完成下面这些阶段：

1. 编译 `demo_regmap.ko`
2. 构造最小 arm64 rootfs
3. 把 `demo_regmap.ko`、trace 脚本、归档脚本打进 rootfs
4. 导出 QEMU `virt` 基础 DTB
5. 注入 day13 的教学设备节点
6. 启动 arm64 QEMU

---

## 8. 进入 guest 后的最小测试步骤

QEMU 启动后，先执行：

```bash
insmod /demo_regmap.ko
cat /sys/kernel/debug/demo_regmap/snapshot
```

然后建议先验证一次 trigger：

```bash
echo 1 > /sys/kernel/debug/demo_regmap/trigger
cat /sys/kernel/debug/demo_regmap/snapshot
```

确认模块、snapshot、trigger 正常后，再跑 function_graph：

```bash
/bin/day13_trace_irq_path.sh 1
cat /tmp/day13_irq_function_graph.txt
```

如果你想让 worker 更“显眼”一些，可以先调大 `WORK_MS`：

```bash
echo "0x28 50" > /sys/kernel/debug/demo_regmap/poke
/bin/day13_trace_irq_path.sh 1
```

---

## 9. guest_trace_irq_path.sh 做了什么

这个脚本会自动完成：

1. 检查 debugfs/tracing 是否可用
2. 检查 `function_graph` 是否在 `available_tracers` 中
3. 清理旧 tracer / 旧 trace buffer / 旧 graph filter
4. 写入 `function_graph_targets.txt` 里的重点函数
5. 启用 `function_graph`
6. 触发一次 `echo <times> > /sys/kernel/debug/demo_regmap/trigger`
7. 等待 worker 跑完
8. 关闭 tracing
9. 保存结果到：

```text
/tmp/day13_irq_function_graph.txt
/tmp/day13_snapshot_before.txt
/tmp/day13_snapshot_after.txt
/tmp/day13_trace_meta.txt
```

---

## 10. function_graph 输出怎么解释

### 10.1 为什么会看到 `sh-*`

说明当前这段路径仍然是在 shell 执行 `echo > trigger` 这条写路径里。

### 10.2 为什么会看到 `kworker/*`

说明 workqueue 已经接管后续重活，执行上下文从当前 shell 跳到了 worker 线程。

### 10.3 为什么会看到 IRQ core 那几层

```text
generic_handle_irq
handle_fasteoi_irq
handle_irq_event
__handle_irq_event_percpu
```

因为你走的是 **Linux IRQ core 的标准派发链**，而不是直接调用 handler。

### 10.4 为什么 `demo_regmap_handler()` 应该很短

因为它只做：

- 计数
- 记时间
- 增加 pending
- `queue_work()`
- 返回

如果你在 trace 中观察到它很快返回，而 `demo_regmap_workfn()` 明显更长，说明你的设计符合 day11 的初衷。

---


## 11.1 源码阅读建议（建议和 trace 一起看）

这版 day13 已经把源码注释补成“教学版”，建议按下面顺序阅读：

1. 先看 `struct demo_regmap_priv`
   - 建立整份驱动的字段地图
2. 再看 `demo_regmap_probe()`
   - 了解模块接管设备时做了哪些初始化
3. 再看 `demo_regmap_trigger_write()`
   - 这是 day13 function_graph 的总入口
4. 再看 `demo_regmap_handler()`
   - 理解 top-half 为什么要短
5. 最后看 `demo_regmap_workfn()`
   - 理解 bottom-half 为什么运行在 worker 线程里

这样你在阅读 `/tmp/day13_irq_function_graph.txt` 时，会更容易把函数名和源码职责一一对应起来。

## 11. 推荐的 trace 结果解释模板

抓到 trace 后，可以按下面这套话术解释：

1. `demo_regmap_trigger_write()` 是软件触发入口
2. `generic_handle_irq()` 说明这次不是直接调 handler，而是在走 IRQ core
3. `handle_fasteoi_irq -> handle_irq_event -> __handle_irq_event_percpu` 是标准派发链
4. `demo_regmap_handler()` 是 top-half，负责最小动作并 `queue_work()`
5. `worker_thread -> process_one_work -> demo_regmap_workfn()` 表明后续重活已切到 process context
6. 如果 `WORK_MS` 较大，`demo_regmap_workfn()` 的持续时间通常比 top-half 更明显

---

## 12. 截图与归档建议

Day13 的“截图归档”建议至少保留三类材料。

### 12.1 原始 trace 文本

由脚本自动保存：

```text
/tmp/day13_irq_function_graph.txt
```

### 12.2 寄存器快照前后对比

由脚本自动保存：

```text
/tmp/day13_snapshot_before.txt
/tmp/day13_snapshot_after.txt
```

### 12.3 终端截图

建议至少截下面三张：

- 图 1：`insmod` + `snapshot` 正常输出
- 图 2：执行 `/bin/day13_trace_irq_path.sh 1` 后提示保存成功
- 图 3：`cat /tmp/day13_irq_function_graph.txt` 中包含关键调用链的区域

你也可以在 guest 内执行：

```bash
/bin/day13_archive_trace.sh
```

它会把相关文本材料归档到一个时间戳目录，例如：

```text
/tmp/day13-archive-20260312-223000/
```

并生成 `SCREENSHOT_TODO.txt`，提醒你当前这轮实验建议截图哪些位置。

---

## 13. 本课验收标准

### 验收 1

`function_graph` 可成功启用。

### 验收 2

trace 中能抓到至少一条：

```text
demo_regmap_trigger_write
-> generic_handle_irq
-> handle_fasteoi_irq
-> handle_irq_event
-> __handle_irq_event_percpu
-> demo_regmap_handler
```

### 验收 3

trace 中还能看到：

```text
worker_thread
-> process_one_work
-> demo_regmap_workfn
```

### 验收 4

能用自己的话解释：

- top-half 在哪里
- worker 在哪里
- 为什么会有 IRQ core 这几层
- 为什么 worker 比 top-half 更长

只要这四点都能说清楚，就算：

> trace 输出可解释

---

## 14. 常见问题

### 14.1 `available_tracers` 里没有 `function_graph`

说明当前内核没开对应配置，Day13 这套实验无法直接跑，需要先检查内核配置。

### 14.2 trace 输出太多、看不清

优先：

- 一次只触发 1 次
- 使用脚本里默认的 graph function 过滤
- 不要一开始就 `echo 100 > trigger`

### 14.3 为什么 trace 里会出现两个主体（`sh-*` 和 `kworker/*`）

这不是异常，恰恰说明你已经看到了：

- 写 trigger 的当前上下文
- workqueue 的进程上下文

---

## 15. 一句话总结

**Day12 是“状态看得见”，Day13 是“路径看得见”。**
