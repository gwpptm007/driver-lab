# Day13 深度指南 - ftrace function_graph 跟踪 IRQ 路径

## 一、Day13 是什么？

Day13 是 W2（嵌入式驱动模式）的第六天，定位是**ftrace function_graph 路径观察**。

**核心目标**：用 function_graph tracer 跟踪一次软件触发 IRQ 的完整调用路径，区分 top-half 路径与 workqueue bottom-half 路径，保存 trace 文本，最终做到**trace 输出可解释**。

Day13 不新增驱动功能。它的重点是：
1. **路径可观测**：用 function_graph 看清 trigger → IRQ core → handler → worker 的完整调用链
2. **两个上下文**：shell 进程上下文（trigger）和 kworker 线程上下文（bottom-half）
3. **trace 归档**：snapshot 对比、文本保存、截图归档

---

## 二、W2 学习路径中的位置

### 2.1 W2 整体架构

```
W2 (嵌入式驱动模式 - day08-14)
├── day08: platform_driver + DT 注入
├── day09: IRQ handler 注册
├── day10: regmap 框架
├── day11: top-half + workqueue bottom-half
├── day12: regmap + debugfs 寄存器快照
├── day13: ftrace function_graph 路径观察   ← 今天
└── day14: bring-up checklist
```

### 2.2 Day13 与前后天的关系

```
Day12 vs Day13：
  - Day12：regmap + debugfs，"状态看得见"
  - Day13：ftrace function_graph，"路径看得见"

Day13 vs Day14：
  - Day13：观察一条具体的 IRQ 路径
  - Day14：把联调经验抽象成 checklist 方法论

Day13 是 W2 的"路径观察收尾"，Day14 是"方法论总结"
```

---

## 三、为什么选 function_graph？

### 3.1 function vs function_graph

```
function tracer：
  - 只能看到"进了哪些函数"
  - 更像是流水账

function_graph tracer：
  - 函数进入 + 函数退出
  - 调用嵌套层次（缩进）
  - 函数大致耗时

function_graph 特别适合观察 IRQ 路径：
  - 入口清晰：trigger_write()
  - 路径清晰：generic_handle_irq() → handle_fasteoi_irq → handler
  - 上下文切换清晰：shell → kworker
```

---

## 四、完整追踪路径

### 4.1 两段追踪路径

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day13 追踪的两段路径                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  第1段：trigger → top-half                                          │
│  ─────────────────────────────────────                              │
│  sh(write trigger)                                                  │
│    → demo_regmap_trigger_write()    [进程上下文 / shell]            │
│    → generic_handle_irq()            [IRQ core 入口]                │
│    → handle_fasteoi_irq()           [GIC flow handler]              │
│    → handle_irq_event()             [IRQ 事件处理]                   │
│    → __handle_irq_event_percpu()    [最终调用 handler]              │
│    → demo_regmap_handler()          [top-half，只做记账+queue_work] │
│    → queue_work()                   [把重活下沉到 workqueue]        │
│                                                                      │
│  第2段：worker 执行重活                                             │
│  ────────────────────────────────                                   │
│  kworker/*                                                         │
│    → worker_thread()                [workqueue 线程]                 │
│    → process_one_work()            [处理单个 work]                   │
│    → demo_regmap_workfn()          [bottom-half，承担重活+延迟统计] │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.2 为什么 trace 里会出现两个主体？

```
sh-xxx：表示当前是 shell 在执行 echo 1 > trigger
kworker/x:y：表示 workqueue 线程正在执行 bottom-half

这恰好说明：
  - 前半段是"触发 + IRQ 派发 + top-half"
  - 后半段是"worker 线程真正开始处理重活"

这是正常现象，不是异常
```

---

## 五、Day13 对 trigger 的小修正

### 5.1 Day12 的问题

```
Day12 的 trigger_write() 里直接调用：
  generic_handle_irq(priv->linux_irq);

这在某些内核上会出现 warning：
  "irq <n> handler demo_regmap_handler enabled interrupts"

根因：
  - generic_handle_irq() 更接近 IRQ core 的标准中断派发入口
  - 它期望调用现场更像 hardirq 进入时的状态
  - 但 debugfs write 本身是普通进程上下文
```

### 5.2 Day13 的修正

```c
for (i = 0; i < times; i++) {
    unsigned long flags;

    local_irq_save(flags);          // 模拟更接近 hardirq 的进入现场
    generic_handle_irq(priv->linux_irq);
    local_irq_restore(flags);       // 恢复原现场
}
```

### 5.3 为什么这样改？

```
local_irq_save/restore 的作用：
  - 保存当前中断状态
  - 禁止本地中断（让调用现场更接近硬中断约束）
  - generic_handle_irq() 不再报 "handler enabled interrupts" warning
  - function_graph 能抓到更干净的 IRQ 派发路径
```

---

## 六、function_graph_targets.txt 详解

### 6.1 文件结构

```
# Day13 function_graph 目标函数集合

# trigger 入口
demo_regmap_trigger_write

# IRQ core 派发链
generic_handle_irq
handle_fasteoi_irq
handle_irq_event
__handle_irq_event_percpu

# top-half handler
demo_regmap_handler
queue_work_on

# bottom-half / worker 路径
worker_thread
process_one_work
demo_regmap_workfn
```

### 6.2 分组含义

```
这个文件不是给内核直接读的，
而是给 guest_trace_irq_path.sh 逐行写入 set_graph_function。

好处：
  1. 很直观地维护"本轮最关心的函数名单"
  2. 不用把函数列表硬编码死在脚本里
  3. 后面扩展实验时，只改这个文件即可
```

---

## 七、guest_trace_irq_path.sh 详解

### 7.1 脚本自动完成的事情

```
1. 检查 debugfs/tracing 是否可用
2. 检查 function_graph 是否在 available_tracers 中
3. 清理旧 tracer / 旧 trace buffer / 旧 graph filter
4. 写入 function_graph_targets.txt 里的重点函数
5. 启用 function_graph
6. 触发一次 echo <times> > /sys/kernel/debug/demo_regmap/trigger
7. 等待 worker 跑完
8. 关闭 tracing
9. 保存结果到 /tmp/day13_*.txt
```

### 7.2 输出文件

```
/tmp/day13_irq_function_graph.txt  # trace 文本
/tmp/day13_snapshot_before.txt     # trace 前寄存器快照
/tmp/day13_snapshot_after.txt     # trace 后寄存器快照
/tmp/day13_trace_meta.txt         # trace 元信息
```

---

## 八、trace 输出怎么解释

### 8.1 推荐解释模板

```
1. demo_regmap_trigger_write() 是软件触发入口
2. generic_handle_irq() 说明这次不是直接调 handler，而是在走 IRQ core
3. handle_fasteoi_irq → handle_irq_event → __handle_irq_event_percpu 是标准派发链
4. demo_regmap_handler() 是 top-half，负责最小动作并 queue_work()
5. worker_thread → process_one_work → demo_regmap_workfn() 表明后续重活已切到 process context
6. 如果 WORK_MS 较大，demo_regmap_workfn() 的持续时间通常比 top-half 更明显
```

### 8.2 为什么会有 IRQ core 这几层？

```
因为不是直接调 demo_regmap_handler()，
而是在走 Linux IRQ 子系统的标准派发链。

generic_handle_irq() → handle_fasteoi_irq() → handle_irq_event() → __handle_irq_event_percpu()

这和真实硬件中断的路径是一样的
```

### 8.3 为什么 demo_regmap_handler() 应该很短？

```
因为它只做：
  - 计数
  - 记时间
  - 增加 pending
  - queue_work()
  - 返回

如果 trace 中观察到它很快返回，而 demo_regmap_workfn() 明显更长，
说明设计符合 day11 的初衷。
```

---

## 九、为什么 trigger 路径更"干净"了？

### 9.1 Day12 的 trace 可能有什么问题？

```
直接调用 generic_handle_irq() 时：
  - IRQ core 认为当前上下文"enabled interrupts"
  - 报 warning，但功能仍然正常
  - trace 里有额外的 warning 信息干扰
```

### 9.2 Day13 的改进

```
加上 local_irq_save/restore 后：
  - 调用现场更接近硬中断约束
  - 不再报 "handler enabled interrupts" warning
  - trace 输出更干净
```

---

## 十、Day13 与 Day14 的关系

### 10.1 Day13 提供了什么？

```
Day13 的 trace 展示了：
  1. top-half（硬中断上下文）只做最小动作
  2. bottom-half（workqueue/进程上下文）承担重活
  3. 中断派发的完整调用链

这正是 Day14 bring-up checklist 第7步：
  "中断联调只看一条最短链"的具体实例
```

### 10.2 Day13 是 W2 的"观测能力总成"

```
W2 演进路线：
  day08-09: DT 匹配 + IRQ handler 注册（"能不能进来"）
  day10: regmap 框架（"怎么统一访问"）
  day11: top-half + bottom-half 分离（"怎么分工"）
  day12: regmap + debugfs 快照（"状态怎么可视化"）
  day13: ftrace function_graph（"路径怎么可解释"）

Day13 是 W2 "可观测性"的收尾
Day14 是 W2 "方法论"的总结
```

---

## 十一、面试要会讲的五句话

1. **"Day13 的核心是用 function_graph tracer 跟踪一次软件触发 IRQ 的完整调用路径，目标是 trace 输出可解释"**
   → 理解 Day13 的目标

2. **"Day13 追踪的两段路径是：trigger → generic_handle_irq → handle_fasteoi_irq → demo_regmap_handler（top-half）和 kworker → worker_thread → demo_regmap_workfn（bottom-half）"**
   → 理解两段追踪路径

3. **"Day13 在 trigger 里加了 local_irq_save/restore，让 generic_handle_irq() 的调用现场更接近硬中断约束，避免 'handler enabled interrupts' warning"**
   → 理解 trigger 的小修正

4. **"function_graph 和 function 的区别是：function 只能看到进了哪些函数，function_graph 还能看到调用嵌套层次和函数大致耗时，更适合观察 IRQ 路径"**
   → 理解为什么选 function_graph

5. **"Day13 是 W2 可观测性的收尾：day08-09 是'能不能进来'，day10 是'怎么统一访问'，day11 是'怎么分工'，day12 是'状态怎么可视化'，day13 是'路径怎么可解释'"**
   → 理解 Day13 在 W2 中的位置

---

## 十二、验收标准

### 12.1 function_graph 启用验收

- [ ] function_graph 在 available_tracers 中
- [ ] /sys/kernel/debug/tracing/trace 可读

### 12.2 追踪路径验收

- [ ] trace 中能抓到 demo_regmap_trigger_write → generic_handle_irq → handle_fasteoi_irq → handle_irq_event → __handle_irq_event_percpu → demo_regmap_handler
- [ ] trace 中能抓到 worker_thread → process_one_work → demo_regmap_workfn

### 12.3 可解释性验收

- [ ] 能用自己的话解释 top-half 在哪里
- [ ] 能解释 worker 在哪里
- [ ] 能解释为什么有 IRQ core 这几层
- [ ] 能解释为什么 worker 比 top-half 更长

### 12.4 归档验收

- [ ] guest_archive_trace.sh 能把 trace 文本、snapshot、dmesg 归档到 /tmp/day13-archive-*/

---

## 附录：完整执行命令

```
# 1. 基础加载
insmod /demo_regmap.ko
cat /sys/kernel/debug/demo_regmap/snapshot

# 2. 验证 trigger 正常
echo 1 > /sys/kernel/debug/demo_regmap/trigger
cat /sys/kernel/debug/demo_regmap/snapshot

# 3. 运行 function_graph 追踪
/bin/day13_trace_irq_path.sh 1
cat /tmp/day13_irq_function_graph.txt

# 4. 调大 WORK_MS 再追踪（让 worker 更显眼）
echo "0x28 50" > /sys/kernel/debug/demo_regmap/poke
/bin/day13_trace_irq_path.sh 1
cat /tmp/day13_irq_function_graph.txt

# 5. 归档
/bin/day13_archive_trace.sh
ls /tmp/day13-archive-*/
```
