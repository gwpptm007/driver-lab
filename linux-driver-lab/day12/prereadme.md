# Day12 Pre-README：为什么要引入 regmap，为什么要做 debugfs 寄存器快照

## 1. 这一课在整个链路里的位置

前面几天你已经把一条很重要的 platform/DT/IRQ 主线跑起来了：

- Day09：Device Tree 节点匹配、`reg` / `interrupts` 解析
- Day10：`request_irq()`、top-half、`/proc/interrupts` 计数可见
- Day11：把重活下沉到 `workqueue`，并记录从 top-half 到 worker 开始执行的粗略延迟

所以 Day12 再往前走时，关注点就不再是“中断能不能来、worker 能不能跑”，而是：

> 驱动内部这些运行时状态，能不能被组织成一组更像“寄存器”的统一视图？

这正是 regmap 适合切入的地方。

---

## 2. 为什么要学 regmap

### 2.1 当前状态还是“变量视角”

到了 day11，驱动内部已经有了不少状态：

- `irq_count`
- `work_runs`
- `work_items`
- `pending_events`
- `last_latency_ns`
- `max_latency_ns`
- `latency_samples`

这些状态当然能直接通过原子变量和 `/proc` 文本导出。

但如果继续这样往前堆，很容易出现一个问题：

**状态很分散，读写路径也很分散。**

这时你会发现：

- 某些值在 top-half 里更新
- 某些值在 worker 里更新
- 某些值在 `/proc` 读取时临时拼出来
- 某些控制项又用模块参数单独管理

从“能跑”角度看没有问题；但从“驱动抽象”角度看，还不够规整。

---

### 2.2 regmap 的核心价值

regmap 的价值，不只是“帮你读写某条总线上的寄存器”。

更重要的是：

**它提供了一套统一的寄存器访问抽象。**

也就是说，驱动可以不再到处散着读写状态，而是通过统一入口：

- `regmap_read()`
- `regmap_write()`
- `regmap_update_bits()`

来操作一组“寄存器视图”。

因此 Day12 学的重点不是“某个 API 的背诵”，而是下面这个思维：

> 把驱动状态组织成一张寄存器表，再通过统一接口访问它。

---

## 3. 为什么这次不急着上“真实 MMIO”

这一步很关键。

你当前 day09/day10/day11 的 DT 节点，本质上是一个教学 fake 设备：

- DT 里能写 `reg`
- 内核也确实能解析 `reg`
- DT 里也能写 `interrupts`
- `platform_driver` 也能正常 `probe()`、`request_irq()`

但这里的 fake 节点，**并不一定真的背后就有一块适合你随便访问的硬件寄存器空间**。

所以如果 Day12 直接跳到：

- `devm_ioremap_resource()`
- `regmap-mmio`
- 真实 `readl()/writel()`

那在当前这个教学环境里，反而容易把问题变成：

> 这个 fake 节点后面到底有没有安全、真实、可访问的 MMIO 设备？

这不是 Day12 最应该解决的问题。

---

## 4. 因此，Day12 最稳的方案是什么

最稳、也最适合教学的方案是：

### 保留 day11 的平台骨架

也就是继续沿用：

- `platform_driver`
- `of_match_table`
- DT 节点
- `request_irq()`
- top-half
- workqueue bottom-half

### 但把“寄存器后端”先做成软件实现

也就是：

- 驱动内部准备一块 `u32 regs[]` 阴影寄存器数组
- 用 regmap 的回调把这块数组封装成“寄存器空间”
- 外部统一通过 `regmap_read()/regmap_write()` 来访问

这样你学到的东西其实一点不少：

- regmap 配置怎么写
- 哪些寄存器可读、哪些可写
- 寄存器步进怎么定义
- 如何把运行时状态同步成寄存器视图
- debugfs 怎样把寄存器快照导出来

同时又不会引入“假 MMIO 真访问”的额外不确定性。

---

## 5. Day12 的核心任务拆解

你这次的任务是：

- regmap 封装寄存器
- debugfs 输出寄存器快照
- 验收：regmap 读写路径跑通

把它翻成驱动设计语言，可以拆成下面 4 步。

### 第一步：定义寄存器视图

先把 day11 的运行时状态“寄存器化”。

最典型的一组寄存器可以是：

- `CTRL`
- `STATUS`
- `IRQ_COUNT`
- `WORK_RUNS`
- `WORK_ITEMS`
- `PENDING_EVENTS`
- `LAST_BATCH`
- `LAST_LATENCY_US`
- `MAX_LATENCY_US`
- `AVG_LATENCY_US`
- `WORK_MS`
- `VERSION`

这样以后再看驱动，你就不再只是看到一堆散变量，而是能从“寄存器地图”角度理解它。

---

### 第二步：把 regmap 后端接起来

这里不用真实总线，而是使用软件后端：

- `reg_read()`：从 `regs[]` 里取值
- `reg_write()`：写回 `regs[]`
- `regmap_config`：定义 `reg_bits / val_bits / reg_stride / max_register`
- `readable_reg()` / `writeable_reg()`：限制可访问范围

这样就形成了一条真正的 regmap 路径：

```text
用户或驱动逻辑
-> regmap_read / regmap_write
-> regmap callback
-> shadow regs[]
```

这就是这节课最核心的“读写路径跑通”。

---

### 第三步：把运行态统计刷新成寄存器视图

regmap 本身只是一层访问抽象。

而 day11 的那些状态仍然主要在这些地方产生：

- top-half：更新 IRQ 计数、pending、时间戳
- worker：更新 work_runs、work_items、latency

所以 Day12 很重要的一步是：

> 在合适的时候，把这些运行态统计刷新到 shadow regs[] 里。

常见做法就是在：

- `debugfs snapshot` 读取前
- worker 完成一轮处理后

进行一次同步。

这就把“真实运行状态”和“寄存器视图”连起来了。

---

### 第四步：通过 debugfs 导出快照

这一步的目标非常明确：

> 让寄存器视图可观测。

所以 Day12 不只是“有 regmap”，还要能看到 regmap 里的东西。

最适合教学的方式就是：

- 建一个 debugfs 目录
- 放一个 `snapshot` 文件
- 读这个文件时，打印所有关键寄存器

例如：

```text
CTRL            0x00000001
STATUS          0x00000004
IRQ_COUNT       0x0000000a
WORK_RUNS       0x00000003
WORK_ITEMS      0x0000000a
PENDING_EVENTS  0x00000000
LAST_BATCH      0x00000004
LAST_LATENCY_US 0x00000123
MAX_LATENCY_US  0x00000456
AVG_LATENCY_US  0x00000222
WORK_MS         0x00000014
VERSION         0x00001200
```

这个输出会让“寄存器模型”瞬间变得直观。

---

## 6. 为什么是 debugfs，而不是继续只用 procfs

`procfs` 更适合拿来做系统视图、进程视图、兼容性更强的导出接口。

而 `debugfs` 的定位更像是：

**给驱动开发者和调试阶段使用的观察窗口。**

Day12 这次需要的正好就是这种能力：

- 可以临时导出调试状态
- 可以打印教学用快照
- 可以增加一个简单的写接口（比如 poke 某个寄存器）
- 不需要把它设计成稳定 ABI

所以它很适合放到这节课里。

---

## 7. Day12 里最重要的边界意识

### 7.1 top-half 仍然不应该做重活

进入 Day12 后，不代表 day11 的原则就失效了。

你仍然要保持：

- top-half 很短
- 真正耗时逻辑在 worker
- 不要在硬中断里做复杂 regmap 访问链路

也就是说，Day12 只是把“状态组织方式”升级了，**不是把中断设计原则推翻了**。

---

### 7.2 regmap 不等于必须在中断里使用

这一点尤其值得记住。

有些 regmap 后端可能会睡眠，有些 regmap 访问链路也更适合进程上下文。

所以对于你这个实验来说，更合理的思路是：

- top-half 只做必要记账
- worker 和 debugfs 读取路径再去刷新/读取寄存器视图

这个边界如果建立起来，后面你写真实驱动也会更稳。

---

## 8. Day12 的主线流程图

可以把这节课先脑补成下面这条链路：

```text
echo 5 > /sys/kernel/debug/demo_regmap/trigger
    -> generic_handle_irq(linux_irq)
    -> demo_regmap_handler()          [top-half]
         -> irq_count++
         -> pending_events++
         -> queue_work()

worker 被调度
    -> demo_regmap_workfn()           [process context]
         -> 更新 work_runs / work_items / latency
         -> 模拟重活

cat /sys/kernel/debug/demo_regmap/snapshot
    -> 刷新 shadow regs[]
    -> regmap_read() 逐个读取寄存器
    -> 输出寄存器快照
```

这条路径把 day11 的执行链路和 day12 的寄存器抽象串在了一起。

---

## 9. 这节课的验收应该怎么看

你给的验收是：

> regmap 读写路径跑通

这句话可以拆成更具体的 4 条。

### 验收 1：模块正常加载

- DT 匹配成功
- `probe()` 成功
- regmap 初始化成功
- debugfs 目录创建成功

### 验收 2：读路径跑通

可以正常执行：

```bash
cat /sys/kernel/debug/demo_regmap/snapshot
```

并看到一组清晰的寄存器快照。

### 验收 3：写路径跑通

可以通过 debugfs 写控制寄存器，例如：

```bash
echo "0x28 50" > /sys/kernel/debug/demo_regmap/poke
```

再读一次 `snapshot`，能看到 `WORK_MS` 已经变化。

### 验收 4：寄存器视图和运行态联动

触发几次 IRQ 之后，再读 `snapshot`，能看到：

- `IRQ_COUNT`
- `WORK_RUNS`
- `WORK_ITEMS`
- `PENDING_EVENTS`
- `LAST_LATENCY_US`
- `MAX_LATENCY_US`

这些值确实在变化。

这就说明：

> 不是只做了一个“死寄存器数组”，而是把驱动运行状态真正映射成了 regmap 视图。

---

## 10. Day12 的一句话总结

如果用一句话概括这节课，那就是：

> 在 day11 已经跑通 top-half/workqueue 的基础上，day12 引入 regmap 作为统一寄存器访问抽象，并用 debugfs 输出寄存器快照，把驱动运行时状态组织成可读、可写、可观察的“寄存器视图”。

这就是 Day12 最值得学会的东西。

---

## 11. 执行 build.sh 前的环境变量准备（补充理解）

Day12 的 `build.sh` 支持“脚本内默认值 + 外部环境变量覆盖”。

最常见的 3 个变量是：

```bash
export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-
```

这里可以记成一句话：

> ARM 场景里更常见三者都要配；非 ARM 场景下，前两个可能仍然要配，第三个则看你是不是交叉编译。

原因分别是：

- `KERNEL_DIR`：内核源码路径，属于路径问题，不是 ARM 专属
- `BUSYBOX_DIR`：BusyBox 工程路径，属于路径问题，不是 ARM 专属
- `CROSS_COMPILE`：交叉编译器前缀，和宿主机架构、目标架构是否一致关系最大

对当前实验环境（x86_64 Ubuntu 主机 + arm64 QEMU guest）来说，建议每次都显式导出这三项，再执行 `./build.sh`。

---

## 12. 本节还有一个很值得学的副产物：call trace 分析

在当前 Day12 实测中，`trigger` 节点通过 `generic_handle_irq()` 软件模拟 IRQ 时，可能出现：

- `handler enabled interrupts`
- `WARNING at __handle_irq_event_percpu`

这并不说明 regmap 或 worker 主线失败了，而是说明：

> 你是从 debugfs 的 `.write` 路径里，在普通进程上下文调用了 `generic_handle_irq()`，IRQ core 会把它当成真正的 IRQ 分发路径来检查，因此会暴露 hardirq 上下文约束问题。

这部分在正式 `README.md` 的“问题分析”小节里会展开到逐层 call trace 解释，适合后面回顾时结合源码一起看。
