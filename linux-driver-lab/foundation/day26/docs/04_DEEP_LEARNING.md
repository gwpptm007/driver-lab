# Day26 深度学习指南

## 一、Day26 是什么？

Day26 在 Day25 的 EDU + MSI 中断实验基础上，**将字符设备接口打磨成用户态友好工具**。

核心目标不是改变底层硬件交互（EDU 设备、MSI 中断路径完全沿用），而是让用户态接口更清晰、更实用：

| 特性 | Day25 | Day26 |
|------|-------|-------|
| read() | 未实现 | 返回可读文本状态快照 |
| write() | 未实现 | 写入整数触发中断（非0值） |
| ioctl() | 4个命令 | 4个命令（相同） |
| 错误码 | 较简单 | 清晰分类（EINVAL/E2BIG/ENODEV/EFAULT） |
| 用户态工具 | 无 | 完整CLI工具 |

---

## 二、EDU 设备（与 Day25 相同）

EDU 是 QEMU 模拟的教学设备，用于学习 PCI + MSI 中断。

### 2.1 PCI ID
- Vendor ID: `0x1234`（QEMU 虚拟厂商）
- Device ID: `0x11e8`（EDU 教学设备）

### 2.2 BAR0 寄存器布局

```
偏移      名称           读写属性    说明
─────────────────────────────────────────────────────
0x00     ID             只读       EDU 设备标识
0x04     LIVENESS       读写       写入 0xa5a55a5a，读回值为按位取反
0x20     STATUS         只读       设备状态
0x24     IRQ_STATUS     只读       当前中断状态（位掩码）
0x60     IRQ_RAISE      只写       写入任意值触发 MSI 中断
0x64     IRQ_ACK        只写       写入 IRQ_STATUS 值清除中断
```

### 2.3 LIVENESS 机制

向 `LIVENESS(0x04)` 写入 `0xa5a55a5a`，读回值为按位取反 `0x5a5aa5a5`。用于验证 MMIO 映射正确。

### 2.4 MSI 中断流程

```
用户态 write(触发值)
    ↓
驱动 write() 收到值
    ↓
writel(触发值, BAR0 + IRQ_RAISE)
    ↓
EDU 硬件向 MSI 地址写入，触发 CPU 中断
    ↓
day26_irq_handler() 执行
    ↓
读 IRQ_STATUS → 更新计数 → 写 IRQ_ACK 清除
```

---

## 三、day26_dev 结构体

```c
struct day26_dev {
    struct pci_dev *pdev;        /* PCI 设备指针 */
    void __iomem *bar0;          /* BAR0 MMIO 映射虚拟地址 */
    resource_size_t bar0_start;   /* BAR0 起始物理地址 */
    resource_size_t bar0_len;     /* BAR0 长度 */
    int irq_vector;              /* MSI 中断向量号 */
    u64 irq_count;               /* 中断处理计数 */
    u32 last_irq_status;         /* 最近中断状态 */
    u32 last_ack_value;          /* 最近 ACK 写入值 */
    u32 identity_value;          /* ID 寄存器值（固定标识） */
    u32 liveness_value;          /* LIVENESS 测试值 */
    u32 liveness_inverted;       /* LIVENESS 取反值 */
    spinlock_t irq_lock;         /* 保护共享数据 */
    dev_t devt;                  /* 设备号 */
    struct cdev cdev;            /* 字符设备结构 */
    struct device *device;       /* sysfs 设备节点 */
};
```

与 Day25 几乎相同，多了 `identity_value` 字段用于存储只读 ID 寄存器值。

---

## 四、用户态工具设计（CLI 工具）

### 4.1 工具命令

```bash
day26_edu_tool <dev> info          # ioctl 获取完整结构化信息
day26_edu_tool <dev> read-state    # read() 获取文本状态快照
day26_edu_tool <dev> trigger <val> # write() 触发中断（val 非0）
day26_edu_tool <dev> count          # ioctl 获取中断计数
day26_edu_tool <dev> status         # ioctl 获取最近中断状态
day26_edu_tool <dev> reset-stats    # ioctl 清零统计
```

### 4.2 返回码设计

```c
enum {
    DAY26_RC_OK = 0,      // 成功
    DAY26_RC_OPEN = 1,    // 打开设备失败
    DAY26_RC_USAGE = 2,   // 命令行用法错误
    DAY26_RC_IOCTL = 3,   // ioctl 系统调用失败
    DAY26_RC_READ = 4,    // read 系统调用失败
    DAY26_RC_WRITE = 5,   // write 系统调用失败
};
```

### 4.3 负向测试：trigger 0

Day26 故意要求触发值必须非零：
- `trigger 0` → 驱动返回 `-EINVAL` → 工具返回 `DAY26_RC_WRITE=5`
- 验证错误处理路径正确

---

## 五、ioctl 命令详解

### 5.1 数据结构

```c
/* 设备完整信息 */
struct day26_info {
    __u32 tool_api_version;   /* API 版本 */
    __u16 vendor_id;          /* PCI Vendor ID */
    __u16 device_id;         /* PCI Device ID */
    __u64 bar0_start;        /* BAR0 物理地址 */
    __u64 bar0_len;          /* BAR0 长度 */
    __u32 irq_vector;        /* MSI 向量号 */
    __u64 irq_count;         /* 中断计数 */
    __u32 last_irq_status;   /* 最近中断状态 */
    __u32 last_ack_value;    /* 最近 ACK 值 */
    __u32 identity_value;    /* ID 寄存器值 */
    __u32 liveness_value;    /* LIVENESS 测试值 */
    __u32 liveness_inverted; /* LIVENESS 取反值 */
    __u32 msi_enabled;       /* MSI 是否启用 */
};

/* 中断计数 */
struct day26_irq_count {
    __u64 count;
};

/* 中断状态 */
struct day26_irq_status {
    __u32 irq_status;
    __u32 ack_value;
};
```

### 5.2 ioctl 编号

```c
#define DAY26_IOC_GET_INFO       _IOR('r', 0x01, struct day26_info)
#define DAY26_IOC_GET_IRQ_COUNT  _IOR('r', 0x02, struct day26_irq_count)
#define DAY26_IOC_GET_IRQ_STATUS _IOR('r', 0x03, struct day26_irq_status)
#define DAY26_IOC_RESET_STATS    _IO('r',  0x04)
```

---

## 六、完整调用链

### 6.1 模块加载（insmod）

```
用户执行 insmod day26_edu_tool.ko
    ↓
day26_init()                    // module_init()
    ↓
alloc_chrdev_region()           // 分配主设备号（major）和次设备号范围（minor 0~31）
    ↓
class_create()                  // 创建 /sys/class/day26_edu/
    ↓
pci_register_driver()           // 注册 PCI 驱动
    ↓
pci_bus_driver.probe()          // PCI 总线枚举匹配到 1234:11e8
    ↓
day26_probe()                   // 探针函数（核心初始化）
    ↓
    ├→ kzalloc(sizeof(day26_dev))          // 分配私有数据结构
    ├→ spin_lock_init()                    // 初始化自旋锁
    ├→ pci_enable_device()                  // 启用 PCI 设备
    ├→ pci_request_regions()               // 请求 BAR0~BAR5 I/O 端口
    ├→ pci_set_master()                    // 设置为主设备（启用总线访问）
    ├→ pci_iomap(BAR0)                    // 将 BAR0 映射到内核虚拟地址
    ├→ pci_alloc_irq_vectors(1, 1, MSI)    // 分配 1 个 MSI 中断向量
    ├→ request_irq()                       // 注册中断处理函数
    ├→ readl(BAR0+ID)                      // 读取 ID 寄存器
    ├→ readl(BAR0+LIVENESS)                // 读取 LIVENESS 寄存器
    ├→ day26_setup_chrdev()               // 创建设备节点
    │     ├→ atomic_fetch_add()            // 分配 minor 号
    │     ├→ MKDEV(major, minor)           // 构建设备号
    │     ├→ cdev_init()                   // 初始化 cdev
    │     ├→ cdev_add()                    // 添加到 VFS
    │     └→ device_create()               // 创建 /dev/day26_edu0
    ↓
pr_info("module init")          // 打印初始化完成
```

### 6.2 用户打开设备

```
用户执行 day26_edu_tool /dev/day26_edu0 info
    ↓
open("/dev/day26_edu0", O_RDWR)
    ↓
VFS 根据设备号找到 cdev
    ↓
day26_open(inode, file)         // file_operations.open
    ↓
container_of(inode->i_cdev, struct day26_dev, cdev)  // 从 cdev 反推 day26_dev
    ↓
file->private_data = d          // 将驱动数据结构存入 file
    ↓
返回 fd 给用户态
```

### 6.3 trigger 命令（触发中断）

```
用户执行 day26_edu_tool /dev/day26_edu0 trigger 1
    ↓
write(fd, "1", 2)               // 用户态 write()
    ↓
VFS 找到 cdev
    ↓
day26_write(file, buf, count, ppos)  // 驱动 write()
    ↓
    ├→ copy_from_user(kbuf, buf, count)     // 拷贝用户数据到内核
    ├→ simple_strtoul(kbuf, &end, 0)        // 解析整数（支持十进制/十六进制）
    ├→ 验证 v != 0 && v <= 0xffffffff       // 必须非零！
    └→ writel(v, BAR0 + IRQ_RAISE)          // 写入触发寄存器
    ↓
EDU 硬件收到触发值，发送 MSI 中断
    ↓
CPU 接收中断，调用 day26_irq_handler()
    ↓
    ├→ readl(BAR0 + IRQ_STATUS)             // 读取中断状态
    ├→ spin_lock_irqsave()                   // 加锁（中断上下文）
    ├→ irq_count++                           // 计数 +1
    ├→ last_irq_status = status              // 保存状态
    ├→ last_ack_value = status               // 保存 ACK 值
    ├→ spin_unlock_irqrestore()              // 解锁
    ├→ writel(status, BAR0 + IRQ_ACK)       // 清除中断
    └→ dev_info()                            // 打印日志
    ↓
write() 返回 count（成功）
    ↓
用户态打印 "triggered value=0x00000001"
```

### 6.4 count 命令（查询中断计数）

```
用户执行 day26_edu_tool /dev/day26_edu0 count
    ↓
ioctl(fd, DAY26_IOC_GET_IRQ_COUNT, &cnt)
    ↓
VFS 找到 cdev
    ↓
day26_ioctl(file, cmd, arg)
    ↓
switch (DAY26_IOC_GET_IRQ_COUNT)
    ├→ irq_count = d->irq_count              // 读取驱动计数
    ├→ copy_to_user(&cnt, &cnt, sizeof(cnt)) // 拷贝到用户态
    └→ return 0
    ↓
用户态打印 "irq_count=1"
```

### 6.5 read-state 命令（读取文本状态）

```
用户执行 day26_edu_tool /dev/day26_edu0 read-state
    ↓
read(fd, buf, sizeof(buf))
    ↓
VFS 找到 cdev
    ↓
day26_read(file, buf, count, ppos)
    ↓
    ├→ day26_build_state_text(d, kbuf, size)  // 构建文本格式状态
    │     └→ scnprintf() 生成多行文本：
    │          vendor=0x1234 device=0x11e8
    │          bar0_start=0x... bar0_len=0x...
    │          irq_vector=50 irq_count=1 msi_enabled=1
    │          identity_value=0x...
    │          liveness_value=0xa5a55a5a liveness_inverted=0x5a5aa5a5
    │          last_irq_status=0x... last_ack_value=0x...
    ├→ simple_read_from_buffer()             // 复制到用户态
    └→ 返回实际写入的字节数
    ↓
用户态打印完整文本状态
```

### 6.6 完整数据流图

```
┌─────────────────────────────────────────────────────────────────────┐
│                         用户态进程                                    │
│                                                                     │
│  open("/dev/day26_edu0")                                           │
│       ↓                                                             │
│  write(fd, "1", 2) ──────→ ioctl(fd, GET_INFO, &info)             │
│       │                              │                              │
│       │                              ↓                              │
│       │                     copy_to_user()                         │
│       │                              │                              │
└───────┼──────────────────────────────┼──────────────────────────────┘
        │                              │
        │   [字符设备接口]              │  [字符设备接口]
        ↓                              ↓
┌───────────────────────────────────────────────────────────────────┐
│                         Linux 内核                                  │
│  day26_write()              day26_ioctl()                         │
│       │                              │                              │
│       ↓                              ↓                              │
│  writel(v, BAR0+IRQ_RAISE)   读取 d->irq_count 等字段             │
│       │                              │                              │
└───────┼──────────────────────────────┼──────────────────────────────┘
        │                              │
        ↓                              ↓
┌───────────────────────────────────────────────────────────────────┐
│                      EDU 硬件 (QEMU 模拟)                          │
│                                                                     │
│  MSI 中断 ──────────────────────────────→                           │
│       │                                                              │
│       ↓                                                              │
│  day26_irq_handler()  ←────────────────┘                            │
│       │                                                              │
│       ├→ readl(BAR0+IRQ_STATUS)                                     │
│       ├→ irq_count++                                                │
│       └→ writel(status, BAR0+IRQ_ACK)                               │
│                                                                     │
└───────────────────────────────────────────────────────────────────┘
```

---

## 七、错误码处理

### 7.1 驱动层错误码

| 错误码 | 含义 | 触发条件 |
|--------|------|----------|
| `-EINVAL` | 无效参数 | count==0、v==0、解析失败 |
| `-E2BIG` | 输入过长 | count >= sizeof(kbuf) |
| `-ENODEV` | 设备不可用 | bar0 未映射或 d 为空 |
| `-EFAULT` | 用户拷贝失败 | copy_from_user/copy_to_user 失败 |

### 7.2 write() 负向路径

```c
/* 触发值必须非零！这是故意的设计 */
if (v == 0 || v > 0xffffffffUL)
    return -EINVAL;
```

- `trigger 0` → 返回 `-EINVAL` → 用户态工具返回 `DAY26_RC_WRITE=5`
- 用途：验证错误处理路径是否正确，错误信息是否清晰

---

## 八、与 Day25 的对比

| 特性 | Day25 | Day26 |
|------|-------|-------|
| 设备接口 | ioctl only | ioctl + read + write |
| read() | 未实现 | 文本状态快照 |
| write() | 未实现 | 整数触发（需非零） |
| 错误码 | 基本 | 清晰分类 |
| 用户态工具 | 无 | 完整 CLI |
| 底层硬件 | EDU + MSI | EDU + MSI（相同） |
| 驱动结构 | day25_dev | day26_dev（基本相同） |

---

## 九、关键设计思想

### 9.1 用户态友好性

- **read() 返回文本**：用户可以直接 `cat /dev/day26_edu0` 查看状态，无需编写程序
- **write() 接受自然输入**：`"1"`、`"0x1"`、`"0x5\n"` 都能正确解析
- **ioctl 返回结构化数据**：便于程序解析，便于自动化测试

### 9.2 错误码清晰

每种错误都有明确的返回码和错误信息，自动化测试（guest/init.day26）可以验证：
- 正向路径正常工作
- 负向路径正确报错

### 9.3 沿用成熟架构

Day26 没有引入新的硬件交互模式，完全沿用 Day25 的：
- PCI 枚举流程
- MSI 中断机制
- BAR0 MMIO 映射
- 自旋锁保护

这体现了"渐进式增量开发"的思想：先打通基础功能，再打磨用户体验。
