# Day25 深度学习指南

## 一、day25 到底要学什么

### 1. 核心问题：PCI 设备如何发中断给 CPU？

day24 实现了驱动与用户态通过 BAR2 共享内存通信。但这只是"软件层面的协议"，设备如果需要**主动通知** CPU 事件（如"数据已到达"、"硬件错误"），就需要**中断机制**。

day25 的核心问题是：**QEMU 模拟的 EDU 设备，如何通过 MSI（Message Signaled Interrupt）向 CPU 发中断？**

### 2. day24 vs day25 的本质区别

| | day24 | day25 |
|---|---|---|
| 设备 | ivshmem-plain (1af4:1110) | EDU teaching device (1234:11e8) |
| 中断 | 无（plain 没有 doorbell） | **有 MSI** |
| 字符设备框架 | miscdevice | **alloc_chrdev_region + class + cdev** |
| 关键 API | pci_iomap / misc_register | **pci_alloc_irq_vectors / request_irq** |
| 验证证据 | dmesg + read/write | **dmesg + irq_count + /proc/interrupts** |
| 用户态动作 | shm-write/shm-read | **trigger（主动触发中断）** |

### 3. 为什么用 EDU 而不是 ivshmem-doorbell？

```
ivshmem-doorbell (1af4:1111) 的问题：
  → 中断触发靠写 BAR0 寄存器，需要知道 doorbell 机制
  → MSI 概念被设备自己的寄存器隐藏了

EDU 设备 (1234:11e8) 的优势：
  → QEMU 官方教学设备，专为学习设计
  → 寄存器文档清晰：ID/LIVENESS/IRQ_STATUS/IRQ_RAISE/IRQ_ACK
  → MSI 触发方式简单：往 IRQ_RAISE 写值 → CPU 收到 MSI → handler 被调用
```

### 4. EDU 设备详解

**EDU = QEMU Virtualization Teaching Device**

QEMU 官方提供的教学用 PCI 设备，专门帮助开发者学习 PCI 驱动开发和 MSI 中断机制。

```
基本信息：
  Vendor ID:    1234
  Device ID:    11e8
  全称:         QEMU Virtualization Teaching Device
  BAR0 大小:    4KB（寄存器空间）
  中断类型:     MSI（Message Signaled Interrupt）
```

**为什么选 EDU**

```
vs ivshmem：
  ivshmem 文档不清晰，需要研究 QEMU 源码才能理解 doorbell 机制
  EDU 是专门的教学设备，寄存器布局清晰明确

EDU 的设计目标：
  → 足够简单，适合入门 PCI 驱动
  → 但又包含完整的中断机制（MSI）
  → 寄存器可以随便读写，不会损坏真实硬件
```

**BAR0 寄存器布局**

```
EDU BAR0 寄存器（都是 32-bit MMIO）：

  0x00  ID           只读   → 设备ID（QEMU_EDU_ID），验证驱动连接
  0x04  LIVENESS    读写   → 写入 0xa5a5aa5a，设备返回按位取反值
  0x20  STATUS      只读   → 设备状态寄存器
  0x24  IRQ_STATUS  只读   → 当前等待中的中断源（多个位可以同时为1）
  0x60  IRQ_RAISE   只写   → 写入任意值触发 MSI 中断（值会出现在 handler 中）
  0x64  IRQ_ACK     只写   → 写入 IRQ_STATUS 的值，清除对应中断
```

**为什么 LIVENESS 用 0xa5a5aa5a**

```
设备会对写入值做按位取反（bitwise NOT）：

  写入:  0xa5a5aa5a = 10100101 10100101 10101010 01011010
  读出:  0x5a5a55a5 = 01011010 01011010 01010101 10100101 (正好是取反)

如果读出来的不是这个值，说明 MMIO 映射有问题。
这是验证 BAR0 映射是否成功的最直接方式。
```

**EDU 是"假硬件"**

```
EDU 设备完全由 QEMU 模拟：
  → 运行在虚拟机里，不是真实硬件
  → 寄存器是 QEMU 进程内存，不是真实 PCI 设备
  → 可以随便读写，不会损坏任何东西
  → 非常适合学习实验
```

---

## 二、MSI 中断原理

### 1. 从 INTx 到 MSI 的演进

```
传统 PCI 中断（INTx）：
  → 设备用一根专门的中断线（INTx#）连到 CPU
  → 电平触发，共享同一根线的设备互相影响
  → 问题：需要路由，复杂；多设备中断共享

MSI（Message Signaled Interrupt）：
  → 设备通过总线发一个"内存写事务"（带特殊地址和数据）
  → 这个写事务被 CPU 识别为中断信号
  → 不需要专门的中断线！
  → 优点：更快、更灵活、可支持多个中断向量
```

### 2. MSI 的工作流程

```
设备侧（EDU）                         CPU 侧
    │                                    │
    │  ① pci_alloc_irq_vectors           │
    │ ─────────────────────────────────►│  分配 MSI 向量
    │                                    │
    │  ② request_irq(handler)            │
    │ ─────────────────────────────────►│  注册中断处理函数
    │                                    │
    │  ③ 往 EDU_REG_IRQ_RAISE 写值       │  ← 用户态 trigger
    │ ─────────────────────────────────►│
    │    [MSI 写事务：地址=MSI向量, 数据=值] │
    │                                    │
    │                              CPU 识别中断 ──► 调用 handler
    │                                    │
    │  ④ handler 读 IRQ_STATUS           │
    │ ◄─────────────────────────────────│
    │  ⑤ 写 IRQ_ACK                     │
    │ ◄─────────────────────────────────│  ← 中断处理完成
```

### 3. 为什么 MSI 需要"分配向量"？

```
PCIe 中 MSI 的本质：
  → 每个 MSI 向量对应一个"地址+数据"
  → 设备写不同的数据（data），CPU 知道是不同中断源

pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI)：
  → 参数 1：最少分配 1 个向量
  → 参数 2：最多分配 1 个向量
  → 参数 3：只要 MSI，不要 INTx 兜底
  → 返回值 < 0：失败；> 0：实际分配的向量数

pci_irq_vector(pdev, 0)：
  → 获取第 0 个向量对应的 IRQ 号
  → 这个号是 Linux 内核分配的全局 IRQ 号
  → 之后 request_irq(irq_vector, ...) 用这个号
```

---

## 三、EDU 寄存器详解

### 1. EDU 寄存器布局（day25 涉及的部分）

```
BAR0 内偏移（都是 32-bit MMIO 寄存器）：
  0x00  ID          ──► 只读，EDU 设备 ID
  0x04  LIVENESS    ──► 读写，0xa5a55a5a 验证 MMIO 可访问
  0x20  STATUS      ──► 只读，EDU 当前状态（文档其他部分）
  0x24  IRQ_STATUS  ──► 只读，当前有那些中断源在等待
  0x60  IRQ_RAISE   ──► 只写，向此寄存器写值会触发 MSI
  0x64  IRQ_ACK     ──► 只写，向此寄存器写值会清除中断
```

### 2. 为什么需要 LIVENESS 检查？

```c
live = 0xa5a5aa5a;
day25_write32(d, DAY25_EDU_REG_LIVENESS, live);  // 写入测试值
d->liveness_inverted = day25_read32(d, DAY25_EDU_REG_LIVENESS);

// 如果 MMIO 映射正确（bar0 虚拟地址正确）：
//   读出的值应该是 0xa5a5aa5a

// 为什么是 0xa5a5aa5a？
//   设备会对写入的值做 bitwise NOT（按位取反）
//   0xa5a5aa5a = 10100101 10100101 10101010 01011010
//   NOT 后 = 01011010 01011010 01010101 10100101 = 0x5a5a55a5
// 如果读回来不是这个值，说明 MMIO 映射有问题
```

**LIVENESS 是 day25 验证 BAR0 iomap 成功的方式**，等价于 day23 的"读 BAR0 第一个 dword"。

### 3. 中断触发与应答流程

```
用户态 trigger                EDU 硬件                  Linux 内核
────────────────────────────────────────────────────────────────
ioctl(TRIGGER_IRQ, value=1)
  → writel(1, BAR0+0x60)     IRQ_RAISE ← 1
    ──────────────────────────► MSI 写事务发到总线
                                CPU 收到 MSI 中断
                                  ──► 调用 day25_irq_handler()
                                       status = readl(BAR0+0x24)  // IRQ_STATUS
                                       if status == 0 → IRQ_NONE
                                       irq_count++ (在 spinlock 内)
                                       writel(status, BAR0+0x64)  // IRQ_ACK
中断处理完成

用户态 ioctl(GET_IRQ_STATUS)
  → 返回 last_irq_status 和 last_ack_value
```

---

## 四、为什么 day25 用 chrdev 而不是 miscdevice

### 1. chrdev 的完整注册流程

day25 采用了完整的字符设备框架（不是 miscdevice）：

```
步骤 1：module_init() 中预分配字符设备号范围
  alloc_chrdev_region(&g_day25_base_dev, 0, DAY25_MAX_MINORS, DRV_NAME)
  → 分配主设备号（内核自动分配）
  → 次设备号范围：0 ~ 31（共 32 个）

步骤 2：module_init() 中创建 class
  class_create(THIS_MODULE, "day25_edu")
  → 在 /sys/class/day25_edu/ 创建目录

步骤 3：probe() 中为每个设备创建 cdev + device
  cdev_init(&d->cdev, &fops)     → 绑定文件操作
  cdev_add(&d->cdev, devt, 1)   → 向内核添加 cdev
  device_create(..., minor)      → 创建 /dev/day25_edu0
```

**为什么需要预分配？**

```
class_create 和 alloc_chrdev_region 必须在 module_init 中做，
不能等到 probe，因为：
  → module_init 在所有 PCI probe 之前执行
  → 这样每个 probe 都能直接用已经分配好的设备号
  → 避免了"probe 到来时设备号不够"的问题
```

### 2. miscdevice vs chrdev 对比（与 day24 的关键差异）

```
为什么 day24 用 miscdevice，day25 用 chrdev？

day24（miscdevice）优点：
  → 一行 misc_register() 自动搞定一切
  → 不需要 module_init/class_create
  → 代码简洁，适合单设备

day25（chrdev）优点：
  → 支持多设备（minor 0~31）
  → 与真实生产驱动完全一致的结构
  → class_create 让 /sys/class/ 有清晰对应关系
  → 为 day26 及以后扩展多设备打下基础
```

### 3. container_of 在 day25 的特殊用法

```c
// day24 的 container_of：
struct miscdevice *misc = file->private_data;
struct day25_dev *d = container_of(misc, struct day25_dev, miscdev);

// day25 的 container_of（用的是 i_cdev）：
struct day25_dev *d = container_of(inode->i_cdev, struct day25_dev, cdev);
```

为什么不同？因为 chrdev 的 inode 里嵌的是 `struct cdev *i_cdev`，miscdevice 里嵌的是 `struct miscdevice *i_cdev`（实际是 `struct device`）。**两个 container_of 都是在已知结构体成员的前提下，反推结构体基地址**，本质相同，只是成员不同。

---

## 五、自旋锁与中断上下文

### 1. 为什么中断 handler 需要自旋锁？

```c
static irqreturn_t day25_irq_handler(int irq, void *opaque)
{
    struct day25_dev *d = opaque;
    unsigned long flags;

    spin_lock_irqsave(&d->irq_lock, flags);  // 关中断 + 加锁
    d->irq_count++;
    d->last_irq_status = status;
    d->last_ack_value = status;
    spin_unlock_irqrestore(&d->irq_lock, flags);  // 恢复中断 + 解锁

    return IRQ_HANDLED;
}
```

**为什么普通 mutex 不行？**

```
中断处理函数运行在"中断上下文"（interrupt context）
  → 不是进程上下文！
  → 不能调用可能睡眠的操作（如 mutex_lock）

自旋锁（spin_lock）的特性：
  → 如果锁被占用，CPU 原地忙等待（spin）
  → 不睡眠，不调用调度器
  → 适合中断上下文

spin_lock_irqsave(&lock, flags)：
  → 保存当前中断状态（flags）并关中断
  → 防止同一中断在处理过程中被"嵌套"打断
  → 返回后用 spin_unlock_irqrestore(&lock, flags) 恢复
```

### 2. 用户态 ioctl 访问同样的变量需要锁吗？

```c
// ioctl(GET_IRQ_COUNT) 运行在进程上下文，可以用 mutex
// 但 day25 的 irq_count 在 handler 中更新，在 ioctl 中读取
// 如果不用锁，ioctl 可能读到"正在写入中"的不完整值

// 注意：day25 实际上在 ioctl(GET_IRQ_COUNT) 中没有加锁
// 这是一个已知简化：读取 u64 在多数架构上是原子的
// 但 irq_status 和 irq_count 同时被更新时，组合读可能不一致
// 生产代码应该在 ioctl 中也加锁或用 seqlock
```

---

## 六、probe 完整流程解析

```c
static int day25_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    // ========== 第1步：分配私有数据 ==========
    d = kzalloc(sizeof(*d), GFP_KERNEL);
    spin_lock_init(&d->irq_lock);       // ← day25 新增：初始化自旋锁
    pci_set_drvdata(pdev, d);

    // ========== 第2步：使能 PCI 设备 ==========
    ret = pci_enable_device(pdev);

    // ========== 第3步：申请 BAR 资源 ==========
    ret = pci_request_regions(pdev, DAY25_DRV_NAME);
    pci_set_master(pdev);

    // ========== 第4步：映射 BAR0 ==========
    d->bar0 = pci_iomap(pdev, DAY25_BAR0, 0);

    // ========== 第5步：分配 MSI 向量 ==========
    // ← day25 核心：分配 MSI 中断向量
    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
    // 参数：最少1个，最多1个，只要MSI（不要INTx兜底）
    // 失败就直接报错，不降级到 INTx

    // ========== 第6步：注册中断处理函数 ==========
    d->irq_vector = pci_irq_vector(pdev, 0);
    ret = request_irq(d->irq_vector,        // 中断号
                       day25_irq_handler,   // 处理函数
                       0,                   // flags（0 = 边沿触发）
                       DAY25_DRV_NAME,      // 中断名字（出现在 /proc/interrupts）
                       d);                  // 传给 handler 的参数
    // 成功返回 0，失败返回负值

    // ========== 第7步：验证 MMIO 可访问 ==========
    ident = day25_read32(d, DAY25_EDU_REG_ID);
    day25_write32(d, DAY25_EDU_REG_LIVENESS, 0xa5a5aa5a);
    d->liveness_inverted = day25_read32(d, DAY25_EDU_REG_LIVENESS);
    // 如果 MMIO 正确，liveness_inverted 应该是 ~0xa5a5aa5a

    // ========== 第8步：建立字符设备节点 ==========
    day25_setup_chrdev(d);  // cdev_add + device_create
    return 0;

    // 错误处理（逆序）：
    // err_irq:       free_irq(d->irq_vector, d)
    // err_iounmap:   pci_iounmap(pdev, d->bar0)
    // err_regions:   pci_release_regions(pdev)
    // err_disable:   pci_disable_device(pdev)
    // err_free:      kfree(d)
}
```

---

## 六-补充：day25 完整调用链

### 1. 模块加载阶段（module_init）

```
insmod demo.ko
  → 调用 day25_init()
       ① alloc_chrdev_region(&g_day25_base_dev, 0, 32, "day25_edu")
          → 分配主设备号（内核动态分配）
          → 次设备号范围 0~31（共 32 个）
          → g_day25_base_dev.major 就是主设备号

       ② class_create(THIS_MODULE, "day25_edu")
          → 在 /sys/class/day25_edu/ 创建目录
          → 为后续 device_create 准备

       ③ pci_register_driver(&day25_pci_driver)
          → 注册 PCI 驱动到内核
          → 注意：这时候 probe 不会立即调用
          → 因为 QEMU 还没启动，PCI 设备还没枚举到

⚠️ module_init 执行完后，chrdev 号和 class 已经就绪
   但设备节点 /dev/day25_edu0 还不存在（要等 probe）
```

### 2. PCI 设备探测阶段（probe）

```
[QEMU 启动，PCI 总线枚举]
  → 发现 Vendor=1234, Device=11e8 的 EDU 设备
  → 内核查找匹配的 pci_device_id
  → 调用 day25_probe(pdev, id)

       ① kzalloc(sizeof(*d))          分配私有数据结构
       ② spin_lock_init(&d->irq_lock)  初始化自旋锁
       ③ pci_set_drvdata(pdev, d)     绑定双向指针

       ④ pci_enable_device(pdev)       使能 PCI 设备

       ⑤ pci_request_regions(pdev, "day25_edu")
          → 声明 BAR0 使用权

       ⑥ pci_set_master(pdev)
          → 使能总线主模式（设备可以发 DMA）

       ⑦ d->bar0 = pci_iomap(pdev, 0, 0)
          → 映射 BAR0 到虚拟地址

       ⑧ pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI)
          → 分配 1 个 MSI 中断向量
          → 失败则报错（不允许降级到 INTx）

       ⑨ d->irq_vector = pci_irq_vector(pdev, 0)
          → 获取分配的 IRQ 号

       ⑩ request_irq(d->irq_vector, day25_irq_handler, 0, "day25_edu", d)
          → 注册中断处理函数
          → 以后往 EDU 写 IRQ_RAISE，这个 handler 就会被调用

       ⑪ EDU 寄存器验证
           day25_read32(ID)           → 验证返回 0xEduardo
           day25_write32(LIVENESS, 0xa5a5aa5a)
           day25_read32(LIVENESS)    → 应该返回 0x5a5a55a5

       ⑫ day25_setup_chrdev(d)
           cdev_init(&d->cdev, &day25_fops)
           cdev_add(&d->cdev, devt, 1)
           device_create(g_day25_class, ..., "day25_edu%d", minor)
              → /dev/day25_edu0 终于创建！

probe 完成后：chrdev 就绪 + MSI handler 注册 + BAR0 映射
```

### 3. 用户态打开设备（open）

```
用户态程序：
  fd = open("/dev/day25_edu0", O_RDWR);

调用链：
  → VFS 根据主设备号找到 chrdev 表
  → 根据次设备号找到 cdev
  → 调用 day25_fops.open
      → inode->i_cdev 指向 d->cdev
      → container_of(inode->i_cdev, ...) 反推 day25_dev*
      → file->private_data = d
      → 返回 0

成功打开后：file->private_data = d，后续 read/write/ioctl 都用这个 d
```

### 4. 用户态触发中断（ioctl TRIGGER_IRQ）

```
用户态：
  ioctl(fd, DAY25_IOC_TRIGGER_IRQ, 1);

内核调用链：
  → VFS 找到 day25_fops.unlocked_ioctl
  → 调用 day25_ioctl(TRIGGER_IRQ, arg)

  switch (TRIGGER_IRQ):
      → 获取用户传来的 value
      → writel(value, d->bar0 + DAY25_EDU_REG_IRQ_RAISE)
         ────────────────────────────────►
                                        EDU 硬件收到写
                                           ↓
                                    MSI 写事务发到 PCI 总线
                                           ↓
                                    CPU 收到 MSI 中断
                                           ↓
                                    Linux 内核调度到 handler
                                           ↓
                              day25_irq_handler(irq_vector, d) 被调用
                                           ↓
                                    CPU 执行完 handler 返回

  → 返回 0（handler 执行完之前不会等，是"触发后立即返回"）
```

### 5. 中断处理函数（irq_handler）

```
day25_irq_handler(int irq, void *opaque)
{
    struct day25_dev *d = opaque;
    u32 status;

    ① 读取 IRQ_STATUS（哪些中断源在等待）
    status = readl(d->bar0 + DAY25_EDU_REG_IRQ_STATUS);

    ② 无有效中断则返回 IRQ_NONE
    if (!status)
        return IRQ_NONE;

    ③ 自旋锁保护（关中断 + 加锁）
    spin_lock_irqsave(&d->irq_lock, flags);

    ④ 更新驱动状态
    d->irq_count++;         ← 统计中断次数
    d->last_irq_status = status;
    d->last_ack_value = status;

    ⑤ 写 IRQ_ACK 清除中断
    writel(status, d->bar0 + DAY25_EDU_REG_IRQ_ACK);

    ⑥ 打印日志
    dev_info(&d->pdev->dev, "irq handler: irq=%d status=0x%x count=%lu\n",
             irq, status, d->irq_count);

    ⑦ 解锁 + 返回
    spin_unlock_irqrestore(&d->irq_lock, flags);
    return IRQ_HANDLED;  ← 告诉内核中断已处理
}
```

### 6. 用户态查询中断结果（ioctl GET）

```
用户态：
  ioctl(fd, DAY25_IOC_GET_IRQ_COUNT);   → irq_count 几了
  ioctl(fd, DAY25_IOC_GET_IRQ_STATUS);  → last_irq_status 和 last_ack_value

内核调用：
  → VFS → day25_ioctl(GET_IRQ_COUNT/GET_IRQ_STATUS)
  → spin_lock_irqsave(&d->irq_lock, flags);  ← 读共享变量要加锁
  → copy_to_user(...)
  → spin_unlock_irqrestore(&d->irq_lock, flags);
```

### 7. 完整数据流总图

```
阶段1: insmod
  day25_init()
    alloc_chrdev_region()
    class_create()
    pci_register_driver()
  ⚠️ probe 这时候不会调用（设备还没枚举到）

阶段2: QEMU启动 + PCI枚举
  QEMU 创建 EDU (1234:11e8)
    → PCI 总线枚举
    → 匹配到驱动
    → day25_probe() 执行
       → BAR0 iomap
       → pci_alloc_irq_vectors()
       → request_irq()
       → cdev_add() + device_create()
       → /dev/day25_edu0 创建！

阶段3: 用户态open
  open("/dev/day25_edu0")
    → file->private_data = d

阶段4: 用户态trigger
  ioctl(TRIGGER_IRQ, 1)
    → writel(1, BAR0+0x60)
    → EDU 收到 → 发 MSI
    → CPU 收到 → 调用 day25_irq_handler()
    → handler: 读status/更新count/写ACK/打印日志
    → 返回 IRQ_HANDLED

阶段5: 用户态查结果
  ioctl(GET_IRQ_COUNT)
    → return d->irq_count
  ioctl(GET_IRQ_STATUS)
    → return last_irq_status + last_ack_value
```

---

## 七、三条证据链验证中断成功

```
证据链 1：驱动内部 irq_count
  → trigger 前：irq_count = 0
  → trigger 后：irq_count = 1
  → 证明：handler 确实被调用了

证据链 2：/proc/interrupts 全局统计
  → trigger 前：day25_edu_irq 0 次
  → trigger 后：day25_edu_irq 1 次
  → 证明：内核全局中断计数器也认可这次中断

证据链 3：dmesg 日志
  → "irq handler: irq=XX status=0xXX count=1"
  → 证明：handler 内部逻辑执行了（读 IRQ_STATUS + 写 IRQ_ACK）

为什么需要三条证据链？
  → irq_count 是驱动自己维护的（可能出错）
  → /proc/interrupts 是内核全局统计（更可信）
  → dmesg 是最细节的日志（能看到 status 值）
  → 三条独立证据同时成立，才算真正确认
```

---

## 八、module_init / module_exit vs module_pci_driver

### 1. 为什么 day25 需要自定义 init/exit？

```
day25 的 init 需要做三件事：
  ① alloc_chrdev_region    ──► 预分配字符设备号
  ② class_create           ──► 创建 sysfs class
  ③ pci_register_driver    ──► 注册 PCI 驱动

day24 用 module_pci_driver 简化写法，
但 module_pci_driver 只做了 pci_register_driver/pci_unregister_driver，
不能同时做 chrdev + class 的初始化。

因此 day25 必须用 module_init / module_exit 手动写清所有初始化。
```

### 2. module_init 执行顺序

```
加载模块时：
  1. 内核分配模块内存
  2. 解析符号依赖
  3. 调用 module_init(day25_init)
       → alloc_chrdev_region          ✓
       → class_create                 ✓
       → pci_register_driver          ✓
         → probe 不会在这里立即调用
           （PCI 设备可能还没枚举到）

4. 等待 PCI 设备被热插拔或 QEMU 启动时枚举
   → probe 被调用

卸载模块时：
  1. 调用 module_exit(day25_exit)
       → pci_unregister_driver        ✓（会自动调用所有 remove）
       → class_destroy                ✓
       → unregister_chrdev_region    ✓
```

---

## 九、day25 在 W4 中的位置

```
W4 (PCIe 基础)
├── day22: 设备枚举可见性
│           理解 PCI enum / lspci / stub 骨架
├── day23: BAR/MMIO 资源接管
│           pci_enable / request_regions / iomap
├── day24: ivshmem BAR2 共享内存协议
│           miscdevice + 协议头 + read/write
├── day25: MSI 中断 ← 今天
│           pci_alloc_irq_vectors / request_irq / chrdev
├── day26-27: 深入学习
└── day28: W4 收口

W5 (DMA 性能)
├── day29: DMA coherent buffer
├── day30: mmap 零拷贝
└── ...
```

---

## 十、一句话总结 day25

> **day25 的目标是在 PCI 驱动基础上加入 MSI 中断：分配 MSI 向量、注册 handler、用户态通过写 EDU_IRQ_RAISE 触发中断、handler 中读取 IRQ_STATUS 并完成 ACK、通过 irq_count + /proc/interrupts + dmesg 三条证据链验证中断闭环。做完这一天，你理解了"设备怎么主动发信号给 CPU"这个最核心的驱动能力。**
