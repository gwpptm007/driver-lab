# Day08 深度指南 - platform_driver + probe/remove + devm 资源管理

## 一、Day08 是什么？

Day08 是 W2（嵌入式驱动模式）的第一天，定位是**platform 总线模型 + 设备生命周期管理**。

**核心目标**：从字符设备的"用户态接口层"，切换到 platform 总线的"设备绑定与资源管理层"，建立"设备从哪里来、驱动怎么匹配、资源怎么取、卸载怎么收"的核心认知。

Day08 不做 /dev 接口，也不做 DT。它的重点是：
1. **platform_device 自注册**：模块内注册教学用假设备
2. **同名匹配触发 probe**：device.name == driver.name
3. **probe/remove 生命周期**：资源解析 → 保存 drvdata → devm 托管
4. **devm_* 自动清理**：devm_kzalloc + devm_add_action_or_reset

---

## 二、W2 学习路径中的位置

### 2.1 W2 整体架构

```
W2 (嵌入式驱动模式 - day08-14)
├── day08: platform_driver + probe/remove + devm   ← 今天
├── day09: Device Tree + of_match_table
├── day10: request_irq + top-half + proc 验证
├── day11: top-half + workqueue bottom-half
├── day12: regmap + debugfs 寄存器快照
├── day13: ftrace function_graph 路径观察
└── day14: bring-up checklist
```

### 2.2 Day08 与前后天的关系

```
W1 vs Day08：
  - W1（day01-07）：字符设备基础，核心是 /dev 接口
  - Day08：切换到 platform 总线，核心是设备绑定与资源管理

Day08 vs Day09：
  - Day08：platform_device 是模块内手工注册的
  - Day09：platform_device 来自 Device Tree

Day08 是 W2 的"骨架课"：
  把 platform 总线的生命周期单独拉出来看清楚，
  Day09 再接入 DT
```

---

## 三、为什么 W2 先学 platform_driver？

### 3.1 字符设备 vs platform 驱动的关注点

```
字符设备（W1）：
  关注点：/dev 节点、open/read/write/ioctl、用户态交互
  本质：用户接口层

platform_driver（W2）：
  关注点：设备从哪里来、驱动怎么匹配probe/remove 生命周期、资源怎么取和收
  本质：设备绑定与资源管理层
```

### 3.2 两者不是互斥关系

```
真实驱动里，这两层是配合使用的：

platform_driver.probe()
    ↓
获取 MMIO / IRQ / clock / reset 资源
    ↓
初始化硬件
    ↓
注册 miscdevice 或 cdev（这才是 /dev 层）
    ↓
用户态通过 /dev/xxx 访问
```

### 3.3 Day08 故意不做 /dev

```
把 platform 驱动的核心生命周期单独拉出来看：
  - device 怎么出现
  - driver 怎么注册
  - 匹配成功后 probe 怎么进
  - 卸载时 remove 怎么走
  - devm 怎么自动收资源

Day08 故意不新增 /dev，
先把底层这条链学清楚
```

---

## 四、platform 总线匹配机制

### 4.1 Day08 的同名匹配

```
Day08 用最简单的"同名匹配"：

  demo_platform_device.name = "demo_pdrv"
  demo_platform_driver.driver.name = "demo_pdrv"

匹配条件：
  platform_device.name == platform_driver.driver.name
```

### 4.2 完整注册顺序

```
1. platform_device_register(&demo_platform_device)
   → device 挂到 platform bus 上

2. platform_driver_register(&demo_platform_driver)
   → driver 挂到 platform bus 上

3. platform bus 扫描已存在的设备，找同名
   → 匹配成功

4. 内核调用 demo_probe()
   → probe 开始
```

### 4.3 为什么先注册 device 再注册 driver？

```
如果先注册 driver，再注册 device：
  → driver 注册时扫描已有设备，找不到匹配
  → driver 被挂上 bus，但没有 probe

如果先注册 device，再注册 driver：
  → device 已经在 bus 上
  → driver 注册时立刻触发匹配
  → probe 被调用

但实际上两种顺序都能 work（kernel 会处理），只是教学上"先 device 后 driver"更直观
```

---

## 五、probe() 详解

### 5.1 probe 的语义

```
probe 不是模块入口，也不是用户态 open()。

probe 表示：
  "驱动已经和一个具体设备匹配成功了，现在开始初始化这个设备"
```

### 5.2 Day08 probe 做了什么

```c
// 1. devm_kzalloc() 分配私有数据
priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);

// 2. platform_get_resource() 取 MEM 资源
mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);

// 3. platform_get_irq() 取 IRQ 资源
irq = platform_get_irq(pdev, 0);

// 4. dev_get_platdata() 取 platform_data
pdata = dev_get_platdata(&pdev->dev);

// 5. platform_set_drvdata() 挂载私有数据
platform_set_drvdata(pdev, priv);

// 6. devm_add_action_or_reset() 注册清理回调
return devm_add_action_or_reset(&pdev->dev, demo_devm_cleanup, priv);
```

### 5.3 probe 为什么要检查 np？

```c
if (!np) {
    dev_err(dev, "no device tree node attached\n");
    return -ENODEV;
}
```
```
这个检查是给 DT 场景准备的：
  - 如果设备来自 DT，np = dev->of_node
  - Day09 的设备来自 DT，这个检查通过
  - Day08 的设备是手工注册的，没有 DT node，np = NULL

但当前 probe 里主要用 platform_get_resource / platform_get_irq
它们不依赖 np，所以这个检查对 Day08 不致命
→ 这是教学代码里"留着给 Day09 用"的痕迹
```

---

## 六、devm_* 资源托管

### 6.1 不用 devm 的问题

```
传统写法（probe 中途失败时）：

  priv = kzalloc(...);        // 成功
  mem = request_mem_region(...); // 失败！
  → 需要手工 kfree(priv)
  → 如果漏掉，内存泄漏

devm_* 的思路：
  把资源和"这个 device 的生命周期"绑定
  → 设备解绑时自动释放
  → probe 中途失败也自动释放
  → 不需要大量 goto err_xxx 回滚
```

### 6.2 Day08 用到的 devm

```
devm_kzalloc()：
  - 分配私有数据
  - 设备解绑时自动 kfree
  - 不需要 remove 里手工 kfree

devm_add_action_or_reset()：
  - 注册一个"设备销毁时自动执行的回调"
  - 用于把清理顺序显式打印出来（教学目的）
  - _or_reset 版本：如果注册失败，立刻回滚执行
```

### 6.3 devm_add_action_or_reset 的语义

```c
return devm_add_action_or_reset(&pdev->dev, demo_devm_cleanup, priv);
```
```
含义：
  - 把 demo_devm_cleanup 挂到这个 device 上
  - 设备解绑时，devres 框架自动调用它
  - 如果当前注册失败（内存不足），立刻执行 cleanup 回滚

demo_devm_cleanup() 只是打印日志，不是功能必须：
  → 目的是让 dmesg 里能看到 remove → devm cleanup → release 的顺序
```

---

## 七、remove() 详解

### 7.1 remove 的语义

```
remove = probe 的反向路径

当驱动和设备解绑时，内核调用 remove

职责：
  - 停止硬件工作
  - 注销子设备
  - 关闭中断
  - 让设备回到安全状态
```

### 7.2 Day08 remove 为什么很简单？

```
Day08 的 remove：

  struct demo_priv *priv = platform_get_drvdata(pdev);
  dev_info(...)
  return 0;

没有：
  - kfree(priv)（因为 devm_kzalloc）
  - free_irq()（因为没有真的 request_irq）
  - iounmap()（因为没有真的 ioremap）

devm 让 remove 大幅简化
```

### 7.3 remove ≠ release

```
这是两个不同的概念：

remove（.remove 回调）：
  - 驱动解绑
  - 驱动和设备的绑定关系解除

release（device.release）：
  - 底层 device 对象生命周期结束
  - 当 device 的引用计数归零时调用

dmesg 里的顺序：
  remove → devm cleanup → platform_device release
           ↑ 这一步属于 devres
                                ↑ 这一步才到 device.release
```

---

## 八、完整卸载日志顺序

```
[  104.799875] demo_pdrv: module exit
[  104.800295] demo_pdrv demo_pdrv: remove
[  104.800940] demo_pdrv demo_pdrv: devm cleanup
[  104.801895] demo_pdrv: platform_device release
```

```
module exit：
  - module_exit() 开始执行
  - platform_driver_unregister() 被调用

remove：
  - 驱动和设备解绑
  - .remove() 回调被执行
  - devm cleanup（devm_kzalloc 和 devm_add_action_or_reset 的资源被自动释放）

platform_device release：
  - device 对象引用计数归零
  - .release() 回调被执行
  - 教学用 pdev 的 .release = demo_pdev_release
```

---

## 九、Day08 与 Day09 的关系

### 9.1 Day08 是 Day09 的骨架

```
Day08 建立了：
  - platform_driver 注册
  - probe/remove 生命周期
  - devm_* 资源托管
  - /sys/bus/platform/devices 和 /sys/bus/platform/drivers 可见性

Day09 改动：
  - 不再手工注册 platform_device
  - 改为 of_match_table（DT compatible 匹配）
  - 设备对象来自 DT（内核根据 DT 节点自动创建 pdev）
  - probe 里多一步：读取 DT 原始 cells

Day09 的"骨架"仍然是 Day08 这套
  只是设备描述来源变了
```

### 9.2 Day08 的局限性（为 Day09 留的接口）

```
当前 probe 里有：
  if (!np) { dev_err(...); return -ENODEV; }

这个检查在 Day08 的教学 pdev 场景下 np = NULL
但代码保留着，让 Day09 接入 DT 时能直接 work

说明：
  Day08 和 Day09 是一体设计的
  Day08 先把骨架立住，Day09 替换设备来源
```

---

## 十、面试要会讲的五句话

1. **"Day08 的核心是从 W1 的字符设备'用户接口层'切换到 W2 的 platform 总线'设备绑定与资源管理层'，重点是：platform_device 怎么出现、platform_driver 怎么匹配、probe/remove 怎么被调用、devm 怎么自动收资源"**
   → 理解 Day08 的定位

2. **"probe() 不是模块入口，也不是 open()，它表示'驱动和设备匹配成功了，现在开始初始化设备'；remove() 是它的反向路径，表示解绑；两者都不等于 release()"**
   → 理解 probe/remove/release 的区别

3. **"devm_kzalloc() 的核心是把内存生命周期绑定到 device：设备解绑时自动释放，probe 中途失败也自动释放，不需要 remove 里手工 kfree；devm_add_action_or_reset() 可以注册设备销毁时的清理回调"**
   → 理解 devm_* 的价值

4. **"Day08 的 remove 里没有 kfree/irq_free/iounmap，因为都是 devm_* 托管的；dmesg 里看到 remove → devm cleanup → platform_device release 的顺序，分别对应驱动解绑、devres 自动清理、device 对象真正销毁"**
   → 理解 devm 和 remove 的关系

5. **"Day08 和 Day09 的关系是'骨架相同，设备来源不同'：Day08 用模块内手工注册的 platform_device 同名匹配，Day09 用 Device Tree compatible + of_match_table 匹配；probe/remove/devm 的写法几乎不变"**
   → 理解 Day08 → Day09 的演进

---

## 十一、验收标准

### 11.1 probe 验收

- [ ] insmod demo_pdrv.ko 成功
- [ ] dmesg | grep demo_pdrv 显示：
  - `probe start`
  - `MEM resource: start=... end=... size=...`
  - `IRQ resource: 11`
  - `platform_data: version=1 label=w2-day08-platform-lab`

### 11.2 platform bus 可见性验收

- [ ] `ls /sys/bus/platform/devices | grep demo_pdrv` 有输出
- [ ] `ls /sys/bus/platform/drivers | grep demo_pdrv` 有输出

### 11.3 remove 验收

- [ ] rmmod demo_pdrv 成功
- [ ] dmesg 显示：
  - `remove: label=w2-day08-platform-lab version=1 irq=11`
  - `devm cleanup: ...`
  - `platform_device release`

---

## 附录：Day08 核心代码路径

```
insmod
    ↓
demo_init()
    → platform_device_register(&demo_platform_device)
    → platform_driver_register(&demo_platform_driver)
    ↓
[demo_probe() 被调用]
    → devm_kzalloc() → priv
    → platform_get_resource() → mem
    → platform_get_irq() → irq
    → dev_get_platdata() → pdata
    → platform_set_drvdata() → 挂 priv
    → devm_add_action_or_reset() → 注册清理回调
    ↓
rmmod
    ↓
demo_exit()
    → platform_driver_unregister()
    ↓
[demo_remove() 被调用]
    → platform_get_drvdata() → 取 priv
    → 打印日志
    → 返回（priv 由 devm 自动释放）
    ↓
[devm cleanup 被自动调用]
    → demo_devm_cleanup()
    ↓
[platform_device release 被调用]
    → demo_pdev_release()
```
