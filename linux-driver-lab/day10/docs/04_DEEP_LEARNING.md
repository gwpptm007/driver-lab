# Day10 深度指南 - request_irq + top-half + /proc/interrupts 验证

## 一、Day10 是什么？

Day10 是 W2（嵌入式驱动模式）的第三天，定位是**request_irq + top-half 注册与双重计数验证**。

**核心目标**：把 Day09 解析出的 DT 中断资源，真正接到 `request_irq()`，通过软件触发把 top-half 跑起来，最终用 `/proc/interrupts` 和驱动内部统计互相印证。

Day10 不做真实硬件中断。它的重点是：
1. **IRQ 注册链路跑通**：`platform_get_irq()` → `request_irq()` → handler
2. **top-half 最小实现**：只做计数 + 限速日志
3. **双重验证**：`/proc/interrupts`（内核通用视角）+ `/proc/demo_irq_stats`（驱动自身视角）
4. **软件触发机制**：`generic_handle_irq()` 在无真实外设前提下模拟 IRQ 注入

---

## 二、W2 学习路径中的位置

### 2.1 W2 整体架构

```
W2 (嵌入式驱动模式 - day08-14)
├── day08: platform_driver + DT 注入
├── day09: DT 解析（reg / interrupts / 自定义属性）
├── day10: request_irq + top-half + proc 验证   ← 今天
├── day11: top-half + workqueue bottom-half
├── day12: regmap + debugfs 寄存器快照
├── day13: ftrace function_graph 路径观察
└── day14: bring-up checklist
```

### 2.2 Day10 与前后天的关系

```
Day09 vs Day10：
  - Day09：解析 DT，把"原始描述"变成"内核资源"
  - Day10：把"内核资源"变成"可观测的中断处理路径"

Day10 vs Day11：
  - Day10：top-half 只有最小计数
  - Day11：top-half 记账 + queue_work()，bottom-half 承接重活

Day10 是 W2 的"中断入口"，
Day11 是 Day10 的"中断深化"
```

---

## 三、为什么需要软件触发？

### 3.1 fake 设备的本质

```
QEMU virt 的 DT 节点是我们手工注入的教学 fake 设备：
  - DT 里写了 reg / interrupts
  - 内核能翻译成 Linux IRQ
  - request_irq() 能成功

但 QEMU virt 并没有给这个 fake 节点接上真实硬件——
没有真实外设会主动拉高中断线。

所以即使 request_irq() 成功，
/proc/interrupts 的计数也不会自动增长。
```

### 3.2 generic_handle_irq() 软件注入

```
echo 1 > /proc/demo_irq_trigger
    ↓
demo_irq_trigger_write()
    → generic_handle_irq(priv->linux_irq)
    ↓
demo_irq_handler()         [top-half / hardirq 上下文]
    → atomic64_inc_return(&priv->irq_count)
    → dev_info_ratelimited()
    → return IRQ_HANDLED
```

### 3.3 几个常见误解

```
❌ 误解1：interrupts = <...> 写进 DT 就已经触发中断了
   → DT 里的 interrupts 只是"硬件描述"，不是"触发事件"

❌ 误解2：platform_get_irq() 返回 irq 号就代表中断发生了
   → 它只是把 DT 中断域翻译成 Linux virq 编号

❌ 误解3：request_irq() 成功就能在 /proc/interrupts 看到计数增长
   → 它只是注册了 handler，不代表 IRQ 会被触发

✅ 正确认知：
   触发前提 = 真实硬件事件 OR 软件注入
   generic_handle_irq() 可以在进程上下文模拟这个触发动作
```

---

## 四、完整执行路径

### 4.1 probe 阶段

```
insmod demo_irq.ko
    ↓
module_init
    → platform_driver_register()
    ↓
[DT 匹配成功]
    ↓
demo_irq_probe()
    → of_node 检查
    → devm_kzalloc() 分配 priv
    → demo_irq_dump_raw_reg()        [打印 DT 原始 reg cells]
    → demo_irq_dump_raw_irq()         [打印 DT 原始 interrupts cells]
    → platform_get_resource()         [解析 MEM 资源]
    → platform_get_irq()               [解析 Linux virq]
    → request_irq()                   [注册 top-half handler]
    → demo_irq_create_proc()          [创建 stats + trigger proc 节点]
    → g_demo_priv = priv
```

### 4.2 触发阶段

```
echo N > /proc/demo_irq_trigger
    ↓
demo_irq_trigger_write()
    → copy_from_user(kbuf)
    → kstrtouint(kbuf) → n
    → for (i=0; i<n; i++)
         generic_handle_irq(priv->linux_irq)
    ↓
[IRQ core 派发]
    → generic_handle_irq()
    → handle_fasteoi_irq()
    → handle_irq_event()
    → __handle_irq_event_percpu()
    → demo_irq_handler()
    → atomic64_inc_return(&priv->irq_count)
    → dev_info_ratelimited()
    → return IRQ_HANDLED
```

---

## 五、核心数据结构

### 5.1 struct demo_irq_priv

```
┌─────────────────────────────────────────────────────────────────────┐
│                    demo_irq_priv 核心字段                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  device *dev              → 关联设备对象，用于 dev_info/dev_err 日志  │
│  struct resource mem      → platform_get_resource() 解析出的 MEM    │
│  int linux_irq           → platform_get_irq() 解析出的 Linux virq  │
│  u32 raw_reg[4]          → DT 原始 reg cells（教学对照用）          │
│  u32 raw_irq[3]          → DT 原始 interrupts cells（教学对照用）   │
│  const char *label        → DT 自定义属性 demo,label 的值           │
│  const char *match_name   → of_match_table data 携带的匹配信息       │
│  atomic64_t irq_count    → 累计中断次数（原子变量，适合中断上下文） │
│  proc_dir_entry *proc_stats    → /proc/demo_irq_stats               │
│  proc_dir_entry *proc_trigger  → /proc/demo_irq_trigger             │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.2 为什么要用 atomic64_t irq_count？

```
中断上下文的特点：
  - 不能睡眠
  - 不能做复杂同步

atomic64_inc_return() 的优势：
  - 不需要加锁（原子指令）
  - 不需要睡眠
  - 适合简单计数场景

教学上可以理解为："一个适合在中断里直接加一的计数器"
```

---

## 六、两个 proc 节点

### 6.1 /proc/demo_irq_stats（只读）

```
cat /proc/demo_irq_stats

输出：
module=demo_irq
label=<DT 中 demo,label 的值>
linux_irq=49
irq_count=<当前累计值>
mem_start=0x...     ← platform_get_resource 解析出的物理地址
mem_size=0x...      ← resource_size
```

### 6.2 /proc/demo_irq_trigger（只写）

```
echo 1 > /proc/demo_irq_trigger    # 注入 1 次
echo 10 > /proc/demo_irq_trigger   # 注入 10 次

上限保护：n == 0 或 n > 100000 则返回 -EINVAL
```

### 6.3 为什么有两个视角？

```
/proc/interrupts          → 内核通用视角，看所有 IRQ 的全局状态
/proc/demo_irq_stats      → 驱动自身视角，看本驱动的私有数据

两者一起看，最容易建立直觉：
  - 一个代表"内核看到的"
  - 一个代表"驱动自己记录的"
```

---

## 七、top-half handler 分析

### 7.1 demo_irq_handler() 为什么极简？

```c
static irqreturn_t demo_irq_handler(int irq, void *dev_id)
{
    struct demo_irq_priv *priv = dev_id;
    s64 new_count;

    new_count = atomic64_inc_return(&priv->irq_count);

    dev_info_ratelimited(priv->dev, "top-half handled irq=%d count=%lld\n",
                         irq, (long long)new_count);
    return IRQ_HANDLED;
}
```

```
这个 handler 只做三件事：
  1. 计数 +1（atomic64_inc_return）
  2. 打一条限速日志（ratelimited，避免刷屏）
  3. 返回 IRQ_HANDLED

为什么不在这里做重活？
  - 中断上下文的执行时间要尽可能短
  - 不能睡眠
  - 后续 days（Day11）会把重活下沉到 workqueue bottom-half
```

### 7.2 为什么返回 IRQ_HANDLED？

```
IRQ_NONE         → 这个 IRQ 不是我的，不处理
IRQ_HANDLED      → 这个 IRQ 是我的，已处理
IRQ_WAKE_THREAD  → 唤醒 threaded handler（今天不讲）

在这个 fake 设备实验里，每次触发都是"我们的 IRQ"，
所以返回 IRQ_HANDLED。
```

---

## 八、Day10 与 Day11 的关系

### 8.1 Day10 是 Day11 的基础

```
Day10 建立了：
  - request_irq() 成功
  - top-half handler 能被执行
  - irq_count 能被计数

Day11 在此基础上加入：
  - queue_work() 把重活下沉到 workqueue
  - worker thread 承接 msleep() 模拟的重活
  - batch 合并（多次 IRQ 合并到一次 worker 处理）
  - 粗略延迟统计
```

### 8.2 Day10 是 Day13 的铺垫

```
Day13 用 ftrace function_graph 追踪的 IRQ 路径：
  trigger_write()
    → generic_handle_irq()
    → handle_fasteoi_irq()
    → handle_irq_event()
    → demo_regmap_handler()      ← Day12/13 的 top-half

这条路径的"前半段"（generic_handle_irq → handler）
在 Day10 就已经建立好了。
```

---

## 九、面试要会讲的五句话

1. **"Day10 的核心是把 Day09 解析出的 DT 中断资源，通过 platform_get_irq() 取到 Linux virq，再通过 request_irq() 注册 top-half handler，最后用软件注入让 handler 被真正执行"**
   → 理解 Day10 的目标

2. **"DT 里的 interrupts = <...> 只是硬件描述，不代表中断已经触发；platform_get_irq() 只是把 DT 中断域翻译成 Linux virq；request_irq() 只是注册了回调——这三者都不是触发本身"**
   → 理解三个常见误解

3. **"因为 QEMU virt 的 DT 节点是教学 fake 设备，没有真实外设拉中断线，所以需要 /proc/demo_irq_trigger 做软件注入：generic_handle_irq() 可以在进程上下文模拟软中断触发"**
   → 理解软件触发机制的必要性

4. **"top-half 应该保持极简：只做计数、记时间戳、queue_work()，不做重活；因为中断上下文执行时间要尽可能短、不能睡眠；Day10 的 demo_irq_handler() 只做了 atomic64_inc_return + ratelimited 日志"**
   → 理解 top-half 最小化原则

5. **"Day10 和 Day11 的关系是：Day10 建立'IRQ 能进来 + handler 能执行'，Day11 在此基础上加入'bottom-half 承接重活'；Day10 和 Day13 的关系是：Day13 追踪的 IRQ 派发链，其前半段在 Day10 就已建立"**
   → 理解 Day10 在 W2 中的位置

---

## 十、验收标准

### 10.1 probe 验收

- [ ] insmod demo_irq.ko 成功
- [ ] dmesg | grep demo_irq 显示：
  - `probe start`
  - `raw DT reg cells`
  - `raw DT interrupts cells`
  - `parsed MEM resource`
  - `parsed Linux IRQ`
  - `request_irq done`

### 10.2 /proc/interrupts 验收

- [ ] `cat /proc/interrupts | grep demo_irq` 能看到一行
- [ ] 刚加载时 irq_count 应该是 0

### 10.3 软件触发验收

- [ ] `echo 1 > /proc/demo_irq_trigger` 不报错
- [ ] 触发后 dmesg 有 `top-half handled irq=x count=1` 日志
- [ ] 再次 `cat /proc/interrupts | grep demo_irq`，计数增加

### 10.4 双重统计验收

- [ ] `cat /proc/demo_irq_stats` 输出模块名、linux_irq、irq_count
- [ ] irq_count 和 /proc/interrupts 显示的计数变化一致
- [ ] `echo 10 > /proc/demo_irq_trigger` 后，irq_count 增加到 11

---

## 附录：完整验收命令

```
# 1. 加载模块
insmod /demo_irq.ko
dmesg | grep demo_irq

# 2. 确认 proc 节点存在
ls -la /proc/demo_irq_*

# 3. 触发前看两个视角的初始状态
cat /proc/interrupts | grep demo_irq
cat /proc/demo_irq_stats

# 4. 软件注入触发
echo 1 > /proc/demo_irq_trigger
dmesg | grep demo_irq

# 5. 触发后再看两个视角
cat /proc/interrupts | grep demo_irq
cat /proc/demo_irq_stats

# 6. 批量触发
echo 10 > /proc/demo_irq_trigger
cat /proc/demo_irq_stats   # irq_count 应该 = 11

# 7. 卸载模块
rmmod demo_irq
dmesg | grep demo_irq      # remove 日志应显示最终 irq_count
```
