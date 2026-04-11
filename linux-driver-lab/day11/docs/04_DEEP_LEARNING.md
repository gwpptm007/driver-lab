# Day11 深度指南 - top-half + workqueue bottom-half + 粗略延迟统计

## 一、Day11 是什么？

Day11 是 W2（嵌入式驱动模式）的第四天，定位是**top-half + bottom-half 分离与延迟统计**。

**核心目标**：把中断处理分成 top-half（硬中断上下文）和 bottom-half（workqueue 进程上下文），验证"耗时动作已经从硬中断上下文下沉到了进程上下文"。

Day11 不做新设备驱动。它的重点是：
1. **top-half 只做最小动作**：记账、记时间戳、queue_work()
2. **bottom-half 承接重活**：workqueue worker 里执行 msleep 模拟耗时处理
3. **粗略延迟统计**：从 top-half 记录到 worker 开始执行之间的时间差

---

## 二、W2 学习路径中的位置

### 2.1 W2 整体架构

```
W2 (嵌入式驱动模式 - day08-14)
├── day08: platform_driver + DT 注入
├── day09: IRQ handler 注册
├── day10: regmap 框架
├── day11: top-half + workqueue bottom-half   ← 今天
├── day12: Device Tree 深入
├── day13: 回归测试 + 完整链路
└── day14: bring-up checklist
```

### 2.2 Day11 与前后天的关系

```
Day10 vs Day11：
  - Day10：IRQ 进来后只做最小处理
  - Day11：引入 workqueue，把重活下沉到 bottom-half

Day11 vs Day12：
  - Day11：理解 top-half 和 bottom-half 的分工
  - Day12：深入 Device Tree 机制

Day11 是"IRQ 处理深化"，Day12 是"DT 机制深化"
```

---

## 三、top-half vs bottom-half 核心概念

### 3.1 为什么要分 top-half 和 bottom-half？

```
硬中断上下文的限制：
  - 执行时间要尽可能短
  - 不能睡眠（不能 schedule）
  - 不能做耗时操作

top-half（硬中断）适合做：
  - 记录发生了几次 IRQ
  - 记录时间戳
  - 抢现场
  - 立刻唤醒后续处理

bottom-half（软中断/workqueue）适合做：
  - 长时间循环
  - 大量打印
  - 复杂字符串处理
  - 可以睡眠的动作
  - 耗时业务逻辑
```

### 3.2 为什么选 workqueue？

```
为什么不选 softirq/tasklet？
  - softirq 和 tasklet 在软中断上下文执行
  - 不能 sleep
  - 用于对性能要求极高的场景

为什么选 workqueue？
  - workqueue 运行在进程上下文
  - 可以 sleep
  - 便于模拟"真实后处理"
  - 教学目标不是追求极致性能，而是讲清楚"下半部"概念
```

---

## 四、代码执行路径详解

### 4.1 完整执行路径

```
echo 1 > /proc/demo_irq_wq_trigger
    ↓
generic_handle_irq(linux_irq)    [进程上下文]
    ↓
demo_irq_wq_handler()            [top-half / hardirq 上下文]
    → irq_count++
    → last_irq_ns = now
    → pending_events++
    → 如果是本批次第一个事件，记 first_pending_irq_ns
    → queue_work(priv->wq, &priv->work)
    → return IRQ_HANDLED

worker thread 被调度
    ↓
demo_irq_wq_workfn()              [bottom-half / 进程上下文]
    → 取出 batch = pending_events（通过 atomic_xchg 清零）
    → 记录 latency = worker_start - first_pending_irq_ns
    → 更新 last/max/avg 延迟统计
    → msleep(work_ms)  ← 模拟重活
    → 更新 work_runs / work_items
```

### 4.2 top-half 详解

```c
static irqreturn_t demo_irq_wq_handler(int irq, void *dev_id)
{
    // 只做四件事：
    // 1. 记当前时间
    now_ns = ktime_get_ns();
    // 2. 增加 irq_count
    new_count = atomic64_inc_return(&priv->irq_count);
    // 3. pending 事件数 +1
    pending = atomic_inc_return(&priv->pending_events);
    // 4. 记录本批次第一个事件的时间戳
    if (pending == 1)
        atomic64_set(&priv->first_pending_irq_ns, now_ns);
    // 5. 把重活下沉到 worker
    queue_work(priv->wq, &priv->work);

    return IRQ_HANDLED;  // 立刻返回，不做重活
}
```

### 4.3 bottom-half 详解

```c
static void demo_irq_wq_workfn(struct work_struct *work)
{
    for (;;) {
        // 1. 原子地取走 pending 事件
        batch = atomic_xchg(&priv->pending_events, 0);
        if (batch <= 0)
            break;

        // 2. 更新 batch 统计
        atomic_set(&priv->last_batch, batch);
        atomic64_inc(&priv->work_runs);
        atomic64_add(batch, &priv->work_items);

        // 3. 计算粗略延迟
        work_start_ns = ktime_get_ns();
        first_ns = atomic64_read(&priv->first_pending_irq_ns);
        latency_ns = work_start_ns >= first_ns ? work_start_ns - first_ns : 0;

        // 4. 更新延迟统计
        demo_irq_wq_update_latency(priv, latency_ns);

        // 5. 模拟重活（可以 sleep）
        if (work_ms)
            msleep(work_ms);
    }
}
```

---

## 五、关键数据结构

### 5.1 struct demo_irq_wq_priv

```
┌─────────────────────────────────────────────────────────────────────┐
│                    demo_irq_wq_priv 核心字段                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  中断/worker 计数：                                                  │
│    irq_count       → top-half 被触发多少次                          │
│    work_runs       → worker 实际运行了多少轮                        │
│    work_items      → worker 总共处理了多少个事件                    │
│                                                                      │
│  pending 相关：                                                     │
│    pending_events  → 当前还有多少个待处理事件（top-half+1，worker-） │
│    last_batch      → 最近一次 worker 一次处理了多少个事件           │
│                                                                      │
│  延迟相关：                                                          │
│    first_pending_irq_ns → 当前批次第一个 pending IRQ 的时间戳       │
│    last_irq_ns          → 最近一次 IRQ 的时间戳                     │
│    last_latency_ns      → 最近一次 worker 启动延迟                  │
│    max_latency_ns       → 历史最大延迟                              │
│    sum_latency_ns       → 延迟累计值（算平均用）                    │
│    latency_samples      → 延迟样本数                                │
│                                                                      │
│  workqueue 相关：                                                    │
│    wq   → 有序单线程 workqueue（同一模块的 work 按顺序处理）        │
│    work → 具体的 work 对象                                         │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.2 延迟计算

```
粗略延迟定义：
  latency = worker_start_ns - first_pending_irq_ns

为什么叫"粗略"：
  - 不是严格的中断响应时间测试
  - 不包括中断确认延迟、硬件握手等
  - 只是教学用可视化指标
  - 用于展示"中断进来后并没有立刻在 top-half 里做重活"

为什么 latency_samples 很重要：
  - 单次 latency 可能受系统负载影响
  - avg_latency = sum_latency_ns / latency_samples
  - 看 avg 更能反映真实趋势
```

---

## 六、软件触发机制

### 6.1 为什么需要软件触发？

```
day11 的 DT 节点是教学 fake 设备：
  - DT 里写了 interrupts = <...>
  - 内核能把它解析成 Linux IRQ
  - 但 QEMU virt 并不会真的给这个 fake 节点产生硬件事件

所以需要软件触发接口：
  echo 1 > /proc/demo_irq_wq_trigger

这个接口在进程上下文里调用 generic_handle_irq()，
让整条链路跑起来，即使没有真实硬件中断源。
```

### 6.2 generic_handle_irq 详解

```c
// /proc/demo_irq_wq_trigger 的写接口
for (i = 0; i < n; i++) {
    // generic_handle_irq 会走到注册的 irq handler
    // 相当于软中断注入，让 top-half 跑起来
    ret = generic_handle_irq(priv->linux_irq);
}
```

### 6.3 batch 合并现象

```
为什么 irq_count 和 work_runs 不一定一一对应？

queue_work() 的语义：
  - 多次 IRQ 到来时，可能被合并到一次 worker 执行里处理
  - pending_events 会累加，但 worker 可能一次取走多个

举例：
  echo 10 > /proc/demo_irq_wq_trigger
  → irq_count = 10
  → 但 work_runs 可能只有 3（分 3 批处理）
  → work_items = 10（总数对）

理解方式：
  - irq_count：top-half 收到了多少次事件
  - work_runs：bottom-half 启动了多少轮
  - work_items：worker 总共处理了多少个事件
  - last_batch：上一轮一次吃掉了多少个 pending
```

---

## 七、/proc 接口

### 7.1 统计接口 /proc/demo_irq_wq_stats

```
cat /proc/demo_irq_wq_stats 输出：

module=demo_irq_wq
label=irq-workqueue-demo
linux_irq=...
irq_count=10        ← top-half 被触发了多少次
work_runs=3         ← worker 实际运行了多少轮
work_items=10       ← worker 总共处理了多少个事件
pending_events=0    ← 当前还有多少个待处理
last_batch=4        ← 最近一次 worker 一次处理了多少个
work_ms=20          ← 模拟重活的时间（ms）
last_latency_us=... ← 最近一次延迟（微秒）
max_latency_us=...  ← 最大延迟
avg_latency_us=...  ← 平均延迟
latency_samples=... ← 延迟样本数
```

### 7.2 触发接口 /proc/demo_irq_wq_trigger

```
echo 1 > /proc/demo_irq_wq_trigger      # 注入 1 次
echo 10 > /proc/demo_irq_wq_trigger    # 注入 10 次
echo 100 > /proc/demo_irq_wq_trigger   # 注入 100 次
```

---

## 八、workqueue 初始化

### 8.1 probe 中的 workqueue 初始化

```c
// 1. 初始化 work 对象
INIT_WORK(&priv->work, demo_irq_wq_workfn);

// 2. 分配有序单线程 workqueue
priv->wq = alloc_ordered_workqueue(DRV_NAME "_wq", 0);
if (!priv->wq) {
    dev_err(dev, "alloc_ordered_workqueue failed\n");
    return -ENOMEM;
}

// 3. 注册 irq handler
ret = request_irq(priv->linux_irq, demo_irq_wq_handler, 0, DRV_NAME, priv);
```

### 8.2 为什么用 alloc_ordered_workqueue？

```
WQ_HIGHPRI：工作队列优先级（可选）
WQ_FREEZABLE：进程 freeze 时是否等待（可选）
WQ_MEM_RECLAIM：是否参与内存回收（可选）
WQ_POWER_EFFICIENT：是否关注功耗（可选）

alloc_ordered_workqueue 的特点：
  - 有序：同一个 workqueue 里的 work 按 FIFO 顺序执行
  - 单线程：只有一个 worker 线程处理所有 work
  - 便于教学观察：统计更容易解释，不有多线程竞争问题
```

---

## 九、延迟可视化原理

### 9.1 为什么延迟不是固定的？

```
workqueue 的调度本来就不是固定常数：
  - 系统当前负载
  - 触发节奏（连续触发 vs 间隔触发）
  - work_ms 大小（模拟重活越长，batch 越容易合并）
  - 调度器决策

所以延迟值会波动，这是正常的
```

### 9.2 如何观察"重活下沉"？

```
方法1：观察 irq_count 和 work_runs 的关系
  - 如果 irq_count >> work_runs，说明有 batch 合并
  - 说明重活没有在 top-half 里做

方法2：加大 work_ms 再观察
  insmod /demo_irq_wq.ko work_ms=50
  echo 10 > /proc/demo_irq_wq_trigger
  cat /proc/demo_irq_wq_stats
  → last_batch 会更大

方法3：观察 dmesg 日志
  top-half 会打印：top-half irq=x irq_count=x pending=x
  worker 会打印：worker start batch=x latency=x us pending_now=x
```

---

## 十、Day11 与 Day13 的关系

### 10.1 Day11 是 IRQ 处理的深化

```
Day10：IRQ handler 基本注册，中断进来能计数
Day11：IRQ 处理分离，top-half 和 bottom-half 协作

Day11 为 Day13 的完整链路打基础：
  - Day13 需要一条能观测的 IRQ 路径
  - 这条路径就是 top-half → queue_work → worker
```

### 10.2 workqueue 和 Day14 的桥接

```
Day11 的 workqueue 概念：
  - top-half 只做最小动作
  - 真正耗时的在 bottom-half（进程上下文）

Day14 bring-up checklist 的第7步：
  "中断联调只看一条最短链：status -> mask -> ack/clear -> handler"

Day11 提供了这条最短链的可视化实现
```

---

## 十一、面试要会讲的五句话

1. **"Day11 的核心是把中断处理分成 top-half 和 bottom-half：top-half 只做记账、记时间戳、queue_work()，真正的重活在 workqueue worker 里做"**
   → 理解 top-half 和 bottom-half 的分工

2. **"粗略延迟 latency = worker_start_ns - first_pending_irq_ns，它不是硬实时 benchmark，而是教学用可视化指标，用于展示'耗时动作已经从硬中断上下文下沉到了进程上下文'"**
   → 理解延迟的定义和用途

3. **"queue_work() 的语义决定了多次 IRQ 可能被合并到一次 worker 执行，所以 irq_count 和 work_runs 不一定一一对应，看 work_items 和 last_batch 更有解释力"**
   → 理解 batch 合并现象

4. **"generic_handle_irq() 可以在进程上下文里触发软中断注入，让整条 IRQ 处理链路跑起来，即使没有真实硬件中断源"**
   → 理解软件触发机制

5. **"Day11 为 Day13 的完整链路打基础：top-half → queue_work → worker 构成了 Day13 回归测试要观测的 IRQ 路径"**
   → 理解 Day11 在 W2 中的位置

---

## 十二、验收标准

### 12.1 功能验收

- [ ] insmod demo_irq_wq.ko 成功
- [ ] /proc/interrupts 能看到 demo_irq_wq 的计数增长
- [ ] /proc/demo_irq_wq_stats 显示 irq_count / work_runs / work_items
- [ ] echo 1 > /proc/demo_irq_wq_trigger 后 irq_count 增加
- [ ] msleep(work_ms) 在 worker 里执行，top-half 里没有

### 12.2 延迟统计验收

- [ ] last_latency_us / max_latency_us / avg_latency_us 有数值
- [ ] latency_samples > 0
- [ ] 多次触发后 avg_latency 有变化

### 12.3 batch 合并验收

- [ ] echo 10 > /proc/demo_irq_wq_trigger 后 work_runs < 10
- [ ] work_items = 10（总数对）
- [ ] last_batch > 1

---

## 附录：完整验收命令

```
# 1. 基础加载
insmod /demo_irq_wq.ko
dmesg | grep demo_irq_wq
cat /proc/interrupts | grep demo_irq_wq
cat /proc/demo_irq_wq_stats

# 2. 单次触发
echo 1 > /proc/demo_irq_wq_trigger
cat /proc/interrupts | grep demo_irq_wq
cat /proc/demo_irq_wq_stats

# 3. 多次触发观察 batch 合并
echo 10 > /proc/demo_irq_wq_trigger
cat /proc/demo_irq_wq_stats

# 4. 加大 work_ms 观察
rmmod demo_irq_wq
insmod /demo_irq_wq.ko work_ms=50
echo 10 > /proc/demo_irq_wq_trigger
cat /proc/demo_irq_wq_stats

# 5. 验证延迟
echo 100 > /proc/demo_irq_wq_trigger
cat /proc/demo_irq_wq_stats
# 查看 max_latency_us 和 avg_latency_us
```
