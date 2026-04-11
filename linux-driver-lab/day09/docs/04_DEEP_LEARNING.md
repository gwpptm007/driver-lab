# Day09 深度指南 - Device Tree + of_match_table + reg/irq 解析

## 一、Day09 是什么？

Day09 是 W2（嵌入式驱动模式）的第二天，定位是**Device Tree 设备描述接入 + DT 属性解析链路**。

**核心目标**：把 Day08 的"设备来自模块内手工注册"，切换成"设备来自 Device Tree"，建立"DT 节点 → platform_device → of_match_table → probe"这条完整链路的认知。

Day09 不做 IRQ 申请，不做 MMIO 访问。它的重点是：
1. **DT fragment 注入**：用 inject_virt_dt.py 把教学节点插入 QEMU DTB
2. **of_match_table**：通过 DT compatible 匹配驱动和设备
3. **reg/interrupts 解析**：platform_get_resource / platform_get_irq 从 DT 拿到资源
4. **原始 cells 对照**：打印 DT 原始 cells 和解析后的 resource 做教学对照

---

## 二、W2 学习路径中的位置

### 2.1 W2 整体架构

```
W2 (嵌入式驱动模式 - day08-14)
├── day08: platform_driver + probe/remove + devm
├── day09: Device Tree + of_match_table + reg/irq   ← 今天
├── day10: request_irq + top-half + proc 验证
├── day11: top-half + workqueue bottom-half
├── day12: regmap + debugfs 寄存器快照
├── day13: ftrace function_graph 路径观察
└── day14: bring-up checklist
```

### 2.2 Day09 与前后天的关系

```
Day08 vs Day09：
  - Day08：设备来自模块内手工注册的 platform_device（同名匹配）
  - Day09：设备来自 Device Tree（compatible 匹配）

Day09 vs Day10：
  - Day09：只解析 reg/interrupts，不申请真实 IRQ
  - Day10：在 Day09 基础上，接 request_irq() 把 top-half 真正跑起来

Day09 是 Day08 的"设备来源升级版"：
 骨架不变，设备来源从"手工"变成"DT"
```

---

## 三、为什么需要 Device Tree？

### 3.1 platform_device 的来源

```
在 Day08 里，设备是模块内手工注册的：

  static struct platform_device demo_platform_device = {
      .name = DRV_NAME,
      .resource = demo_resources,
  };

这在教学上很直观，但真实板级开发有问题：

  - 板子换了，设备列表要改驱动代码
  - 内核代码和硬件描述强耦合
  - 驱动代码难以在不同板子间复用
```

### 3.2 Device Tree 的价值

```
DT 把"硬件描述"从驱动代码里剥离出来：

  .dts（设备树源文件）：
    / {
        demo_dt: demo_node@10000000 {
            compatible = "demo,day09-pdrv";
            reg = <0x0 0x10000000 0x0 0x1000>;
            interrupts = <0x0 0x64 0x4>;
        };
    };

  驱动代码：
    of_match_table = {
        .compatible = "demo,day09-pdrv",
    };

好处：
  - 换板子：只要换 DT 文件，驱动代码不动
  - Linux 内核统一用 DT 描述硬件
  - ARM/ARM64/RISC-V 等架构广泛使用
```

---

## 四、QEMU virt 上的 DT 实验链路

### 4.1 为什么用 QEMU virt？

```
真实 ARM 开发板：
  - 硬件已经固定，DT 由厂商提供
  - 不容易在运行时修改 DT

QEMU virt：
  - 软件模拟的虚拟开发板
  - DT 可以通过 -dtb 注入
  - 非常适合学习 DT 注入和解析流程

Day09 的实验链路：
  1. 让 QEMU virt 导出基础 DTB
  2. 用 inject_virt_dt.py 把教学节点插进去
  3. 用新 DTB 启动 QEMU
  4. 内核根据 DT 节点自动创建 platform_device
  5. 驱动通过 of_match_table 匹配
```

### 4.2 inject_virt_dt.py 的作用

```bash
# 1. QEMU 导出基础 DTB
qemu-system-aarch64 -machine virt,dumpdtb=virt-base.dtb

# 2. 反编译成 DTS
dtc -I dtb -O dts -o virt-base.dts virt-base.dtb

# 3. 注入教学节点
python3 inject_virt_dt.py \
    --input virt-base.dts \
    --fragment demo_day09.fragment.dtsi \
    --output virt-day09.dts

# 4. 编译回 DTB
dtc -I dts -O dtb -o virt-day09.dtb virt-day09.dts

# 5. 用新 DTB 启动 QEMU
qemu-system-aarch64 -dtb virt-day09.dtb ...
```

### 4.3 demo_day09.fragment.dtsi 的内容

```dts
        demo_dt: demo_dt@10000000 {
            compatible = "demo,day09-pdrv";
            label = "from-qemu-dt";
            reg = <0x0 0x10000000 0x0 0x1000>;
            interrupts = <0x0 0x64 0x4>;
        };
```

```
节点名：demo_dt@10000000
compatible：驱动匹配关键字
label：自定义字符串属性（教学用）
reg：地址范围（4 个 cells，arm64 惯例）
interrupts：GIC 中断三元组（3 个 cells）
```

---

## 五、of_match_table 详解

### 5.1 Day09 的匹配表

```c
static const struct of_device_id demo_of_match[] = {
    {
        .compatible = "demo,day09-pdrv",
        .data = "day09-of-match",
    },
    { }  // sentinel
};
MODULE_DEVICE_TABLE(of, demo_of_match);
```

```
匹配过程：
  1. 内核解析 DT，创建 platform_device
  2. platform_device 的 .of_node->full_name = "/demo_dt@10000000"
  3. platform_device 的 .of_node->properties 包含 compatible = "demo,day09-pdrv"
  4. platform_driver 注册时，扫描 of_match_table
  5. 找到 compatible 匹配，触发 probe

注意：Day08 是 device.name == driver.name（同名匹配）
      Day09 是 device.of_node.compatible == of_match_table.compatible（DT 匹配）
```

### 5.2 device_get_match_data() 的作用

```c
priv->match_name = (const char *)device_get_match_data(dev);
```

```
probe 里可以通过这个方式取出 of_match_table 里的 .data

作用：
  - 区分同一驱动的不同变体
  - 教学里用来打印"匹配来源"字符串

Linux 5.15+ 推荐用 device_get_match_data()
早期版本可能用 of_device_get_match_data()
```

---

## 六、reg 和 interrupts 解析

### 6.1 DT 中的 reg 属性

```dts
reg = <0x0 0x10000000 0x0 0x1000>;
```

```
arm64 的 #address-cells = 2，#size-cells = 2

4 个 u32 cells：
  [0] = addr_hi（高 32 位，通常 0）
  [1] = addr_lo（低 32 位，0x10000000）
  [2] = size_hi（高 32 位，0）
  [3] = size_lo（低 32 位，0x1000）

platform_get_resource(pdev, IORESOURCE_MEM, 0) 后：
  → mem->start = 0x10000000
  → mem->end = 0x10000fff（0x10000000 + 0x1000 - 1）
  → mem->size = 0x1000
```

### 6.2 DT 中的 interrupts 属性

```dts
interrupts = <0x0 0x64 0x4>;
```

```
GIC 中断三元组（3 个 cells）：
  [0] = 中断类型（0 = SPI，1 = PPI）
  [1] = 中断号（0x64 = 100，对应 SPI 100）
  [2] = 触发标志（4 = 上升沿触发）

platform_get_irq(pdev, 0) 后：
  → 返回 Linux 内部的 virq 编号（通常不是 100）
  → 中间经过 IRQ domain 映射

打印原始 cells 是为了让教学：
  - 看到 DT 里写的原始值
  - 再看 platform_get_irq() 解析后的 Linux virq
  - 对照理解"DT 描述"和"内核视图"的区别
```

### 6.3 raw cells vs parsed resource

```
demo_of_dump_raw_reg() 打印：
  raw DT reg cells: <0x0 0x10000000 0x0 0x1000>

platform_get_resource() 后打印：
  parsed MEM resource: start=0x10000000 end=0x10000fff size=0x1000

两者对应关系：
  raw cells 是 DT 里的原始 u32 数组
  parsed resource 是内核解析后的 struct resource
  → 教学上把"描述"和"解析结果"对应起来
```

---

## 七、Day09 probe 详解

### 7.1 probe 的变化

```c
// Day08 probe 里：
if (!np) { dev_err(...); return -ENODEV; }  // Day08 的 pdev 没有 of_node

// Day09 probe 里：
if (!np) { dev_err(...); return -ENODEV; }  // Day09 的 pdev 有 of_node，通过！
```

```
Day09 probe 相比 Day08：
  1. 多了一步 of_property_read_string()（读取自定义 DT 属性）
  2. 多了一步 demo_of_dump_raw_irq()（打印原始 DT interrupts cells）
  3. 资源解析、drvdata 保存、devm 清理 —— 骨架完全相同
```

### 7.2 为什么 Day09 仍然不做真实 IRQ 申请？

```
因为 QEMU virt 上的教学 DT 节点：
  - DT 描述了 interrupts 属性
  - 内核能解析成 Linux virq
  - 但 QEMU virt 不会真的给这个假节点产生硬件中断

如果 request_irq() 成功注册了 handler：
  - 但没有真实硬件触发源
  - /proc/interrupts 计数不会自动增长

这和 Day08 故意不做真实 MMIO 是同样的教学策略：
  先把"DT → platform_device → probe → 资源解析"这条链跑通
  Day10 再接入 request_irq + 软件触发
```

---

## 八、Day09 vs Day10 的关系

### 8.1 Day09 建立了什么？

```
Day09 建立了：
  1. DT 节点能进入 QEMU virt 的 DTB
  2. 内核根据 DT 节点创建 platform_device
  3. of_match_table 能匹配到驱动
  4. probe 里能解析出 reg/interrupts
  5. device_get_match_data() 能取到匹配私有数据

Day10 在此基础上：
  1. request_irq() 真正注册 IRQ handler
  2. /proc/demo_irq_trigger 软件注入触发
  3. /proc/interrupts 计数能看到变化
```

### 8.2 Day09 probe 日志的预期

```
成功时的 dmesg 日志：

[   11.605836] demo_of_pdrv 10000000.demo_dt: probe start
[   11.606258] demo_of_pdrv 10000000.demo_dt: of node full name: demo_dt@10000000
[   11.606647] demo_of_pdrv 10000000.demo_dt: of match data: day09-of-match
[   11.606974] demo_of_pdrv 10000000.demo_dt: dt label: from-qemu-dt
[   11.607503] demo_of_pdrv 10000000.demo_dt: raw DT reg cells: <0x0 0x10000000 0x0 0x1000>
[   11.607967] demo_of_pdrv 10000000.demo_dt: raw DT interrupts cells: <0x0 0x64 0x4>
[   11.608382] demo_of_pdrv 10000000.demo_dt: parsed MEM resource: start=0x10000000 end=0x10000fff size=0x1000
[   11.608859] demo_of_pdrv 10000000.demo_dt: parsed Linux IRQ: 49
```

---

## 九、面试要会讲的五句话

1. **"Day09 的核心是把 Day08 的'设备来自模块内手工注册'切换成'设备来自 Device Tree'：QEMU virt 通过 inject_virt_dt.py 把教学节点注入 DTB，内核根据 DT 节点自动创建 platform_device，驱动通过 of_match_table 的 compatible 匹配"**
   → 理解 Day09 的目标

2. **"DT 的 reg 属性（4 个 cells：addr_hi/addr_lo/size_hi/size_lo）和 interrupts 属性（3 个 cells：type/nr/flags）是硬件描述的原始形式，platform_get_resource() 和 platform_get_irq() 是把它们翻译成内核 struct resource 和 Linux virq 的解析过程"**
   → 理解 reg/interrupts 和解析后资源的关系

3. **"of_match_table 的匹配过程是：内核解析 DT → 创建 platform_device（带 of_node）→ platform_driver 注册时扫描 of_match_table → 找 compatible 匹配 → 成功则调用 probe；Day08 是 device.name == driver.name 同名匹配，Day09 是 compatible 匹配"**
   → 理解 of_match_table 匹配机制

4. **"打印 raw DT cells 是教学设计：让学员看到 DT 原始 u32 数组，再对照解析后的 resource struct，直观理解'描述'和'解析结果'的区别；raw cells 不是驱动必须这样写，而是为了教学对照"**
   → 理解 raw cells 打印的教学目的

5. **"Day09 和 Day10 的关系是骨架不变、层层递进：Day09 建立'DT → pdev → probe → reg/irq 解析'链路，Day10 在此基础上接入 request_irq() + 软件触发 + /proc/interrupts 计数；Day09 不申请真实 IRQ 因为 QEMU virt 的教学 DT 节点没有真实硬件中断源"**
   → 理解 Day09 → Day10 的演进

---

## 十、验收标准

### 10.1 QEMU + DTB 验收

- [ ] virt-base.dtb 能从 QEMU 导出
- [ ] virt-day09.dtb 能生成（包含教学节点）
- [ ] QEMU 能用新 DTB 正常启动

### 10.2 probe 验收

- [ ] insmod demo_of.ko 成功
- [ ] dmesg 显示：
  - `probe start`
  - `of node full name: demo_dt@10000000`
  - `of match data: day09-of-match`
  - `raw DT reg cells: <0x0 0x10000000 0x0 0x1000>`
  - `raw DT interrupts cells: <0x0 0x64 0x4>`
  - `parsed MEM resource: start=0x10000000 ...`
  - `parsed Linux IRQ: 49`

### 10.3 platform bus 可见性验收

- [ ] `ls /sys/bus/platform/devices | grep demo` 有输出
- [ ] `ls /sys/firmware/devicetree/base/` 能看到 `demo_dt@10000000`

---

## 附录：Day09 完整 DT 注入链路

```
QEMU virt 基础 DTB
    ↓ [qemu-system-aarch64 -machine virt,dumpdtb=virt-base.dtb]
virt-base.dtb
    ↓ [dtc -I dtb -O dts]
virt-base.dts（可读文本）
    ↓ [inject_virt_dt.py --fragment demo_day09.fragment.dtsi]
virt-day09.dts（包含教学节点）
    ↓ [dtc -I dts -O dtb]
virt-day09.dtb
    ↓ [QEMU 启动 -dtb virt-day09.dtb]
guest 内核启动
    ↓ [内核解析 DT]
    platform_device 创建（of_node = demo_dt@10000000）
    ↓ [platform_driver 注册，of_match_table 匹配]
    demo_of_probe() 被调用
```
