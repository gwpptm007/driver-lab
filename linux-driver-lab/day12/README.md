# Day12 - regmap 封装寄存器 + debugfs 输出寄存器快照

## 1. 本课目标

day11 已经把这条链路跑通：

- Device Tree 生成 platform_device
- platform_driver 通过 `of_match_table` 匹配
- `platform_get_irq()` 解析出 Linux virq
- `request_irq()` 注册 top-half
- `workqueue` 作为 bottom-half 承担重活
- `/proc` 导出粗略延迟统计

Day12 在这个基础上继续往前走一步：

- 用 **regmap** 统一封装“寄存器视图”
- 用 **debugfs** 导出寄存器快照
- 提供一个简单的写接口，验证 **regmap 读写路径** 都能跑通

这次不追求真实 MMIO，而是优先把“寄存器抽象”和“可观测性”讲清楚。

---

## 2. 为什么这次使用“软件后端 regmap”

当前 QEMU `virt` 上注入的教学 DT 节点，本质上还是 fake 设备：

- `reg` 可以解析
- `interrupts` 可以解析
- `platform_driver` 能正常匹配
- `request_irq()` / top-half / workqueue 也都能跑通

但这个 fake 节点并不一定真的背后就有一块适合你直接 `ioremap + readl/writel` 的硬件寄存器空间。

所以 Day12 采用一个更稳的教学方案：

- 驱动内部维护一块 `u32 regs[]` 阴影寄存器数组
- 通过 `regmap` 回调把它封装成寄存器空间
- 外部统一使用 `regmap_read()` / `regmap_write()` 访问

这样既能学到：

- regmap 配置
- 可读/可写寄存器约束
- debugfs 快照导出
- 读写路径验证

又不会被 fake MMIO 的不确定性干扰。

---

## 3. 目录说明

```text
day12/
├── Makefile
├── build.sh
├── prereadme.md
├── README.md
├── demo_regmap.c
├── demo_regmap.fragment.dtsi
└── inject_virt_dt.py
```

---

## 4. 寄存器地图

本实验把 day11 的运行态统计组织成下面这组寄存器：

| 偏移 | 名称 | 含义 | 访问属性 |
|---|---|---|---|
| `0x00` | `CTRL` | 控制寄存器，当前使用 bit0 表示是否允许触发 | RW |
| `0x04` | `STATUS` | 状态寄存器，反映 enable/pending/busy | RO |
| `0x08` | `IRQ_COUNT` | top-half 总触发次数 | RO |
| `0x0c` | `WORK_RUNS` | worker 实际运行次数 | RO |
| `0x10` | `WORK_ITEMS` | worker 实际处理事件总数 | RO |
| `0x14` | `PENDING_EVENTS` | 当前待处理事件数 | RO |
| `0x18` | `LAST_BATCH` | 最近一轮 worker 处理的 batch 大小 | RO |
| `0x1c` | `LAST_LATENCY_US` | 最近一次粗略延迟（微秒） | RO |
| `0x20` | `MAX_LATENCY_US` | 最大粗略延迟（微秒） | RO |
| `0x24` | `AVG_LATENCY_US` | 平均粗略延迟（微秒） | RO |
| `0x28` | `WORK_MS` | worker 模拟重活时长（毫秒） | RW |
| `0x2c` | `VERSION` | 教学版本号 | RO |

---

## 5. 代码执行路径

```text
echo 5 > /sys/kernel/debug/demo_regmap/trigger
    -> generic_handle_irq(linux_irq)
    -> demo_regmap_handler()            [top-half / hardirq context]
         -> irq_count++
         -> pending_events++
         -> queue_work(priv->wq, &priv->work)
         -> return IRQ_HANDLED

worker thread 被调度
    -> demo_regmap_workfn()             [process context]
         -> 取出 batch = pending_events
         -> 更新 work_runs / work_items
         -> 计算 last/max/avg latency
         -> msleep(work_delay_ms) 模拟重活

cat /sys/kernel/debug/demo_regmap/snapshot
    -> 刷新 shadow regs[]
    -> regmap_read() 逐个读取寄存器
    -> 输出寄存器快照
```

---

## 6. debugfs 节点说明

模块加载后，会创建：

```text
/sys/kernel/debug/demo_regmap/
├── snapshot
├── poke
└── trigger
```

### 6.1 `snapshot`
只读，打印一份寄存器快照。

例如：

```bash
cat /sys/kernel/debug/demo_regmap/snapshot
```

### 6.2 `poke`
只写，用于写寄存器，格式：

```bash
echo "<reg> <val>" > /sys/kernel/debug/demo_regmap/poke
```

示例：

```bash
echo "0x28 50" > /sys/kernel/debug/demo_regmap/poke
```

这表示把 `WORK_MS` 改成 `50ms`。

### 6.3 `trigger`
只写，用于软件方式触发 fake IRQ，格式：

```bash
echo 1 > /sys/kernel/debug/demo_regmap/trigger
echo 10 > /sys/kernel/debug/demo_regmap/trigger
```

它本质上会调用 `generic_handle_irq()`，从而把：

- top-half
- workqueue
- 统计更新
- regmap 视图刷新

整条教学链路跑起来。

---

## 7. 代码里的关键设计点

### 7.1 top-half 仍然保持最小化

`demo_regmap_handler()` 继续保持“day11 的原则”：

- 记录 IRQ 次数
- 记录时间戳
- 增加 pending 数
- `queue_work()`
- 立刻返回

也就是说：

> Day12 虽然引入了 regmap，但**没有把重活重新塞回中断上下文**。

### 7.2 worker 负责重活和延迟统计

`demo_regmap_workfn()` 中继续做这些事情：

- 取走一批 pending 事件
- 计算粗略延迟
- 更新 `work_runs` / `work_items`
- `msleep(work_delay_ms)` 模拟重活

这样就能继续验证：

- 重活在 bottom-half 中完成
- 延迟统计仍然有效

### 7.3 regmap 负责统一“寄存器读写入口”

这次 `regmap` 不是拿来访问真实硬件，而是拿来统一访问驱动维护的 `regs[]`：

- `regmap_read()`：读阴影寄存器
- `regmap_write()`：写可写控制寄存器
- `debugfs snapshot`：通过 `regmap_read()` 导出快照
- `debugfs poke`：通过 `regmap_write()` 修改控制寄存器

这就是本课最关键的“统一访问抽象”。

---

## 8. 执行 `build.sh` 前的环境准备

Day12 的 `build.sh` 支持“外部环境变量覆盖 + 脚本内默认值”两种方式：

```bash
export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-
```

如果你本机目录布局正好和脚本默认值一致，那么**理论上不手工 `export` 也可以运行**。  
但从日常测试和排错角度，我仍然建议在进入 `day12/` 后先显式导出这 3 个变量，再执行 `./build.sh`。

### 8.1 这三个变量分别代表什么

- `KERNEL_DIR`：内核源码路径。只要脚本需要编译模块、取内核镜像、取构建输出目录，就可能要用到它。它是**路径问题**，不是 ARM 专属。
- `BUSYBOX_DIR`：BusyBox 工程路径。脚本需要从这里取出用于 guest rootfs 的 BusyBox 安装目录。它也是**路径问题**，不是 ARM 专属。
- `CROSS_COMPILE`：交叉编译器前缀。它主要和**宿主机架构是否与目标架构一致**有关。

### 8.2 什么时候更建议显式配置

可以记成一句话：

> ARM 场景里更常见三者都要配；非 ARM 场景下，前两个可能仍然要配，第三个则看你是不是交叉编译。

更具体地说：

#### 场景 A：x86_64 Ubuntu 主机，目标是 arm64 QEMU（本实验当前就是这个场景）
建议显式导出这三项：

```bash
export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-
```

因为这时你同时涉及：

- 外部内核源码路径
- 外部 BusyBox 路径
- x86 -> arm64 的交叉编译

#### 场景 B：宿主机和目标机都是同一架构，例如 x86_64 原生编 x86_64
这时：

- `KERNEL_DIR` 仍然可能要配
- `BUSYBOX_DIR` 仍然可能要配
- `CROSS_COMPILE` 一般不一定要配

#### 场景 C：arm64 机器原生编 arm64
这时：

- `KERNEL_DIR` / `BUSYBOX_DIR` 仍取决于你的实际目录
- `CROSS_COMPILE` 通常不一定需要

### 8.3 本实验推荐执行方式

在你当前 Day12 测试环境下，推荐固定按下面方式执行：

```bash
export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-

cd /home/wq7/workspace/driver-lab/linux-driver-lab/day12
./build.sh
```

这样更稳，也更容易在脚本输出里确认自己最终使用的是哪套内核、BusyBox 和工具链。

---

## 9. 如何编译和启动

进入 `day12/` 后执行：

```bash
./build.sh
```

这个脚本会完成：

1. 编译 `demo_regmap.ko`
2. 构造最小 rootfs
3. 把模块放到 guest 根目录
4. 导出 QEMU `virt` 的基础 DTB/DTS
5. 注入 `demo_regmap.fragment.dtsi`
6. 启动 arm64 QEMU

---

## 10. guest 内测试步骤

### 10.1 加载模块

```bash
insmod /demo_regmap.ko
```

你应该先看到类似下面的日志，说明模块加载、`probe()`、debugfs 节点创建都成功了：

```text
demo_regmap: module init
demo_regmap 10002000.demo_regmap: probe begin
demo_regmap 10002000.demo_regmap: raw DT reg cells: <0x0 0x10002000 0x0 0x1000>
demo_regmap 10002000.demo_regmap: raw DT interrupts cells: <0x0 0x7a 0x4>
demo_regmap 10002000.demo_regmap: probe ok: label=regmap-demo match=demo,regmap-pdrv ...
demo_regmap 10002000.demo_regmap: debugfs: /sys/kernel/debug/demo_regmap/{snapshot,poke,trigger}
```

### 10.2 查看 debugfs 快照，验证 regmap 读路径

```bash
cat /sys/kernel/debug/demo_regmap/snapshot
```

第一次读取时，寄存器快照应大致类似：

```text
module=demo_regmap
label=regmap-demo
match_name=demo,regmap-pdrv
linux_irq=49
raw_reg=<0x0 0x10002000 0x0 0x1000>
raw_irq=<0x0 0x7a 0x4>
----------------------------------------
CTRL             reg=0x00 val=0x00000001 (1)
STATUS           reg=0x04 val=0x00000004 (4)
IRQ_COUNT        reg=0x08 val=0x00000000 (0)
WORK_RUNS        reg=0x0c val=0x00000000 (0)
WORK_ITEMS       reg=0x10 val=0x00000000 (0)
PENDING_EVENTS   reg=0x14 val=0x00000000 (0)
LAST_BATCH       reg=0x18 val=0x00000000 (0)
LAST_LATENCY_US  reg=0x1c val=0x00000000 (0)
MAX_LATENCY_US   reg=0x20 val=0x00000000 (0)
AVG_LATENCY_US   reg=0x24 val=0x00000000 (0)
WORK_MS          reg=0x28 val=0x00000014 (20)
VERSION          reg=0x2c val=0x00001200 (4608)
```

这一步的意义是：

- `snapshot` 节点存在且可读
- `demo_regmap_refresh_view()` 已把运行态刷新到 shadow regs[]
- `regmap_read()` 逐项把寄存器值读了出来

也就是说，**regmap 读路径已经跑通**。

### 10.3 触发几次 fake IRQ，验证 top-half / worker / 统计联动

```bash
echo 5 > /sys/kernel/debug/demo_regmap/trigger
```

正常情况下，触发后应能在日志里看到 top-half 和 worker 的输出，例如：

```text
demo_regmap ...: top-half irq=49 irq_count=1 pending=1
demo_regmap ...: worker start batch=1 latency=178 us pending_now=0 work_ms=20
```

再次查看快照：

```bash
cat /sys/kernel/debug/demo_regmap/snapshot
```

此时应能观察到：

- `IRQ_COUNT` 增长
- `WORK_RUNS` 增长
- `WORK_ITEMS` 增长
- `LAST_LATENCY_US` / `MAX_LATENCY_US` / `AVG_LATENCY_US` 变化

### 10.4 验证 regmap 写路径

把 worker 模拟耗时从 20ms 改成 50ms：

```bash
echo "0x28 50" > /sys/kernel/debug/demo_regmap/poke
cat /sys/kernel/debug/demo_regmap/snapshot
```

日志会显示：

```text
demo_regmap ...: poke reg=0x28 val=0x32 ok
```

而快照里应能看到：

```text
WORK_MS          reg=0x28 val=0x00000032 (50)
```

这说明：

- `poke` 已正确解析寄存器地址和写入值
- `regmap_write()` 路径正常
- 结果可通过 `snapshot` 的 `regmap_read()` 再次读回

也就是说，**regmap 写路径也已跑通**。

### 10.5 继续触发，观察处理节奏变化

```bash
echo 10 > /sys/kernel/debug/demo_regmap/trigger
cat /sys/kernel/debug/demo_regmap/snapshot
```

此时由于 `WORK_MS` 变大，worker 的处理节奏和粗略延迟数据也会随之变化。

---

## 11. 验收标准

### 验收 1
模块正常加载，probe 成功，debugfs 目录可见。

### 验收 2
`snapshot` 能正常导出一组寄存器值。

### 验收 3
通过 `poke` 写 `CTRL` 或 `WORK_MS`，再读 `snapshot`，可以看到写入结果。

### 验收 4
触发 fake IRQ 后，快照中这些值会变化：

- `IRQ_COUNT`
- `WORK_RUNS`
- `WORK_ITEMS`
- `PENDING_EVENTS`
- `LAST_BATCH`
- `LAST_LATENCY_US`
- `MAX_LATENCY_US`
- `AVG_LATENCY_US`

这说明：

> regmap 不是一个“死数组”，而是已经和 day11 的运行态真正连上了。

---

## 12. 问题分析：`trigger` 路径上的 call trace 与 warning 解释

### 12.1 实际现象

在当前实现中，如果执行：

```bash
echo 5 > /sys/kernel/debug/demo_regmap/trigger
```

你可能会看到类似下面的 warning：

```text
irq 49 handler demo_regmap_handler+0x0/0x1b8 [demo_regmap] enabled interrupts
WARNING: CPU: 0 PID: 1 at kernel/irq/handle.c:159 __handle_irq_event_percpu+0x138/0x170
```

同时 call trace 大致如下：

```text
__handle_irq_event_percpu
handle_irq_event
handle_fasteoi_irq
generic_handle_irq
demo_regmap_trigger_write [demo_regmap]
full_proxy_write
vfs_write
ksys_write
__arm64_sys_write
invoke_syscall
el0_svc_common.constprop.0
do_el0_svc
el0_svc
el0t_64_sync_handler
el0t_64_sync
```

### 12.2 这段栈到底说明了什么

这条栈的完整含义是：

> 用户态执行 `echo 5 > /sys/kernel/debug/demo_regmap/trigger`  
> -> 进入 `write()` 系统调用  
> -> VFS 调到 debugfs 的 `.write`  
> -> 进入 `demo_regmap_trigger_write()`  
> -> 它里面调用 `generic_handle_irq(priv->linux_irq)`  
> -> IRQ core 按真正中断的处理路径去调度这个 IRQ  
> -> 最终执行到 `demo_regmap_handler()`  
> -> 但由于这次调用发生在“普通进程上下文”，而不是“真实硬中断现场”，IRQ core 检查到上下文不满足 hardirq 约束，于是在 `__handle_irq_event_percpu()` 报 warning。

也就是说，**这条栈是在告诉你：软件触发 fake IRQ 时，真正走进了 IRQ core，而不是简单地直接调用你的 handler。**

### 12.3 每一层栈帧在做什么

#### `el0t_64_sync` / `el0t_64_sync_handler` / `el0_svc` / `do_el0_svc`
这是 arm64 从用户态进入内核态的系统调用入口。说明整个过程的起点只是一次普通的用户态 `write()`。

#### `__arm64_sys_write` / `ksys_write` / `vfs_write`
这是标准的 Linux 写文件路径。说明当前上下文本质上是：

- 一个普通进程
- 正在向 debugfs 文件写数据
- 还不是硬中断上下文

#### `full_proxy_write`
这是 debugfs 文件写操作的包装层，本质上是把 VFS 写请求继续转交给你的 `.write` 回调。

#### `demo_regmap_trigger_write`
这是你的模块中 `trigger` 节点的写函数。当前 Day12 的关键逻辑就在这里：

```c
for (i = 0; i < times; i++)
    generic_handle_irq(priv->linux_irq);
```

也就是说，这里不是“直接调 `demo_regmap_handler()`”，而是调用 IRQ core 的通用入口。

#### `generic_handle_irq`
这个函数的作用是：按 Linux IRQ 子系统的规则，处理一次指定的 Linux IRQ 号。

它会进一步找到：

- 这个 IRQ 对应的 `irq_desc`
- 对应的 flow handler
- 挂在这个 IRQ 上的 action 链表

然后再去调用真正的 handler。

#### `handle_fasteoi_irq`
这说明当前这个 IRQ 在 arm64 + GIC 环境里走的是 `fasteoi` 类型的 flow handler。  
这不是错误，恰恰说明已经进入了标准 IRQ core 分发路径。

#### `handle_irq_event`
这层可以理解为：已经准备好去执行这个 IRQ 上注册的具体 handler 了。

#### `__handle_irq_event_percpu`
这是最终调用你的 `demo_regmap_handler()` 的核心位置。  
warning 也正是在这里报出来的。

### 12.4 为什么会出现 `handler enabled interrupts`

这个文案很容易让人误以为：

> “你的 handler 里面手工开了中断。”

但对当前这个案例，更准确的理解是：

> IRQ core 期望 hardirq handler 运行在“像硬中断一样”的约束环境中。  
> 但你是从 debugfs 的 `.write` 路径里，在普通进程上下文直接调用了 `generic_handle_irq()`。  
> 这个调用现场与真实硬中断现场不一样，于是 IRQ core 在一致性检查时发现：handler 执行所处的上下文不满足预期，因此报出 `handler enabled interrupts` warning。

所以这里的问题**不在 regmap，不在 worker，也不在统计逻辑本身**，而在于：

> `trigger` 的 fake IRQ 触发方式，不够像真实的硬中断现场。

### 12.5 为什么功能仍然基本是通的

虽然出现了 warning，但你最终仍能看到：

- `IRQ_COUNT = 5`
- `WORK_RUNS = 2`
- `WORK_ITEMS = 5`
- `LAST_BATCH = 4`
- `LAST/MAX/AVG LATENCY` 都有值

这说明：

- top-half 确实被调到了
- worker 也确实跑了
- regmap / debugfs 的主线仍然是正常的

也就是说，**Day12 的核心目标已经完成**，warning 只是暴露了 `trigger` 路径的上下文模拟细节问题。

### 12.6 为什么 `WORK_RUNS` 不一定等于 `IRQ_COUNT`

这次实测里：

- `IRQ_COUNT = 5`
- `WORK_RUNS = 2`
- `WORK_ITEMS = 5`

这是正常现象，不是 bug。

因为当前只用了一个 `work_struct`：

```c
queue_work(priv->wq, &priv->work);
```

当 worker 已经排队或正在执行时，后续多次 `queue_work()` 不会无限拆成多个独立 worker 实例。  
结果就是：

- 第一次 IRQ 触发时，worker 很快跑一轮，`batch = 1`
- 后续 4 次 IRQ 叠加成 pending
- 下一轮 worker 把这 4 个事件一次处理掉，`batch = 4`

所以最终呈现为：

- `IRQ_COUNT = 5`
- `WORK_RUNS = 2`
- `WORK_ITEMS = 5`

这正好体现了 day11/day12 中“top-half 次数”和“worker 运行轮数”不一定 1:1 对应的教学点。

### 12.7 后续如何修正这个 warning

最小修法是在 `demo_regmap_trigger_write()` 里让 fake IRQ 的调用现场更接近真实 hardirq：

```c
for (i = 0; i < times; i++) {
    unsigned long flags;

    local_irq_save(flags);
    generic_handle_irq(priv->linux_irq);
    local_irq_restore(flags);
}
```

这样做的目的不是改变 regmap 主线，而是：

- 让 `trigger` 的软件触发方式更接近真实中断进入 handler 时的上下文
- 避免 IRQ core 的一致性检查报出 `handler enabled interrupts` warning

### 12.8 这一节最值得记住的结论

这次问题分析最值得记住的不是某一个栈帧名字，而是下面这句话：

> `generic_handle_irq()` 不是简单的 helper，而是真正会进入 IRQ core 的通用入口。  
> 因此在普通进程上下文里用它模拟硬中断时，必须考虑 hardirq 的上下文约束；否则即使功能表面能跑通，也可能在 IRQ core 的一致性检查阶段报 warning。

---

## 13. 一句话总结

Day12 的核心成果不是“做了一个更复杂的 demo”，而是：

> 把 day11 的运行时状态进一步整理成一组统一的寄存器视图，用 regmap 作为读写抽象层，并通过 debugfs 快照把它可视化出来。

同时，这次实测还补上了一个很重要的工程经验：

> 软件方式触发 fake IRQ 时，不能只看“功能是否跑通”，还要看调用现场是否满足 IRQ core 对 hardirq 上下文的约束。
