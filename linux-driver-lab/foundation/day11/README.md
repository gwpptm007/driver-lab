# Day11 - top-half + workqueue bottom-half + 粗略延迟统计

## 1. 本课目标

day10 已经把这条链路跑通：

- Device Tree 生成 platform_device
- platform_driver 通过 `of_match_table` 匹配
- `platform_get_irq()` 解析出 Linux virq
- `request_irq()` 注册 top-half
- `/proc/demo_irq_trigger` 做软件注入
- `/proc/interrupts` 可以看到计数增长

day11 在这个基础上继续往前走一步：

- 引入 `workqueue` 作为 bottom-half
- 把“重活”从硬中断上下文下沉到 worker
- 记录从 top-half 到 worker 开始执行之间的**粗略延迟**
- 通过 `/proc` 导出统计结果

你的验收目标对应到代码里就是：

1. **中断处理函数足够短**：只做记账和 `queue_work()`
2. **真正耗时动作在 workqueue 中执行**：允许 `msleep()`
3. **延迟数据可见**：`cat /proc/demo_irq_wq_stats`

---

## 2. 为什么 day11 要引入 bottom-half

### 2.1 top-half 适合做什么

硬中断上下文里最适合做的是：

- 记录发生了几次 IRQ
- 记录时间戳
- 抢现场
- 立刻唤醒后续处理

也就是：**短、快、确定性强**。

### 2.2 top-half 不适合做什么

不适合在中断上下文里直接做：

- 长时间循环
- 大量打印
- 复杂字符串处理
- 可能睡眠的动作
- 明显耗时的业务逻辑

所以 day11 的核心思想就是：

> top-half 只做最小动作，真正的重活交给 bottom-half。

### 2.3 为什么这里选 workqueue

因为这次教学目标不是追求极致性能，而是想把“下半部”的概念讲清楚。
`workqueue` 非常适合这个阶段：

- 运行在**进程上下文**
- 可以 `sleep`
- 便于模拟“真实后处理”
- 后面你写实际驱动时也更常见

---

## 3. 这节课的执行路径

```text
echo 1 > /proc/demo_irq_wq_trigger
    -> generic_handle_irq(linux_irq)
    -> demo_irq_wq_handler()          [top-half / hardirq context]
         -> irq_count++
         -> last_irq_ns = now
         -> pending_events++
         -> 如果是本批次第一个事件，记 first_pending_irq_ns
         -> queue_work(priv->wq, &priv->work)
         -> return IRQ_HANDLED

worker thread 被调度
    -> demo_irq_wq_workfn()           [process context]
         -> 取出 batch = pending_events
         -> 记录 latency = worker_start - first_pending_irq_ns
         -> 更新 last / max / avg 延迟统计
         -> 模拟重活 msleep(work_ms)
         -> 更新 work_runs / work_items
```

这条路径就是 day11 最重要的学习结果。

---

## 4. 什么叫“粗略延迟”

本实验里导出的延迟，不是硬实时 benchmark，也不是严格的中断响应时间测试。

这里的定义是：

> 从当前批次第一个 pending IRQ 在 top-half 中被记录，到 worker 真正开始运行之间的时间差。

也就是：

```text
latency = worker_start_ns - first_pending_irq_ns
```

它的价值在于：

- 能直观看到中断不是在 top-half 里做重活
- 能看到 workqueue 调度是有等待时间的
- 触发多次后，`last/max/avg` 都能观察变化

所以它是**教学用可视化指标**，不是精密 benchmark。

---

## 5. 为什么还能继续使用“软件触发”

这次 DT 节点仍然是一个教学 fake 设备：

- DT 里写了 `interrupts = <...>`
- 内核能够把它解析成一条 Linux IRQ
- 但 QEMU `virt` 并不会真的给这个 fake 节点产生硬件事件

所以 day11 仍然保留一个教学接口：

```bash
echo 1 > /proc/demo_irq_wq_trigger
```

这个接口会在进程上下文里调用 `generic_handle_irq()`，让整条链路跑起来。

这样即使没有真实硬件中断源，你依然能把：

- `request_irq()`
- top-half
- `queue_work()`
- workqueue worker
- `/proc/interrupts`
- `/proc/demo_irq_wq_stats`

完整观察一遍。

---

## 6. 目录说明

```text
day11/
├── Makefile
├── build.sh
├── demo_irq_wq.c
├── demo_irq_wq.fragment.dtsi
├── inject_virt_dt.py
└── README.md
```

---

## 7. 代码里的几个关键字段

`struct demo_irq_wq_priv` 里最值得关注的是这些字段：

### 7.1 中断 / worker 计数

- `irq_count`：top-half 被触发多少次
- `work_runs`：worker 实际运行了多少轮
- `work_items`：worker 一共处理了多少个 pending 事件

### 7.2 pending 相关

- `pending_events`：当前还有多少个待处理事件
- `last_batch`：上一轮 worker 一次处理了多少个事件

### 7.3 延迟相关

- `first_pending_irq_ns`：当前批次第一个 pending IRQ 的时间
- `last_irq_ns`：最近一次 IRQ 的时间
- `last_latency_ns`：最近一次 worker 启动延迟
- `max_latency_ns`：最大延迟
- `sum_latency_ns` + `latency_samples`：用于计算平均延迟

### 7.4 workqueue 相关

- `wq`：有序单线程 workqueue
- `work`：实际执行的 work 对象

---

## 8. top-half 和 worker 分工

### 8.1 top-half：`demo_irq_wq_handler()`

这个函数刻意保持很短，只做：

1. 记录当前时间
2. `irq_count++`
3. `pending_events++`
4. 如果这是本批次第一个事件，记录 `first_pending_irq_ns`
5. `queue_work()`
6. 返回 `IRQ_HANDLED`

换句话说：

> top-half 只抢现场，不做重活。

### 8.2 worker：`demo_irq_wq_workfn()`

worker 里做真正的后处理：

1. 取出一批 pending 事件
2. 计算本轮粗略延迟
3. 更新 `last/max/avg` 统计
4. `msleep(work_ms)` 模拟重活
5. 更新处理计数

这里故意使用 `msleep()`，目的就是让你清楚看到：

- **这段逻辑已经不在硬中断上下文里了**
- 所以“重活下沉”是成立的

---

## 9. 为什么 `irq_count` 和 `work_runs` 不一定一样

这是 day11 很值得理解的点。

`queue_work()` 的语义决定了：

- 多次 IRQ 到来时，可能被合并到一次 worker 执行里处理
- 所以 `irq_count` 可能增长很多次
- 但 `work_runs` 不一定一一对应

因此更合理的理解方式是：

- `irq_count` 看 top-half 收到了多少次事件
- `work_runs` 看 bottom-half 启动了多少轮
- `work_items` 看 worker 总共处理了多少个事件
- `last_batch` 看上一轮一次吃掉了多少个 pending

这比单纯追求“一一对应”更接近真实驱动逻辑。

---

## 10. `/proc` 接口说明

### 10.1 触发接口

```bash
echo 1 > /proc/demo_irq_wq_trigger
```

或：

```bash
echo 10 > /proc/demo_irq_wq_trigger
```

表示做 1 次或 10 次软件注入。

### 10.2 统计接口

```bash
cat /proc/demo_irq_wq_stats
```

你会看到类似：

```text
module=demo_irq_wq
label=irq-workqueue-demo
linux_irq=...
irq_count=10
work_runs=3
work_items=10
pending_events=0
last_batch=4
work_ms=20
last_latency_us=...
max_latency_us=...
avg_latency_us=...
```

其中最重要的是：

- `irq_count`
- `work_runs`
- `work_items`
- `last_latency_us`
- `max_latency_us`
- `avg_latency_us`

---

## 11. `build.sh` 在做什么

### 11.1 编译模块

先执行：

```bash
make ... clean
make ...
```

生成 `demo_irq_wq.ko`。

### 11.2 构造最小 rootfs

把静态 BusyBox 拷进去，创建常用命令软链接，再把模块拷到 guest 根目录。

### 11.3 生成 `/init`

启动时自动挂载：

- `/proc`
- `/sys`
- `/dev`

并打印 day11 的常用验收命令。

### 11.4 导出 QEMU virt 基础 DTB

通过：

```bash
qemu-system-aarch64 -machine virt,dumpdtb=virt-base.dtb ...
```

得到基础 DTB。

### 11.5 注入 day11 的 fake 设备节点

流程是：

```text
virt-base.dtb
  -> virt-base.dts
  -> inject_virt_dt.py 插入 demo_irq_wq.fragment.dtsi
  -> virt-day11.dtb
```

### 11.6 启动 QEMU

最后把：

- `Image`
- `virt-day11.dtb`
- `rootfs.img`

一起喂给 QEMU，进入 guest 后直接测试。

---

## 12. Guest 里验收命令

### 12.1 基础加载

```bash
insmod /demo_irq_wq.ko
dmesg | grep demo_irq_wq
cat /proc/interrupts | grep demo_irq_wq
cat /proc/demo_irq_wq_stats
```

### 12.2 单次触发

```bash
echo 1 > /proc/demo_irq_wq_trigger
cat /proc/interrupts | grep demo_irq_wq
cat /proc/demo_irq_wq_stats
```

你应该看到：

- `irq_count` 增加
- `work_runs` 增加
- `work_items` 增加
- `last_latency_us` 有数值

### 12.3 多次触发

```bash
echo 10 > /proc/demo_irq_wq_trigger
cat /proc/demo_irq_wq_stats
```

你应该看到：

- `irq_count` 继续增长
- `work_runs` 不一定等于 10
- `work_items` 会累计到 10
- `last_batch` 可能大于 1

### 12.4 观察更明显的“重活下沉”

你可以把模拟重活改大一点：

```bash
rmmod demo_irq_wq
insmod /demo_irq_wq.ko work_ms=50
```

然后再次触发：

```bash
echo 10 > /proc/demo_irq_wq_trigger
cat /proc/demo_irq_wq_stats
```

这样更容易看到 batch 合并和延迟变化。

---

## 13. 这节课如何证明“没有长时间把活留在中断里”

严格讲，day11 不是在做 `irqsoff` tracer 的定量 benchmark。
但从教学角度，它已经给了你两层证据。

### 证据 1：代码结构

`demo_irq_wq_handler()` 里没有做长耗时逻辑，只做：

- 计数
- 记时间
- `queue_work()`
- 返回

### 证据 2：行为分离

真正模拟重活的是：

```c
msleep(work_ms);
```

它只出现在 worker 里，不在 top-half 里。

这就说明：

> 耗时动作已经从硬中断上下文下沉到了进程上下文。

这正是 day11 想验证的事。

---

## 14. 常见问题

### 14.1 为什么 `/proc/interrupts` 的计数增加了，但 `work_runs` 不是每次都 +1？

因为多个 IRQ 可能被合并到同一轮 worker 里处理，这是正常现象。
看：

- `work_items`
- `last_batch`

更有解释力。

### 14.2 为什么延迟值不是固定的？

因为 workqueue 的调度本来就不是固定常数，系统负载、触发节奏、`work_ms` 大小都会影响它。

### 14.3 为什么这里说“粗略延迟”？

因为这个值主要是为了教学展示“中断进来后并没有立刻在 top-half 里做重活”，不是为了做严格性能评测。

---

## 15. 一句话总结

**day10 学的是：IRQ 进来了。**

**day11 学的是：IRQ 进来以后，top-half 只做最小动作，真正的重活交给 workqueue bottom-half，并把这段等待过程通过统计数据展示出来。**
