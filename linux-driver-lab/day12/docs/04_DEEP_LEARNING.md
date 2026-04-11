# Day12 深度指南 - regmap 封装寄存器 + debugfs 寄存器快照

## 一、Day12 是什么？

Day12 是 W2（嵌入式驱动模式）的第五天，定位是**regmap 统一寄存器抽象 + debugfs 可观测性**。

**核心目标**：把 Day11 的运行时状态组织成"寄存器视图"，用 regmap 作为统一读写抽象层，通过 debugfs 导出寄存器快照。

Day12 不追求真实 MMIO。它的重点是：
1. **软件后端 regmap**：驱动内部维护 `u32 regs[]` 阴影寄存器，regmap 封装成统一访问接口
2. **debugfs 快照**：snapshot/poke/trigger 三个 debugfs 节点
3. **regmap 读写路径跑通**：验证读路径（snapshot）和写路径（poke）都正常

---

## 二、W2 学习路径中的位置

### 2.1 W2 整体架构

```
W2 (嵌入式驱动模式 - day08-14)
├── day08: platform_driver + DT 注入
├── day09: IRQ handler 注册
├── day10: regmap 框架
├── day11: top-half + workqueue bottom-half
├── day12: regmap + debugfs 寄存器快照   ← 今天
├── day13: 回归测试 + 完整链路
└── day14: bring-up checklist
```

### 2.2 Day12 与前后天的关系

```
Day11 vs Day12：
  - Day11：IRQ 处理分离（top-half + workqueue）
  - Day12：在 Day11 基础上，把运行时状态组织成寄存器视图

Day12 vs Day13：
  - Day12：建立 regmap + debugfs 可观测性
  - Day13：用这条链路做完整回归测试

Day12 是"寄存器抽象"，Day13 是"回归验证"
```

### 2.3 为什么需要 regmap？

```
Day11 的问题：
  - irq_count, work_runs, work_items 等状态分散
  - 读写路径分散（top-half 更新 / worker 更新 / /proc 读取时拼出来）
  - 不够规整

regmap 的价值：
  - 提供统一的寄存器访问抽象
  - regmap_read() / regmap_write() / regmap_update_bits()
  - 把驱动状态组织成一张"寄存器地图"
```

---

## 三、为什么用"软件后端 regmap"？

### 3.1 为什么不用真实 MMIO？

```
当前 QEMU virt 上注入的 DT 节点是教学 fake 设备：
  - DT 里写了 reg / interrupts
  - platform_driver 能正常匹配
  - request_irq() / top-half / workqueue 都能跑通
  - 但背后不一定真的有一块适合直接 ioremap + readl/writel 的硬件寄存器空间

如果直接上 devm_ioremap_resource() + regmap_mmio：
  - 可能遇到"假 MMIO 真访问"的问题
  - 干扰 Day12 的核心教学目标
```

### 3.2 软件后端 regmap 方案

```
最稳的教学方案：
  1. 驱动内部维护 u32 regs[] 阴影寄存器数组
  2. 用 regmap 的回调把这个数组封装成"寄存器空间"
  3. 外部统一通过 regmap_read()/regmap_write() 访问

这样学到的东西：
  - regmap 配置怎么写
  - 哪些寄存器可读、哪些可写
  - 寄存器步进怎么定义
  - 如何把运行时状态同步成寄存器视图
  - debugfs 怎样导出寄存器快照

同时不引入"假 MMIO 真访问"的额外不确定性
```

---

## 四、寄存器地图

### 4.1 寄存器定义

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day12 寄存器地图                                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  偏移     名称              含义                        访问属性   │
│  ────────────────────────────────────────────────────────────────   │
│  0x00    CTRL              控制寄存器，bit0=允许触发     RW        │
│  0x04    STATUS            状态寄存器，enable/pending    RO        │
│  0x08    IRQ_COUNT         top-half 总触发次数          RO        │
│  0x0c    WORK_RUNS        worker 实际运行次数           RO        │
│  0x10    WORK_ITEMS       worker 处理事件总数           RO        │
│  0x14    PENDING_EVENTS   当前待处理事件数             RO        │
│  0x18    LAST_BATCH       最近一轮 worker 处理 batch   RO        │
│  0x1c    LAST_LATENCY_US  最近一次粗略延迟（微秒）     RO        │
│  0x20    MAX_LATENCY_US   最大粗略延迟（微秒）         RO        │
│  0x24    AVG_LATENCY_US   平均粗略延迟（微秒）         RO        │
│  0x28    WORK_MS          worker 模拟重活时长          RW        │
│  0x2c    VERSION           教学版本号                   RO        │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.2 访问属性说明

```
RW（可读写）：
  - CTRL：控制是否允许触发
  - WORK_MS：调整 worker 模拟重活时长

RO（只读）：
  - STATUS：反映 enable/pending/busy 状态
  - IRQ_COUNT / WORK_RUNS / WORK_ITEMS 等统计值
  - LAST/MAX/AVG_LATENCY_US 等延迟统计

regmap 的 readable_reg() / writeable_reg() 约束：
  - 限制了哪些寄存器可以读、哪些可以写
  - 防止误访问
```

---

## 五、regmap 软件后端配置

### 5.1 regmap_config 定义

```c
// regmap 配置：定义寄存器视图的基本参数
static struct regmap_config demo_regmap_regmap_config = {
    .reg_bits = 32,        // 寄存器地址宽度
    .val_bits = 32,        // 寄存器值宽度
    .reg_stride = 4,        // 寄存器步进（每个寄存器 4 字节）
    .max_register = 0x2c,   // 最大寄存器地址
    .reg_read = demo_regmap_reg_read,    // 读回调
    .reg_write = demo_regmap_reg_write,   // 写回调
    .readable_reg = demo_regmap_readable_reg,   // 可读约束
    .writeable_reg = demo_regmap_writeable_reg, // 可写约束
};
```

### 5.2 regmap 读回调

```c
// 从 shadow regs[] 读取
static int demo_regmap_reg_read(void *context,
                                unsigned int reg,
                                unsigned int *val)
{
    struct demo_regmap_priv *priv = context;

    if (reg >= ARRAY_SIZE(priv->regs))
        return -EINVAL;

    *val = priv->regs[reg / 4];  // reg 是偏移，/4 转数组索引
    return 0;
}
```

### 5.3 regmap 写回调

```c
// 写入 shadow regs[]（带约束）
static int demo_regmap_reg_write(void *context,
                                 unsigned int reg,
                                 unsigned int val)
{
    struct demo_regmap_priv *priv = context;

    if (reg >= ARRAY_SIZE(priv->regs))
        return -EINVAL;

    // writeable_reg() 已约束只有 CTRL(0x00) 和 WORK_MS(0x28) 可写
    priv->regs[reg / 4] = val;
    return 0;
}
```

### 5.4 读写路径

```
用户态读 snapshot：
  → debugfs snapshot_read()
  → demo_regmap_refresh_view() 刷新 shadow regs[]
  → regmap_read(priv->regmap, reg, &val)
  → demo_regmap_reg_read() 读 shadow regs[]

用户态写 poke：
  → debugfs poke_write()
  → regmap_write(priv->regmap, reg, val)
  → demo_regmap_reg_write() 写 shadow regs[]
```

---

## 六、debugfs 节点

### 6.1 三个 debugfs 节点

```
/sys/kernel/debug/demo_regmap/
├── snapshot   # 只读，打印寄存器快照
├── poke       # 只写，写入格式: "<reg> <val>"
└── trigger    # 只写，触发格式: "<times>"
```

### 6.2 snapshot 节点

```
cat /sys/kernel/debug/demo_regmap/snapshot

输出示例：
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
...
WORK_MS          reg=0x28 val=0x00000014 (20)
VERSION          reg=0x2c val=0x00001200 (4608)
```

### 6.3 poke 节点

```
echo "0x28 50" > /sys/kernel/debug/demo_regmap/poke
→ 写 WORK_MS 为 50ms

echo "0x00 0" > /sys/kernel/debug/demo_regmap/poke
→ 写 CTRL 为 0（禁止触发）
```

### 6.4 trigger 节点

```
echo 5 > /sys/kernel/debug/demo_regmap/trigger
→ 触发 5 次 fake IRQ
→ 走 generic_handle_irq() → top-half → queue_work → worker
```

---

## 七、完整执行路径

### 7.1 trigger 路径

```
echo 5 > /sys/kernel/debug/demo_regmap/trigger
    ↓
demo_regmap_trigger_write()
    → generic_handle_irq(priv->linux_irq)
    ↓
demo_regmap_handler()              [top-half / hardirq 上下文]
    → irq_count++
    → pending_events++
    → queue_work(priv->wq, &priv->work)
    → return IRQ_HANDLED

worker 被调度
    ↓
demo_regmap_workfn()               [进程上下文]
    → 取走 batch = pending_events
    → 更新 work_runs / work_items / latency
    → msleep(work_delay_ms) 模拟重活
    → demo_regmap_refresh_view() 刷新 shadow regs[]
```

### 7.2 snapshot 路径

```
cat /sys/kernel/debug/demo_regmap/snapshot
    ↓
demo_regmap_snapshot_read()
    → demo_regmap_refresh_view()  刷新 shadow regs[]
    → regmap_read() 逐个读寄存器
    → 输出格式化快照
```

---

## 八、"handler enabled interrupts" warning 分析

### 8.1 现象

```
执行 echo 5 > /sys/kernel/debug/demo_regmap/trigger 后：

WARNING: CPU: 0 PID: 1 at kernel/irq/handle.c:159 __handle_irq_event_percpu+0x138/0x170
irq 49 handler demo_regmap_handler+0x0/0x1b8 [demo_regmap] enabled interrupts
```

### 8.2 call trace 解读

```
el0t_64_sync           → arm64 用户态进入内核态的系统调用入口
  ↓
__arm64_sys_write     → 标准 write() 系统调用
  ↓
vfs_write              → VFS 写路径
  ↓
full_proxy_write       → debugfs 文件写操作的包装层
  ↓
demo_regmap_trigger_write  → 模块的 trigger 写函数
  ↓
generic_handle_irq     → IRQ core 的通用入口
  ↓
handle_fasteoi_irq     → arm64 + GIC 环境的标准 flow handler
  ↓
handle_irq_event       → 准备执行 IRQ handler
  ↓
__handle_irq_event_percpu → 最终调用 handler 的位置，warning 在这里报出
```

### 8.3 为什么出现 warning？

```
问题根源：
  - generic_handle_irq() 是 IRQ core 的通用入口
  - 它会按真实中断的处理路径来检查上下文
  - 但 trigger_write() 是在普通进程上下文里调用的
  - 不是真实硬中断现场

IRQ core 的期望：
  - hardirq handler 应该运行在"像硬中断一样"的约束环境中
  - 但当前在进程上下文里调用 generic_handle_irq()
  - 上下文不满足硬中断约束 → 报 warning

注意：
  - 功能仍然是正常的（IRQ_COUNT 会增加，worker 会跑）
  - warning 只是暴露了"上下文模拟不够真实"的问题
```

### 8.4 为什么功能仍然正常？

```
实测结果：
  IRQ_COUNT = 5
  WORK_RUNS = 2
  WORK_ITEMS = 5
  LAST_BATCH = 4

说明：
  - top-half 确实被调用了
  - worker 确实跑了
  - regmap / debugfs 主线是正常的

warning 只说明"trigger 路径的上下文模拟细节"有问题，不影响核心功能
```

### 8.5 最小修正方法

```c
// 在 demo_regmap_trigger_write() 里：
for (i = 0; i < times; i++) {
    unsigned long flags;

    local_irq_save(flags);         // 保存中断状态
    generic_handle_irq(priv->linux_irq);
    local_irq_restore(flags);       // 恢复中断状态
}
```

### 8.6 最值得记住的结论

```
generic_handle_irq() 不是简单的 helper，
而是真正会进入 IRQ core 的通用入口。

在普通进程上下文里用它模拟硬中断时，
必须考虑 hardirq 的上下文约束；
否则即使功能表面能跑通，
也可能在 IRQ core 的一致性检查阶段报 warning。
```

---

## 九、top-half 仍然保持最小化

### 9.1 Day12 没有把重活重新塞回中断上下文

```
Day12 的 demo_regmap_handler() 继续保持 Day11 的原则：
  - 记录 IRQ 次数
  - 记录时间戳
  - 增加 pending 数
  - queue_work()
  - 立刻返回

也就是说：
  Day12 只是把"状态组织方式"升级了（regmap）
  不是把中断设计原则推翻了
```

### 9.2 regmap 不等于必须在中断里使用

```
有些 regmap 后端可能会睡眠
有些 regmap 访问链路更适合进程上下文

Day12 的合理边界：
  - top-half 只做必要记账
  - worker 和 debugfs 读取路径再去刷新/读取寄存器视图
```

---

## 十、Day12 与 Day13 的关系

### 10.1 Day12 为 Day13 提供可观测性

```
Day13 需要一条能观测的完整链路：
  - regmap_read() 读寄存器（snapshot 节点）
  - regmap_write() 写寄存器（poke 节点）
  - generic_handle_irq() 触发 IRQ（trigger 节点）
  - workqueue worker 执行后刷新寄存器视图

Day12 建立的就是这条可观测链路
```

### 10.2 debugfs 和 Day14 的桥接

```
Day12 的 debugfs 节点：
  - snapshot：导出寄存器快照（可读）
  - poke：写寄存器（可写）
  - trigger：触发 fake IRQ（可写）

Day14 bring-up checklist 的第5步：
  "先做只读观测，再做写入"

Day12 的 debugfs 节点正好对应这个检查点
```

---

## 十一、面试要会讲的五句话

1. **"Day12 的核心是把 Day11 的运行时状态组织成一张寄存器地图，用 regmap 作为统一读写抽象层，并通过 debugfs snapshot 导出寄存器快照"**
   → 理解 Day12 的目标

2. **"为什么用软件后端 regmap？因为当前 QEMU virt 的 DT 节点是教学 fake 设备，背后不一定有真实 MMIO 空间，用 shadow regs[] 更稳且能学到 regmap 配置"**
   → 理解软件后端 regmap 的原因

3. **"generic_handle_irq() 不是简单 helper，而是真正进入 IRQ core 的通用入口，在进程上下文里调用它时如果不加 local_irq_save/restore，会报 'handler enabled interrupts' warning"**
   → 理解 generic_handle_irq 的本质

4. **"Day12 的 top-half 仍然保持最小化：只记账、记时间戳、queue_work()，真正耗时逻辑在 worker；regmap 只是状态组织方式升级，不是把中断设计原则推翻"**
   → 理解 top-half 最小化原则仍然有效

5. **"Day12 为 Day13 的回归测试提供可观测性：snapshot 节点可读寄存器、poke 节点可写寄存器、trigger 节点可触发 IRQ 路径"**
   → 理解 Day12 在 W2 中的位置

---

## 十二、验收标准

### 12.1 模块加载验收

- [ ] insmod demo_regmap.ko 成功
- [ ] dmesg 显示 probe ok
- [ ] /sys/kernel/debug/demo_regmap/ 目录存在

### 12.2 regmap 读路径验收

- [ ] cat /sys/kernel/debug/demo_regmap/snapshot 输出一组寄存器值
- [ ] CTRL/STATUS/IRQ_COUNT/WORK_RUNS 等字段可见
- [ ] VERSION 字段显示 0x00001200 (4608)

### 12.3 regmap 写路径验收

- [ ] echo "0x28 50" > /sys/kernel/debug/demo_regmap/poke 后 WORK_MS 变为 50
- [ ] poke 后再读 snapshot，WORK_MS 已变化
- [ ] poke 已正确解析寄存器地址和写入值

### 12.4 触发与统计联动验收

- [ ] echo 5 > /sys/kernel/debug/demo_regmap/trigger 后 IRQ_COUNT = 5
- [ ] WORK_RUNS < IRQ_COUNT（batch 合并）
- [ ] WORK_ITEMS = 5
- [ ] LAST/MAX/AVG_LATENCY_US 有数值

### 12.5 运行态联动验收

- [ ] 触发后 snapshot 中所有 RO 字段（IRQ_COUNT/WORK_RUNS 等）都变化
- [ ] 多次触发后 pending_events 能正确归零
- [ ] 说明 regmap 不是"死数组"，而是和运行态真正连上了

---

## 附录：完整验收命令

```
# 1. 基础加载
insmod /demo_regmap.ko
dmesg | grep demo_regmap
ls -la /sys/kernel/debug/demo_regmap/

# 2. 验证 regmap 读路径
cat /sys/kernel/debug/demo_regmap/snapshot

# 3. 验证 regmap 写路径
echo "0x28 50" > /sys/kernel/debug/demo_regmap/poke
cat /sys/kernel/debug/demo_regmap/snapshot
# 确认 WORK_MS = 50

# 4. 触发 IRQ 并验证统计联动
echo 5 > /sys/kernel/debug/demo_regmap/trigger
cat /sys/kernel/debug/demo_regmap/snapshot
# 确认 IRQ_COUNT=5, WORK_ITEMS=5, LAST_BATCH>0

# 5. 观察 batch 合并
echo 10 > /sys/kernel/debug/demo_regmap/trigger
cat /sys/kernel/debug/demo_regmap/snapshot
# WORK_RUNS < 10, WORK_ITEMS = 10
```
