# Day08 - platform_driver + probe/remove + devm 资源管理

## 0. Day08 一句话总结

Day08 的重点不是继续扩展 `/dev/demo` 这条字符设备线，而是切换到 Linux 设备模型里的 **platform 总线模型**，把下面这条链路单独学透：

```text
platform_device 注册
        ↓
platform_driver 注册
        ↓
platform bus 匹配
        ↓
probe() 获取资源并初始化
        ↓
remove() 执行解绑逻辑
        ↓
devm 自动清理资源
        ↓
device release
```

这一天故意不新增 `/dev` 节点。目标是先把 **驱动如何绑定设备、如何拿资源、如何释放资源** 这条主线搞清楚。

---

## 1. 本日学习目标

### 1.1 任务

- 理解 `platform_driver` 的定位和工作方式
- 理解 `probe/remove` 生命周期
- 理解 `devm_*` 资源托管机制
- 在当前 QEMU 学习环境中完成一次完整验证

### 1.2 产出与验收

- `probe()` 能打印平台资源
- `remove()` 后资源释放路径清晰可见
- 能在 `/sys/bus/platform/devices` 和 `/sys/bus/platform/drivers` 中看到绑定关系

---

## 2. 为什么 W2 先学 platform_driver

前面 Day01 到 Day06 主要围绕字符设备展开，重点是：

- `alloc_chrdev_region`
- `cdev_add`
- `class_create`
- `device_create`
- `file_operations`
- 用户态通过 `/dev/demo` 与驱动交互

这一套更偏向 **用户接口层**，它解决的是：

> 怎么把驱动能力暴露给用户态

而嵌入式驱动更常见的起点并不是 `/dev`，而是：

> 内核里已经有一个设备对象了，驱动如何和这个设备对象绑定起来

这就是 platform 总线模型的核心问题。

在很多 SoC 场景中，外设是板上固定存在的，不是像 PCIe 那样动态枚举，也不是像 USB 那样热插拔发现。内核会先拿到设备描述，再创建 `platform_device`，驱动侧则用 `platform_driver` 去匹配这个设备。匹配成功之后才进入 `probe()`。

所以 platform_driver 更像是在回答这几个问题：

- 这个设备是谁
- 它有哪些资源
- 驱动什么时候开始接管它
- 解绑时资源怎么回收

---

## 3. platform_driver 和前面 `/dev` 驱动的本质区别

这是 Day08 最关键的理解点。

### 3.1 字符设备驱动的关注点

字符设备那条线主要关注接口：

- 主次设备号
- `/dev/xxx`
- `open/read/write/ioctl`
- 用户态和驱动之间的通信

它更像是 **访问入口层**。

### 3.2 platform_driver 的关注点

platform_driver 主要关注设备绑定和资源管理：

- 系统里有一个 `struct device`
- 这个设备属于 `platform bus`
- 它携带若干资源
- 驱动注册后和设备匹配
- 匹配成功后进入 `probe()`
- 卸载或解绑时进入 `remove()`

它更像是 **设备生命周期层**。

### 3.3 两者不是互斥关系

真实驱动里，这两层经常是同时存在的：

```text
platform_driver.probe()
    ↓
获取 MMIO / IRQ / clock / reset 资源
    ↓
初始化硬件
    ↓
注册 miscdevice 或 cdev
    ↓
用户态通过 /dev/xxx 访问
```

所以：

- `platform_driver` 负责找到并接管设备
- `cdev` 或 `miscdevice` 负责把能力暴露给用户态

Day08 故意不做 `/dev`，是为了先把底层这条链学清楚。

---

## 4. platform 总线模型到底是什么

可以把 platform bus 理解成内核里的一个“配对中心”。

它主要做三件事：

1. 维护平台设备列表
2. 维护平台驱动列表
3. 尝试让设备和驱动匹配

当你注册一个 `platform_device` 时，它会挂到 platform bus 上。

当你注册一个 `platform_driver` 时，它也会挂到 platform bus 上。

只要匹配规则满足，内核就会调用这个 driver 的 `probe()`。

### 4.1 Day08 的匹配方式

Day08 采用最基础的 **同名匹配**：

```c
#define DRV_NAME "demo_pdrv"
```

然后：

- `demo_platform_device.name = DRV_NAME`
- `demo_platform_driver.driver.name = DRV_NAME`

也就是说：

```text
platform_device.name == platform_driver.driver.name
```

只要两边名字一致，platform bus 就能把它们绑定起来。

### 4.2 为什么后面还要学 Device Tree

因为真实项目里，一般不会让驱动自己手工注册一个教学用 `platform_device`。

更常见的情况是：

- Device Tree 里写一个节点
- 内核根据 DT 创建 `platform_device`
- 驱动通过 `of_match_table` 去匹配这个节点

所以可以把 Day08 理解为：

- **先学 platform_driver 的骨架和生命周期**
- Day09 再学 **设备对象从 DT 来时，这条链怎么落地**

---

## 5. Day08 的设计思路

当前环境是 x86 + QEMU + initramfs 学习环境，不是现成 ARM 开发板 BSP。

如果一上来就把 platform 总线和 Device Tree 放在同一天讲，初学时很容易把“总线模型”和“设备描述来源”混在一起。

所以 Day08 故意这样拆：

- 模块内部自带一个教学用 `platform_device`
- 同时注册一个同名 `platform_driver`
- 通过同名匹配触发 `probe()`
- 通过 `rmmod` 触发 `remove()`、`devm cleanup` 和 `release`

这样可以把 platform 驱动的核心生命周期单独拉出来看。

---

## 6. Day08 代码结构

```text
day08/
├── demo_pdrv.c
├── Makefile
├── build.sh
└── README.md
```

### 6.1 `demo_pdrv.c`

核心内容：

- 教学用 `platform_device`
- `platform_driver`
- `probe/remove`
- `devm_kzalloc()`
- `devm_add_action_or_reset()`
- `platform_data`
- 资源打印与生命周期日志

### 6.2 `build.sh`

沿用前面 QEMU 学习环境的风格：

- 编译内核模块
- 组装最小 rootfs
- 启动 QEMU

### 6.3 为什么没有 `test.c`

因为这一天不是 `/dev` 接口实验。

Day08 的主要操作是：

- `insmod`
- `rmmod`
- `dmesg`
- 查看 `/sys/bus/platform/devices`
- 查看 `/sys/bus/platform/drivers`

---

## 7. `probe()` 到底做什么

`probe()` 不是模块入口，也不是用户态 `open()`。

它表示：

> 驱动已经和一个具体设备匹配成功了，现在开始初始化这个设备

在真实驱动里，`probe()` 常做这些事情：

1. 分配私有数据
2. 读取平台资源
3. `ioremap` 寄存器
4. 申请 IRQ
5. 初始化时钟、复位、电源
6. 注册字符设备、input、netdev、miscdevice 等子接口

Day08 只保留最核心的前几步：

1. `devm_kzalloc()` 分配私有数据
2. `platform_get_resource()` 读取 MEM 资源
3. `platform_get_irq()` 读取 IRQ 资源
4. `dev_get_platdata()` 读取 `platform_data`
5. `platform_set_drvdata()` 挂驱动私有数据

这样能先把主线看清楚。

---

## 8. `remove()` 到底做什么

`remove()` 可以理解成 `probe()` 的反向路径。

当驱动和设备解绑时，内核会调用它。

常见职责包括：

- 停止硬件工作
- 注销子设备
- 关闭中断
- 让设备回到安全状态

但要特别注意：

> `remove()` 不等于所有资源都必须手工释放

如果你使用的是 `devm_*` 系列接口，很多资源会在设备解绑时自动回收。

---

## 9. `devm_*` 到底解决什么问题

`devm` 可以理解成 **device managed resource**。

意思是：

> 把资源和这个 `device` 的生命周期绑定起来

### 9.1 不用 `devm_*` 时常见的问题

传统写法里，`probe()` 里可能有很多步：

1. `kzalloc`
2. `ioremap`
3. `request_irq`
4. `clk_prepare_enable`
5. `device_create_file`

如果中途某一步失败，你就需要手工回滚前面已经成功的步骤。于是代码里常常会出现一长串 `goto err_xxx`。

这种代码不是不能写，而是：

- 容易漏释放
- 错误路径长
- 可读性差
- 维护成本高

### 9.2 `devm_*` 的思路

用了 `devm_*` 之后，很多资源会在两种场景自动回收：

- `probe()` 中途失败
- 设备解绑

这样就能显著减少手工回滚代码。

### 9.3 Day08 里用了什么

本例用了两个最适合教学的点：

- `devm_kzalloc()`
- `devm_add_action_or_reset()`

#### `devm_kzalloc()`

说明：

- `priv` 不需要在 `remove()` 里手工 `kfree`
- 设备解绑时内核会自动释放这块内存

#### `devm_add_action_or_reset()`

说明：

- 可以给这个设备挂一个“解绑时自动执行的清理动作”
- 如果当前注册失败，它还会立刻回滚执行

Day08 加这个接口不是因为功能必须，而是因为它非常适合把 **devm 自动清理顺序** 明确打印出来。

---

## 10. Day08 实验环境和操作方式

### 10.1 编译与启动

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/day08
chmod +x build.sh
./build.sh
```

进入 QEMU 后，主要用下面这些命令验证：

```sh
insmod /demo_pdrv.ko
rmmod demo_pdrv
dmesg | tail -n 50
ls /sys/bus/platform/devices
ls /sys/bus/platform/drivers
```

### 10.2 实验观察点

这一天重点不是功能读写，而是观察：

- 模块加载后 `probe()` 是否进入
- 资源是否打印正确
- `demo_pdrv` 是否出现在 platform bus 里
- 模块卸载后 `remove()`、`devm cleanup`、`release` 顺序是否正确

---

## 11. 实际测试过程记录

下面是本次 Day08 的实际测试过程整理。

### 11.1 加载模块并观察 `probe`

执行：

```sh
insmod /demo_pdrv.ko
dmesg | tail -n 50
```

实际日志：

```text
[   23.098491] demo_pdrv: loading out-of-tree module taints kernel.
[   23.102051] demo_pdrv: module init
[   23.102537] demo_pdrv demo_pdrv: probe start
[   23.102923] demo_pdrv demo_pdrv: MEM resource: start=0x10000000 end=0x10000fff size=0x1000
[   23.103368] demo_pdrv demo_pdrv: IRQ resource: 11
[   23.103533] demo_pdrv demo_pdrv: platform_data: version=1 label=w2-day08-platform-lab
[   23.104067] demo_pdrv: module init ok
```

#### 这一步说明了什么

说明 `module_init()` 执行后：

- `platform_device_register()` 成功
- `platform_driver_register()` 成功
- platform bus 完成了同名匹配
- `probe()` 正常进入
- MEM 资源获取成功
- IRQ 资源获取成功
- `platform_data` 获取成功

也就是说，Day08 的 **probe 路径已经跑通**。

---

### 11.2 查看 platform bus 里的设备与驱动

执行：

```sh
ls /sys/bus/platform/devices
ls /sys/bus/platform/drivers
```

实际输出：

```text
~ # ls /sys/bus/platform/devices
Fixed MDIO bus.0        demo_pdrv               regulatory.0
PNP0103:00              i8042                   serial8250
QEMU0002:00             pcspkr
alarmtimer.0.auto       platform-framebuffer.0

~ # ls /sys/bus/platform/drivers
acpi-fan      alarmtimer    demo_pdrv     i8042
acpi-ged      clk-pmc-atom  gpio-clk      serial8250
```

#### 这一步说明了什么

说明：

- `demo_pdrv` 已经作为一个 **platform_device** 挂到了 `/sys/bus/platform/devices`
- `demo_pdrv` 对应的 **platform_driver** 已经挂到了 `/sys/bus/platform/drivers`
- 设备对象和驱动对象都确实存在于 platform bus 中

这一点非常关键，因为它把“设备对象”和“用户态 `/dev` 节点”区分开了。

这里看到的是 **内核设备模型里的对象**，不是字符设备接口。

---

### 11.3 卸载模块并观察 `remove/devm/release`

执行：

```sh
rmmod demo_pdrv
dmesg | tail -n 50
```

实际日志：

```text
[  104.799875] demo_pdrv: module exit
[  104.800295] demo_pdrv demo_pdrv: remove: label=w2-day08-platform-lab version=1 irq=11
[  104.800940] demo_pdrv demo_pdrv: devm cleanup: label=w2-day08-platform-lab version=1 irq=11
[  104.801895] demo_pdrv: platform_device release
```

#### 这一步说明了什么

这组日志顺序非常重要：

```text
module exit
-> remove
-> devm cleanup
-> platform_device release
```

它分别表示：

1. 模块退出流程开始
2. 驱动和设备解绑，进入 `remove()`
3. `devm_*` 管理的资源开始自动清理
4. `struct device` 对象生命周期结束，进入 `release()`

这说明 Day08 的 **remove 路径也完整跑通了**。

---

## 12. 实际测试结果归纳

### 12.1 `probe` 路径

**结果：通过**

证据：

- 有 `probe start`
- 有 `MEM resource`
- 有 `IRQ resource`
- 有 `platform_data`

### 12.2 platform bus 可见性

**结果：通过**

证据：

- `/sys/bus/platform/devices` 中可以看到 `demo_pdrv`
- `/sys/bus/platform/drivers` 中可以看到 `demo_pdrv`

### 12.3 `remove` 释放路径

**结果：通过**

证据：

- 有 `remove`
- 有 `devm cleanup`
- 有 `platform_device release`

### 12.4 本日总体验收

Day08 的 D7 任务：

- `platform_driver probe/remove`
- `devm_*` 资源管理

**验收结论：完整通过**

---

## 13. 本次实验得到的核心结论

### 13.1 `module_init()` 不等于 `probe()`

- `module_init()` 是模块级入口
- `probe()` 是驱动和设备匹配成功后的设备级入口

这个区别在字符设备实验里不明显，但在平台驱动里必须分清。

### 13.2 `remove()` 不等于 `release()`

- `remove()` 表示驱动解绑
- `release()` 表示底层 `device` 对象生命周期结束

所以 `remove` 和 `release` 不是一回事，不能混为一个概念。

### 13.3 platform_driver 不等于字符设备

- platform_driver 负责设备绑定和资源管理
- 字符设备负责给用户态提供 `/dev` 访问入口

二者不是替代关系，而是经常配合使用。

### 13.4 `devm_*` 的价值是真正减少错误路径复杂度

Day08 虽然只是个教学 demo，但已经能清楚看到：

- `devm_kzalloc()` 让私有数据不必手工释放
- `devm_add_action_or_reset()` 可以把自动清理动作显式可视化

在真实驱动里，当资源变多时，`devm_*` 的价值会更明显。

### 13.5 当前 Day08 学到的是 platform 驱动骨架

Day08 还没有进入：

- DT 节点匹配
- 真正的 MMIO 映射
- 真正的中断申请
- regmap 访问

但这并不代表内容少。

恰恰相反，Day08 把最重要的“骨架”先立住了：

- 设备从哪里来
- 驱动怎么匹配
- `probe/remove` 什么时候进
- 资源怎么托管

这一步立住，后面 day09 和 day10 才不会乱。

---

## 14. 代码阅读建议

建议按下面顺序阅读 `demo_pdrv.c`：

1. `struct demo_priv`
   - 看平台设备私有数据如何组织
2. `demo_probe()`
   - 看资源如何取出来
   - 看 `devm_kzalloc()` 怎么用
   - 看 `platform_set_drvdata()` 怎么挂私有数据
3. `demo_remove()`
   - 看为什么这里不用手工 `kfree`
4. `demo_devm_cleanup()`
   - 看 devm 自动清理如何通过日志展示出来
5. `demo_platform_device`
   - 看教学用资源如何放进去
6. `demo_init()` / `demo_exit()`
   - 看注册顺序和注销顺序

---

## 15. 面试或学习时怎么描述 Day08

可以这样表述：

> 在 W2 的第一天，我先没有直接做 Device Tree 和真实寄存器访问，而是先把 platform 总线模型单独跑通。我在模块里注册了一个教学用 platform_device，再注册一个 platform_driver，通过同名匹配触发 probe。probe 里用 `platform_get_resource()` 读取 MEM 和 IRQ 资源，用 `devm_kzalloc()` 管理私有数据，并通过 `devm_add_action_or_reset()` 显式展示设备解绑时的自动清理顺序。最终通过 `/sys/bus/platform/devices`、`/sys/bus/platform/drivers` 以及 `remove -> devm cleanup -> release` 的日志顺序，验证了平台驱动的绑定、解绑和资源托管机制。

---

## 16. 下一步怎么接

Day09 很自然会接到 Device Tree：

- 去掉模块里手工注册的教学用 `platform_device`
- 改成 `of_match_table`
- 让资源从 DT 的 `reg` 和 `interrupts` 里来

到那时你会明显感觉到：

> 设备描述来源变了，但 `probe()` 的主体思路并没有变

这就是 Day08 的价值。
