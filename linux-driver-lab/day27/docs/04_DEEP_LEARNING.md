# Day27 深度学习指南

## 一、Day27 是什么？

Day27 的核心目标是**验证驱动在重复装载/卸载下的稳定性**。

Day26 追求"用户态友好接口"，Day27 则追求"可重复卸载的健壮驱动"。

| 特性 | Day26 | Day27 |
|------|-------|-------|
| 核心目标 | 用户态友好接口 | insmod/rmmod 循环稳定性 |
| 循环测试 | 无 | 200 次循环 |
| probe 复杂度 | 读取 ID/LIVENESS | 最小化，仅必需操作 |
| MSI 申请 | 只申请 MSI | MSI 优先，失败时回退到 LEGACY |
| remove 对称性 | 基本对称 | **严格对称**，逐项核对 |

---

## 二、为什么需要循环卸载测试？

### 2.1 驱动卸载是"反向初始化"

内核模块的加载（init）和卸载（exit）必须严格对称：
- `insmod` 分配的资源 → `rmmod` 必须释放
- `probe` 设置的状态 → `remove` 必须还原
- 任何遗漏都会导致：资源泄漏、孤立进程、系统不稳定

### 2.2 长期稳定性问题

单次 insmod/rmmod 可能看起来正常，但重复 200 次后：
- 内存泄漏会累积
- 资源句柄会耗尽
- 竞态条件会暴露
- 隐藏的指针错误会显现

### 2.3 Day27 的验收标准

```
loop_count=200
pass=200
fail=0
```

- 无 `BUG:`、`Oops:`、`Kernel panic`
- 无 `hung task`（任务挂起）
- `probe success` 和 `remove leave` 出现次数一致

---

## 三、与 Day26 的对比

### 3.1 结构体对比

```c
/* Day26 多了一个 identity_value 和 liveness 相关字段 */
struct day26_dev {
    ...
    u32 identity_value;          /* Day26 有，Day27 没有 */
    u32 liveness_value;         /* Day26 有，Day27 没有 */
    u32 liveness_inverted;       /* Day26 有，Day27 没有 */
    ...
};

/* Day27 更精简，只保留中断相关核心字段 */
struct day27_dev {
    struct pci_dev *pdev;
    void __iomem *bar0;
    resource_size_t bar0_start;
    resource_size_t bar0_len;
    unsigned int irq_vector;
    u64 irq_count;
    u32 last_irq_status;
    u32 last_ack_value;
    spinlock_t irq_lock;
    dev_t devt;
    struct cdev cdev;
    struct device *device;
};
```

**设计思路**：Day26 在 probe 时读取硬件标识（LIVENESS）用于验证 MMIO 正确性；Day27 假设链路已打通，简化 probe，专注稳定性测试。

### 3.2 MSI 申请对比

```c
/* Day26：只申请 MSI */
ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);

/* Day27：MSI 优先，失败时回退到 LEGACY（传统中断） */
ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
```

**设计思路**：Day27 考虑了更广泛的环境兼容性。有些虚拟化环境可能不支持 MSI，必须允许回退。

---

## 四、remove 对称性详解

### 4.1 probe 资源分配顺序

```
1. kzalloc(day27_dev)           // 分配私有数据结构
2. pci_enable_device()           // 启用 PCI 设备
3. pci_request_regions()         // 请求 BAR 资源
4. pci_set_master()             // 设置为主设备
5. pci_iomap(BAR0)             // 映射 BAR0 MMIO
6. pci_alloc_irq_vectors()      // 分配中断向量
7. request_irq()                // 注册中断处理函数
8. day27_setup_chrdev()        // 注册字符设备
```

### 4.2 remove 资源释放顺序（严格反向）

```
1. day27_destroy_chrdev()       // 销毁字符设备（cdev_del + device_destroy）
2. free_irq()                   // 释放中断处理函数
3. pci_free_irq_vectors()       // 释放中断向量
4. pci_iounmap()               // 解除 BAR0 MMIO 映射
5. pci_release_regions()       // 释放 BAR 资源
6. pci_disable_device()        // 禁用 PCI 设备
7. kfree()                      // 释放私有数据结构
```

### 4.3 对称性检查表

| probe 分配 | remove 释放 | 检查点 |
|------------|-------------|--------|
| `kzalloc` | `kfree` | 内存不泄漏 |
| `pci_enable_device` | `pci_disable_device` | 设备状态还原 |
| `pci_request_regions` | `pci_release_regions` | BAR 资源回收 |
| `pci_iomap` | `pci_iounmap` | MMIO 映射解除 |
| `pci_alloc_irq_vectors` | `pci_free_irq_vectors` | 中断向量归还 |
| `request_irq` | `free_irq` | 中断处理函数注销 |
| `cdev_add` | `cdev_del` | 字符设备从 VFS 移除 |
| `device_create` | `device_destroy` | sysfs 设备节点删除 |

---

## 五、完整调用链

### 5.1 模块加载（insmod）

```
用户执行 insmod day27_edu_loop.ko
    ↓
day27_init()                    // module_init()
    ↓
alloc_chrdev_region()           // 分配主设备号（32 个 minor）
    ↓
class_create()                  // 创建 /sys/class/day27_edu/
    ↓
pci_register_driver()           // 注册 PCI 驱动
    ↓
pci_bus_driver.probe()          // PCI 总线枚举匹配到 1234:11e8
    ↓
day27_probe()                   // 探针函数
    ├→ kzalloc(day27_dev)                  // 分配私有数据
    ├→ spin_lock_init()                    // 初始化自旋锁
    ├→ pci_enable_device()                  // 启用 PCI 设备
    ├→ pci_request_regions()               // 请求 BAR 资源
    ├→ pci_set_master()                    // 设置为主设备
    ├→ pci_iomap(BAR0)                    // 映射 BAR0 MMIO
    ├→ pci_alloc_irq_vectors(MSI|LEGACY)  // 分配中断向量（允许回退）
    ├→ request_irq()                       // 注册中断处理函数
    ├→ day27_setup_chrdev()               // 创建字符设备节点
    └→ return 0
    ↓
pr_info("day27_edu_loop: loaded")
```

### 5.2 单次循环 Smoke 测试

```
每轮循环内执行一次 smoke：
    ├→ day27_edu_tool /dev/day27_edu0 trigger 1
    │     └→ write() → IRQ_RAISE → MSI 中断 → irq_handler
    ├→ day27_edu_tool /dev/day27_edu0 count
    │     └→ ioctl(GET_IRQ_COUNT) → irq_count > 0 → pass
    └→ rmmod day27_edu_loop
          └→ day27_exit() → pci_unregister_driver() → day27_remove()
```

### 5.3 模块卸载（rmmod）

```
用户执行 rmmod day27_edu_loop
    ↓
day27_exit()                    // module_exit()
    ↓
pci_unregister_driver()         // 注销 PCI 驱动（阻止新设备匹配）
    ↓
pci_bus_driver.remove()         // 触发已加载设备的 remove
    ↓
day27_remove(pdev)              // 移除函数
    ├→ day27_destroy_chrdev()               // 销毁字符设备
    ├→ free_irq()                           // 释放中断
    ├→ pci_free_irq_vectors()               // 释放中断向量
    ├→ pci_iounmap()                       // 解除 MMIO 映射
    ├→ pci_release_regions()               // 释放 BAR
    ├→ pci_disable_device()                // 禁用设备
    └→ kfree()                             // 释放内存
    ↓
class_destroy()                  // 销毁 sysfs 类
unregister_chrdev_region()       // 释放设备号
    ↓
pr_info("day27_edu_loop: unloaded")
```

---

## 六、MSI 中断与 remove 的关系

### 6.1 MSI 中断的申请/释放必须匹配

```c
// probe 中
pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
request_irq(d->irq_vector, day27_irq_handler, ...);

// remove 中（顺序很重要）
free_irq(d->irq_vector, d);        // 先释放 IRQ
pci_free_irq_vectors(pdev);        // 再释放向量
```

**为什么顺序重要？**
- `free_irq` 需要使用 `irq_vector`，必须在 `pci_free_irq_vectors` 之前调用
- 释放顺序错误会导致使用已释放的中断向量

### 6.2 中断处理函数的注册时机

```c
// request_irq 在 MSI 向量分配之后
d->irq_vector = pci_irq_vector(pdev, 0);  // 先获取向量号
ret = request_irq(d->irq_vector, day27_irq_handler, 0, ...);  // 后注册
```

---

## 七、200 次循环测试的观测性设计

### 7.1 每轮 smoke 测试内容

每轮循环执行最小测试：
1. 打开 `/dev/day27_edu0`
2. `trigger 1` 触发一次中断
3. `count` 确认 `irq_count > 0`
4. 记录 pass/fail
5. 卸载模块

### 7.2 观测点

```
dmesg 中反复出现：
    probe enter: 1234:11e8
    probe success
    irq handler: irq=XX status=0x... count=YY
    remove enter
    remove leave
```

### 7.3 失败模式

如果 remove 不对称，会观察到：
- `Oops:` 或 `BUG:` 在后续循环中出现
- `dmesg` 显示资源泄漏警告
- `proc/interrupts` 显示孤立中断计数

---

## 八、错误处理与健壮性

### 8.1 probe 错误处理

```c
static int day27_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    d = kzalloc(...);
    if (!d) return -ENOMEM;

    ret = pci_enable_device(pdev);
    if (ret) goto err_free;        // 只有 kfree 需要

    ret = pci_request_regions(pdev, ...);
    if (ret) goto err_disable;     // 需要 disable + free

    ret = pci_alloc_irq_vectors(pdev, ...);
    if (ret < 0) goto err_iounmap; // 需要 iounmap + release + disable + free
    ...
}
```

**错误标签设计原则**：
- 每个标签跳转到对应的清理代码
- 清理顺序与分配顺序相反
- 已经成功的步骤必须在更后面的错误处理中再次清理

### 8.2 remove 的防御性检查

```c
static void day27_remove(struct pci_dev *pdev)
{
    struct day27_dev *d = pci_get_drvdata(pdev);
    if (!d) return;  // 防御性检查：防止重复 remove

    day27_destroy_chrdev(d);  // 内部有 if (d->device) 检查
    free_irq(d->irq_vector, d);
    pci_free_irq_vectors(pdev);
    if (d->bar0)              // 防御性检查：bar0 可能映射失败
        pci_iounmap(pdev, d->bar0);
    ...
}
```

---

## 九、完整数据流图

```
【200 次循环架构】

┌─────────────────────────────────────────────────────────────────────┐
│                      用户态（Guest QEMU）                             │
│                                                                     │
│  for i in {1..200}:                                                 │
│      insmod day27_edu_loop.ko                                        │
│          ↓                                                          │
│      /dev/day27_edu0 opened                                         │
│      write("1") → trigger IRQ                                       │
│      read irq_count → verify > 0                                     │
│          ↓                                                          │
│      rmmod day27_edu_loop                                           │
│          ↓                                                          │
│      if fail: break                                                 │
│                                                                     │
│  最终输出：loop-summary.txt                                          │
└─────────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────────┐
│                      Linux 内核（内核空间）                          │
│                                                                     │
│  day27_init()                                                       │
│      ├→ alloc_chrdev_region()                                        │
│      ├→ class_create()                                               │
│      └→ pci_register_driver()                                        │
│                                                                     │
│  day27_probe() ──────────────────────→ [循环第 i 次]                │
│      ├→ kzalloc(day27_dev)                                          │
│      ├→ pci_enable_device()                                        │
│      ├→ pci_request_regions()                                       │
│      ├→ pci_iomap(BAR0)                                             │
│      ├→ pci_alloc_irq_vectors(MSI|LEGACY)                          │
│      ├→ request_irq()                                               │
│      └→ day27_setup_chrdev()                                        │
│                                                                     │
│  day27_irq_handler() ←──────────────────── [每次 trigger]           │
│      ├→ readl(BAR0 + IRQ_STATUS)                                   │
│      ├→ irq_count++                                                │
│      └→ writel(status, BAR0 + IRQ_ACK)                              │
│                                                                     │
│  day27_remove() ──────────────────────→ [每次 rmmod]                 │
│      ├→ day27_destroy_chrdev()                                     │
│      ├→ free_irq()                                                 │
│      ├→ pci_free_irq_vectors()                                     │
│      ├→ pci_iounmap()                                              │
│      ├→ pci_release_regions()                                     │
│      ├→ pci_disable_device()                                       │
│      └→ kfree()                                                    │
│                                                                     │
│  day27_exit()                                                       │
│      ├→ pci_unregister_driver()                                    │
│      ├→ class_destroy()                                            │
│      └→ unregister_chrdev_region()                                 │
└─────────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────────┐
│                      EDU 硬件（QEMU 模拟）                          │
│                                                                     │
│  QEMU EDU device (1234:11e8)                                        │
│      ├→ BAR0: MMIO 寄存器 (256 bytes)                              │
│      │     ├→ IRQ_STATUS (0x24)                                    │
│      │     ├→ IRQ_RAISE (0x60)                                    │
│      │     └→ IRQ_ACK (0x64)                                      │
│      └→ MSI 中断控制器                                              │
│              ↑                                                      │
│              │ MSI 写内存特殊地址                                    │
│              │                                                      │
│              └──────────────────────────────────→ CPU 中断          │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 十、关键设计思想

### 10.1 简化即健壮

Day27 去掉了 Day26 的 `identity_value` 和 `liveness_value` 读取：
- 原因：200 次循环中这些验证是多余的
- 目的：减少 probe 中的操作，降低出错概率
- 思想：每轮循环只做必须做的事

### 10.2 错误回退设计

```c
pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
```

允许 MSI 失败时回退到传统中断，确保在各种虚拟化环境下都能工作。

### 10.3 观测性日志

每次 probe/remove/interrupt 都有 `dev_info` 日志：
- 便于调试定位问题
- 便于自动化脚本解析验证
- 便于人工观察循环是否正常

---

## 十一、常见问题与排查

### 11.1 资源泄漏

**症状**：`lsmod` 显示模块无法卸载，或 `dmesg` 显示资源警告

**排查**：
1. 检查 `dmesg | grep "fail"` 或 `"error"`
2. 检查 `proc/driver/*` 是否有残留
3. 使用 `leak` 工具检测内存泄漏

### 11.2 中断句柄未释放

**症状**：第二次 insmod 时 `request_irq` 失败（IRQ 已占用）

**排查**：
1. `dmesg | grep request_irq`
2. 检查 `free_irq` 是否在 `pci_free_irq_vectors` 之前调用

### 11.3 MMIO 映射未解除

**症状**：`pci_iomap` 第二次调用失败

**排查**：
1. 检查 `pci_iounmap` 是否调用
2. 检查 `bar0` 是否检查 NULL

---

## 十二、与 W3（day17-21）回归测试的对比

| 特性 | W3 回归测试 | Day27 循环测试 |
|------|-------------|----------------|
| 目标 | 功能正确性 | 长期稳定性 |
| 循环次数 | 较少（stress 测试） | 200 次 |
| 硬件环境 | 真实硬件/虚拟化 | QEMU EDU 设备 |
| 关注点 | 功能回归 | 资源泄漏、remove 对称性 |
| 失败表现 | 功能测试失败 | 系统级问题（Oops/panic） |
