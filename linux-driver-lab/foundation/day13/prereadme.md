# Day13 Pre-README：为什么要用 ftrace function_graph 看一次 IRQ 路径

## 1. 这一天在整个链路里的位置

前面几天你已经把“能工作”的骨架做出来了：

- Day10：中断 top-half 能进入，`/proc/interrupts` 可见
- Day11：重活下沉到 `workqueue`，并记录粗略延迟
- Day12：用 regmap + debugfs 把运行态整理成一组可观测的寄存器视图

所以 Day13 的重点不再是“再实现一个新能力”，而是：

> 把一条你自己能控制、能重复、能解释的 IRQ 路径看清楚。

这正是 ftrace `function_graph` 最适合做的事。

---

## 2. 为什么选 function_graph，而不是别的 tracer

### 2.1 `function` 只能看到“进了哪些函数”

普通 `function` tracer 更像是流水账：告诉你内核进了哪些函数，但不够直观地展示“谁调用了谁、函数什么时候返回”。

### 2.2 `function_graph` 能直接看到调用层级

`function_graph` 会展示：

- 函数进入
- 函数退出
- 调用嵌套层次
- 函数大致耗时

因此它特别适合这一天的任务：

- 看 `demo_regmap_trigger_write()` 如何进入 IRQ core
- 看 `generic_handle_irq()` 如何派发到 `demo_regmap_handler()`
- 看 `queue_work()` 之后为什么会切到 `kworker` 线程执行 `demo_regmap_workfn()`

---

## 3. 为什么 Day13 仍然复用 day12 的 `demo_regmap`

这一天的目标不是重写驱动，而是追踪路径。

所以最稳的做法，是继续复用 day12 已经验证过的教学驱动：

- `snapshot`：帮你看到寄存器视图
- `poke`：帮你调节 `WORK_MS`
- `trigger`：帮你稳定重复地触发一条 IRQ 路径

也就是说，Day13 的价值在于：

> 站在已经能跑通的 day12 基线上，学习怎样观察、解释和归档一条执行路径。

---

## 4. 为什么继续用 `trigger` 做追踪入口

真实硬件中断当然更“天然”，但教学上不一定最合适。

对于 Day13，`trigger` 的优势很明显：

- 可控：你可以精确决定触发 1 次还是 5 次
- 可重复：每次都能重复相同实验
- 可解释：入口就是 `demo_regmap_trigger_write()`，路径清晰
- 易归档：命令、trace、截图都更容易留存

因此 Day13 最推荐的最小实验是：

```text
echo 1 > /sys/kernel/debug/demo_regmap/trigger
```

一次即可，不要上来就追 100 次。

---

## 5. 这一天真正想看清楚的两段路径

### 5.1 第一段：trigger 到 top-half

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

这段路径回答的是：

- 为什么软件写 debugfs 可以驱动一次 fake IRQ
- Linux IRQ core 派发链长什么样
- 你的 top-half 在哪里执行

### 5.2 第二段：worker 真正执行重活

```text
kworker/*
  -> worker_thread()
  -> process_one_work()
  -> demo_regmap_workfn()
```

这段路径回答的是：

- 为什么 day11 说 workqueue 是 process context
- 为什么 top-half 应该很短
- 为什么真正耗时更可能出现在 worker

---

## 6. 你会在 trace 里看到“两个上下文”

这是 Day13 最重要的理解点之一。

你在 trace 里通常会看到类似两个调用主体：

- `sh-xxx`：表示当前是 shell 在执行 `echo 1 > trigger`
- `kworker/x:y-z`：表示工作队列线程正在执行 bottom-half

这恰好说明：

- 前半段是“触发 + IRQ 派发 + top-half”
- 后半段是“worker 线程真正开始处理重活”

这和 day11 的设计目标是严格一致的。

---

## 7. 为什么 Day13 顺手修了 day12 的 software-trigger warning

在 day12 中，`trigger` 是在 debugfs `.write` 里直接调用：

```c
generic_handle_irq(priv->linux_irq);
```

这条路径可以工作，但在某些内核上会看到：

```text
irq <n> handler demo_regmap_handler enabled interrupts
```

其根因不是 handler 主动开中断，而是：

- `generic_handle_irq()` 更接近 IRQ core 的标准中断派发入口
- 它期望调用现场更像 hardirq 进入时的状态
- 但 debugfs write 本身是普通进程上下文，本地中断通常是开的

因此 Day13 把 `trigger` 改成：

```c
local_irq_save(flags);
generic_handle_irq(priv->linux_irq);
local_irq_restore(flags);
```

这样 fake IRQ 的执行现场更接近真实硬中断进入时的约束环境，trace 结果也更干净。

---

## 8. Day13 的最小闭环是什么

1. 启用 `function_graph`
2. 限制 graph function 范围，避免 trace 太杂
3. 清空 trace buffer
4. 触发一次 `echo 1 > trigger`
5. 读取 trace
6. 保存文本、截图、归档
7. 能用自己的话解释这段路径

这就满足了：

> trace 输出可解释

---

## 9. 你最终应该能回答的几个问题

### 9.1 为什么 trace 里会出现 `generic_handle_irq -> handle_fasteoi_irq -> handle_irq_event -> __handle_irq_event_percpu`

因为你不是直接调 `demo_regmap_handler()`，而是在走 Linux IRQ 子系统的标准派发链。

### 9.2 为什么 `demo_regmap_handler()` 很短

因为 top-half 只做最小工作：

- 计数
- 记时间
- 增加 pending
- `queue_work()`
- 返回

### 9.3 为什么 `demo_regmap_workfn()` 看起来更慢

因为它运行在 `kworker` 线程里，承担了模拟重活和统计更新，`WORK_MS` 默认 20ms，本来就比 top-half 长。

### 9.4 为什么这是一次“可解释”的 trace

因为：

- 入口是你自己控制的 `trigger`
- 路径上每一层函数职责都能对应到具体设计
- 输出既能和源码对应，也能和 day10/day11/day12 的学习主线对应

---

## 10. 一句话记忆

**Day12 是“状态看得见”，Day13 是“路径看得见”。**


## 10. 读 day13 源码时要特别注意什么

这版代码已经把新知识点对应的函数都补了较详细注释，阅读时建议特别关注：

- `demo_regmap_trigger_write()`：软件触发 fake IRQ 的入口，也是 function_graph 的总入口
- `demo_regmap_handler()`：top-half 最小职责
- `demo_regmap_workfn()`：bottom-half/workqueue 的批处理逻辑
- `demo_regmap_refresh_view()`：运行态统计如何刷新成寄存器视图
- `demo_regmap_reg_read()` / `demo_regmap_reg_write()`：regmap 软件后端的真正读写点
- `guest_trace_irq_path.sh`：guest 里 function_graph 的一键实验脚本

建议把源码注释、README 测试步骤、trace 输出三者对照着看。
