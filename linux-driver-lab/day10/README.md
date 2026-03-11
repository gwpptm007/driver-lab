# Day10 - request_irq + top-half + `/proc/interrupts` 验证

> 这一节是在 day09 的基础上继续前进：
>
> **把“DT 描述的中断资源”真正接到 `request_irq()`，再通过软件触发的方式，把 top-half 跑起来并看到计数增长。**

---

## 1. 本课到底学什么

day09 已经把下面这条链路打通了：

- Device Tree 节点被插入到 QEMU `virt` 导出的基础 DT 中
- 内核根据 DT 创建 `platform_device`
- `platform_driver` 通过 `of_match_table` 完成匹配
- `probe()` 中能拿到：
  - `reg`
  - `interrupts`
  - 自定义属性 `demo,label`

但是，**day09 还停留在“解析到资源”这一步**。

到了 day10，要继续把中断相关的几个核心知识串起来：

- `platform_get_irq()` 如何从 platform 设备中取出 Linux IRQ
- `request_irq()` 如何注册中断处理函数
- top-half 最小处理函数应该长什么样
- `/proc/interrupts` 里的计数到底什么时候会变化
- 没有真实硬件事件源时，如何自己构造一个可观察的教学实验

所以，这一节不是在做“真实外设驱动”，而是在做一个**教学化最小 IRQ 实验**。

---

## 2. Day10 相比 Day09 多了什么

### day09 的重点

day09 重点是：

- 理解 DT 节点如何描述平台设备
- 理解 `of_match_table`
- 理解 `platform_get_resource()` 和 `platform_get_irq()` 的来源
- 看懂“原始 DT cells”与“解析后资源”的关系

### day10 的重点

day10 在 day09 基础上补齐这条链路：

```text
DT interrupts 属性
    -> 内核中断域把它翻译成 Linux virq
    -> platform_get_irq() 取到 virq
    -> request_irq() 注册 top-half
    -> top-half 执行并累计计数
    -> /proc/interrupts 与驱动内部统计互相印证
```

也就是说，**day09 是“资源解析课”，day10 是“中断注册与观测课”。**

---

## 3. 为什么 day10 还需要“软触发接口”

这是这节课最容易误解的一点。

当前的 DT 节点是我们手工注入到 QEMU `virt` 平台里的一个教学 fake 设备：

- DT 里确实写了 `interrupts = <...>`
- 内核也确实能把它翻译成一条 Linux IRQ
- 驱动也确实可以 `request_irq()` 成功

但是：

**QEMU `virt` 并没有真的给这个 fake 节点接上一块会主动拉中断线的硬件。**

所以如果只做到：

- `platform_get_irq()`
- `request_irq()`

那么驱动虽然可能加载成功，但 `/proc/interrupts` 里的计数通常不会自动增长。

这不是驱动坏了，而是**没有真实外设事件源**。

因此 day10 专门加了一个教学接口：

```bash
echo 1 > /proc/demo_irq_trigger
```

它的作用不是模拟真实硬件寄存器，而是：

- 主动对这条已经申请成功的 Linux IRQ 做一次软件注入
- 让 top-half 真正被执行一次
- 让你看到：
  - 驱动日志变化
  - 内部计数变化
  - `/proc/interrupts` 变化

这样就能在**没有真实硬件**的前提下，把 IRQ 核心流程先学懂。

---

## 4. 本课你应该建立的几个核心认知

### 4.1 `interrupts = <...>` 不是“已经触发中断”

它只是一个**硬件描述**，说明这个设备应该接到哪条中断线上。

### 4.2 `platform_get_irq()` 也不是“中断来了”

它只是把 DT/中断域里描述的那条中断解析成 Linux 内部的 virq 编号。

### 4.3 `request_irq()` 也不是“已经能看到计数增长”

它只是注册好了处理函数，表示：

> 以后如果这条 IRQ 被触发，请回调我。

### 4.4 `/proc/interrupts` 增长的前提

前提是：

- 这条 IRQ 真被触发了
- 内核中断路径真正走到了 handler

在本实验中，这个“触发”动作不是来自真实硬件，而是来自 `/proc/demo_irq_trigger` 的软件注入。

---

## 5. 目录说明

```text
day10/
├── Makefile
├── build.sh
├── demo_irq.c
├── demo_irq.fragment.dtsi
├── inject_virt_dt.py
└── README.md
```

各文件作用如下：

### `demo_irq.c`
核心驱动文件，包含：

- `platform_driver`
- `probe/remove`
- `request_irq()`
- top-half handler
- `/proc/demo_irq_stats`
- `/proc/demo_irq_trigger`

### `demo_irq.fragment.dtsi`
教学用 DT 片段。

把一个 fake 平台设备节点插进 QEMU `virt` 的基础 DT 中，提供：

- `compatible`
- `reg`
- `interrupts`
- `demo,label`

### `inject_virt_dt.py`
把上面的 DT 片段插入到 QEMU 导出的基础 DTS 中。

### `build.sh`
一键完成整个实验链路：

- 编译模块
- 构造最小 rootfs
- 导出 QEMU 基础 DTB/DTS
- 注入 day10 的教学节点
- 重新编译 DTB
- 启动 ARM64 QEMU 虚拟机

### `Makefile`
外部模块编译入口。

---

## 6. 驱动总体执行流程

可以先从整体流程理解，再回头看代码。

```text
加载 demo_irq.ko
    -> module_init
    -> platform_driver_register
    -> 与 DT 节点匹配成功
    -> demo_irq_probe()
        -> 读取 DT 原始 reg / interrupts
        -> platform_get_resource() 取 MEM 资源
        -> platform_get_irq() 取 Linux IRQ
        -> request_irq() 注册 top-half
        -> 创建 /proc/demo_irq_stats
        -> 创建 /proc/demo_irq_trigger

用户 echo N > /proc/demo_irq_trigger
    -> demo_irq_trigger_write()
    -> generic_handle_irq(linux_irq)
    -> demo_irq_handler()
    -> irq_count++
    -> /proc/interrupts 计数增长

卸载 demo_irq.ko
    -> remove()
    -> 删除 proc 节点
    -> free_irq()
```

---

## 7. `demo_irq.c` 里最重要的知识点

## 7.1 私有结构 `struct demo_irq_priv`

这份结构体把本实验最关键的状态都收在一起：

- `mem`：解析出来的内存资源
- `linux_irq`：解析出来的 Linux virq
- `raw_reg[]`：原始 `reg` cells
- `raw_irq[]`：原始 `interrupts` cells
- `label`：DT 自定义字符串
- `irq_count`：驱动内部累计的中断次数
- `proc_stats` / `proc_trigger`：两个 proc 节点

这样做的好处是：

- 你可以同时看到“原始 DT 数据”和“解析后资源”
- 你可以把 day09 和 day10 的知识连续地放在一个驱动里理解

---

## 7.2 `demo_irq_dump_raw_reg()`

这个函数直接读 DT 中的 `reg` 原始 cells。

作用不是驱动必须这样写，而是为了教学：

- 让你看到 `reg = <...>` 在 DT 里长什么样
- 再和 `platform_get_resource()` 得到的结果做对照

也就是把：

```text
原始 DT 描述
```

和：

```text
内核解析后的 resource
```

对应起来。

---

## 7.3 `demo_irq_dump_raw_irq()`

与 `raw_reg` 一样，这里直接读 `interrupts` 的原始三元组。

例如：

```dts
interrupts = <0x0 0x78 0x4>;
```

你可以把它理解为：

- 中断类型信息
- 中断号信息
- 触发方式信息

然后再和 `platform_get_irq()` 得到的 Linux virq 做对照。

注意：

**DT 里的 `interrupts` 数值，和最终 Linux 内部看到的 virq，不一定是一模一样的数字。**

中间还会经过中断域（IRQ domain）的翻译。

---

## 7.4 `demo_irq_handler()`

这是本实验的最小 top-half。

这里故意不做太多事情，只做一件核心动作：

```c
atomic64_inc_return(&priv->irq_count)
```

为什么要极简？

因为这节课是先让你搞清楚：

- handler 什么时候会进来
- 进入后最基本的统计怎么做
- `/proc/interrupts` 和驱动自己的计数怎么互相印证

至于：

- bottom half
- tasklet / workqueue
- 硬件状态寄存器 ack
- 中断共享
- 屏蔽/解除屏蔽

这些是后续再展开的话题。

---

## 7.5 为什么用 `atomic64_t irq_count`

因为中断上下文和普通进程上下文不同。

虽然这个教学实验里逻辑很简单，但在中断路径里做计数，使用原子变量更合适，原因是：

- 不需要睡眠
- 不依赖 mutex
- 适合做简单统计

教学上你可以先把它理解成：

> “一个适合在中断里直接加一的计数器。”

---

## 7.6 `demo_irq_stats_show()`

`/proc/demo_irq_stats` 是从“驱动内部视角”看状态。

它会输出：

- 模块名
- label
- Linux IRQ
- irq_count
- mem_start
- mem_size

这个接口的意义是：

- `/proc/interrupts` 是**内核通用视角**
- `/proc/demo_irq_stats` 是**驱动自身视角**

两个一起看，最容易建立直觉。

---

## 7.7 `demo_irq_trigger_write()`

这是 day10 最关键的教学接口。

你执行：

```bash
echo 1 > /proc/demo_irq_trigger
```

本质上就是告诉驱动：

> “请对这条已经申请成功的 Linux IRQ，人为触发 1 次。”

如果你写入：

```bash
echo 10 > /proc/demo_irq_trigger
```

就会循环注入 10 次。

这里为什么要限制次数不能太大？

因为它只是教学接口，防止误操作一下子触发太多，造成日志和统计不易观察。

---

## 7.8 `demo_irq_probe()` 是本课主战场

这个函数基本就是 day10 的全部重点。

它按顺序完成：

### 第一步：检查 `of_node`

确认这个平台设备确实来自 DT。

### 第二步：分配私有数据

用 `devm_kzalloc()` 给驱动状态分配内存。

### 第三步：读取教学属性

包括：

- `demo,label`
- 原始 `reg`
- 原始 `interrupts`

### 第四步：拿内存资源

```c
platform_get_resource(pdev, IORESOURCE_MEM, 0)
```

### 第五步：拿中断资源

```c
platform_get_irq(pdev, 0)
```

这一句是 day10 的核心之一。

### 第六步：注册中断处理函数

```c
request_irq(priv->linux_irq, demo_irq_handler, 0, DRV_NAME, priv)
```

### 第七步：创建 proc 观察接口

让用户空间能读状态、触发 IRQ。

### 第八步：保存 drvdata 和全局入口

方便 `remove()` 和 proc 接口访问。

---

## 8. 为什么这里还要保留全局指针 `g_demo_priv`

从工程设计角度说，全局单例通常不是最优方案。

但这个实验这样写有一个明显好处：

- `/proc` 读写接口很容易访问到当前这一个教学设备状态
- 结构更直观
- 对单实例 demo 来说足够清晰

也就是说，这里是**为了教学可读性做的简化**。

如果以后扩展到多实例设备，就不能继续这样写，而应该把 proc 节点和具体设备实例做更严谨的关联。

---

## 9. `remove()` 做了什么

卸载模块时，需要把在 `probe()` 里申请的资源按逆序释放：

- 清空全局指针
- 删除 proc 节点
- `free_irq()` 释放中断

这个顺序很重要，因为：

- 如果 proc 节点还在，用户可能还能访问
- 如果 IRQ 还在，handler 可能还会被回调

所以 remove 的职责就是把实验现场收干净。

---

## 10. `build.sh` 到底做了什么

这一节专门把脚本拆开讲，因为 day10 的脚本比 day09 更完整，也更值得作为后面 day11、day12 的模板继续用。

---

## 10.1 第 1 段：准备路径和环境变量

```bash
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
```

这两句用于定位：

- 当前 day10 目录
- 仓库根目录

随后脚本定义：

- `KERNEL_DIR`
- `BUSYBOX_DIR`
- `CROSS_COMPILE`
- `ARCH_NAME`
- `QEMU_BIN`
- `DTC_BIN`
- `PYTHON_BIN`

这些变量都允许外部覆盖。

这意味着你以后切换环境时，可以直接：

```bash
export KERNEL_DIR=...
export BUSYBOX_DIR=...
export CROSS_COMPILE=...
./build.sh
```

而不需要每次改脚本正文。

---

## 10.2 第 2 段：检查依赖是否齐全

脚本会依次检查：

- `KERNEL_DIR` 是否存在
- `BUSYBOX_DIR` 是否存在
- `qemu-system-aarch64` 是否存在
- `dtc` 是否存在
- `python3` 是否存在
- 交叉编译器 `${CROSS_COMPILE}gcc` 是否存在

这一步的意义是：

**在真正开始编译前，先把环境问题尽早暴露出来。**

这是一个很好的工程习惯。

---

## 10.3 第 3 段：确认内核构建产物

脚本会检查：

- arm64 build 目录是否存在
- `Image` 是否存在

如果 `output/arm64/Image` 不存在，但 `build/arm64/arch/arm64/boot/Image` 已存在，就自动复制过去。

这个设计很实用，因为：

- 你平时可能只编译了内核
- 但还没手动同步到 `output/arm64`
- 脚本会帮你补一次

---

## 10.4 第 4 段：定位 BusyBox

脚本会从多个候选路径里找 BusyBox：

- `output/arm64/_install/bin/busybox`
- `output/arm64/busybox`
- `build/arm64/busybox`

这样做的原因是：

- 不同阶段你可能只有 build 产物
- 或者已经执行过 install
- 脚本尽量兼容多种状态

之后还会用 `file` 检查它是否是静态链接。

如果是动态链接，就直接报错退出。

原因很重要：

**最小 rootfs/initramfs 里通常没有一整套动态库环境，动态链接 BusyBox 很容易在 guest 里起不来。**

---

## 10.5 第 5 段：编译 day10 模块

```bash
make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE" clean
make KDIR="$KDIR" ARCH="$ARCH_NAME" CROSS_COMPILE="$CROSS_COMPILE"
```

这一步就是按当前 arm64 内核构建目录，编译 `demo_irq.ko`。

---

## 10.6 第 6 段：构造最小 rootfs

脚本会创建：

```text
rootfs/
├── bin/
├── dev/
├── proc/
├── sys/
├── sbin/
├── etc/
└── tmp/
```

然后：

- 把 BusyBox 拷进去
- 建立一批常用命令软链接
- 把 `demo_irq.ko` 拷到根目录
- 生成 `/init`

这里的 `/init` 会在 guest 启动后：

- 挂载 `proc`
- 挂载 `sysfs`
- 挂载 `devtmpfs`
- 打印实验提示
- 最后进入 `/bin/sh`

也就是说，**这个 rootfs 不是通用 Linux 发行版 rootfs，而是专门为 day10 实验定制的最小用户空间。**

---

## 10.7 第 7 段：把 rootfs 打成 initramfs 镜像

```bash
find . | cpio -o -H newc | gzip -9 > ../rootfs.img
```

这一步会把前面构造的 `rootfs/` 打包成 `rootfs.img`。

启动 QEMU 时，这个镜像会被作为 initramfs 传给内核。

---

## 10.8 第 8 段：导出 QEMU `virt` 基础 DT

```bash
qemu-system-aarch64 -machine virt,dumpdtb=virt-base.dtb ...
```

这一步并不是正式启动虚拟机做实验，而是借助 QEMU 先导出它自带的 `virt` 机器 DTB。

随后再用：

```bash
dtc -I dtb -O dts -o virt-base.dts virt-base.dtb
```

把它反编译成人可读的 DTS。

---

## 10.9 第 9 段：注入教学节点

脚本会调用：

```bash
python3 ./inject_virt_dt.py \
    --input virt-base.dts \
    --fragment demo_irq.fragment.dtsi \
    --output virt-irq.dts
```

也就是把我们写的教学设备片段插入到基础 DTS 中。

再通过 `dtc` 编译回新的 `virt-irq.dtb`。

正式启动 QEMU 时，使用的就是这个**已经插入教学节点的新 DTB**。

---

## 10.10 第 10 段：正式启动 QEMU

最后用：

- `-kernel "$KERNEL_IMG"`
- `-dtb virt-irq.dtb`
- `-initrd rootfs.img`
- `-append "console=ttyAMA0 root=/dev/ram0 rw rdinit=/init"`

启动 ARM64 虚拟机。

这表示：

- 内核镜像来自你自己的 arm64 编译产物
- 设备树来自“QEMU 基础 DT + day10 片段”
- 用户空间来自刚刚构造的最小 rootfs

这三部分拼起来，才构成完整实验环境。

---

## 11. 推荐执行方式

进入 day10：

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day10
```

设置环境变量：

```bash
export KERNEL_DIR=/home/wq7/workspace/driver-lab/kernel-src/linux-5.15.10
export BUSYBOX_DIR=/home/wq7/workspace/driver-lab/kernel-src/busybox-1.36.1
export CROSS_COMPILE=aarch64-linux-gnu-
./build.sh
```

这是当前仓库结构下最推荐的执行方式。

---

## 12. Guest 里推荐验收步骤

进入 guest shell 后，按下面顺序操作。

### 12.1 加载模块

```bash
insmod /demo_irq.ko
```

### 12.2 看驱动日志

```bash
dmesg | grep demo_irq
```

你应该重点关注：

- `probe start`
- `raw DT reg cells`
- `raw DT interrupts cells`
- `parsed MEM resource`
- `parsed Linux IRQ`
- `request_irq done`

---

### 12.3 看 `/proc/interrupts`

```bash
cat /proc/interrupts | grep demo_irq
```

刚加载完成时，计数通常还是 0，或者还没出现明显增长。

这是正常的，因为还没有真实事件发生。

---

### 12.4 看驱动内部统计

```bash
cat /proc/demo_irq_stats
```

预期至少能看到：

- `module=demo_irq`
- `label=...`
- `linux_irq=...`
- `irq_count=0`
- `mem_start=...`
- `mem_size=...`

---

### 12.5 软件触发一次中断

```bash
echo 1 > /proc/demo_irq_trigger
```

然后重新看：

```bash
cat /proc/interrupts | grep demo_irq
cat /proc/demo_irq_stats
dmesg | grep demo_irq
```

此时你应该能看到计数变化。

---

### 12.6 连续触发多次

```bash
echo 10 > /proc/demo_irq_trigger
```

再看统计值，应该会继续累加。

---

### 12.7 卸载模块

```bash
rmmod demo_irq
```

卸载后再看日志，可以确认 `remove()` 是否走到、最终累计计数是多少。

---

## 13. 这节课最容易踩的坑

### 13.1 `request_irq()` 成功，不代表计数会自动涨

因为没有真实外设事件源。

### 13.2 `interrupts` 三元组不是最终 virq 号

中间还会经过 IRQ domain 翻译。

### 13.3 BusyBox 不能是动态链接

否则最小 rootfs 里很容易跑不起来。

### 13.4 看不到 `/proc/demo_irq_*`

说明模块可能没成功 `probe()`，先查 `dmesg`。

### 13.5 `/proc/interrupts` 没变化

先确认：

- `insmod` 是否成功
- `platform_get_irq()` 是否成功
- `request_irq()` 是否成功
- `echo 1 > /proc/demo_irq_trigger` 是否返回错误

---

## 14. 这节课结束后你应该能回答的问题

学完 day10，建议你至少能把下面这些问题讲清楚：

1. `platform_get_irq()` 的数据来源是什么？
2. `request_irq()` 成功说明了什么？
3. 为什么 fake 设备需要额外做软件触发？
4. top-half 在这个实验里做了什么？
5. `/proc/interrupts` 和 `/proc/demo_irq_stats` 各自代表什么视角？
6. 为什么 `build.sh` 里要先导出 QEMU 基础 DT 再注入片段？

如果这些问题你都能自己讲顺，说明 day10 的学习目标基本达到了。

---

## 15. 为 day11 做铺垫

day10 学完后，后面就可以自然进入更深入的话题，例如：

- 为什么真实硬件驱动通常还要做寄存器 ack
- 为什么中断处理不应该在 top-half 干太多重活
- bottom half / threaded irq / workqueue 分别适合什么场景
- 共享中断和独占中断的区别
- 如何把“中断发生”与“设备状态变化”结合起来分析

所以 day10 的意义，不是只会写一个 `request_irq()`，而是：

**把“资源解析”正式推进到“中断处理路径”的入口。**

