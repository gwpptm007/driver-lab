# Day24 深度学习指南

## 一、day24 到底要学什么

### 1. 核心问题：PCI BAR 映射的内存能做什么？

day23 完成了 PCI 资源接管，得到了 BAR0（寄存器窗口）和 BAR2（共享内存）的虚拟地址。day24 要回答：**拿到这些映射地址后，怎么设计一个双方（驱动与用户态）都能理解的通信协议？**

### 2. day22/day23/day24 的演进逻辑

| | day22 | day23 | day24 |
|---|---|---|---|
| 做了什么 | 看见设备 | 接管资源 | **在 BAR2 上建立通信协议** |
| pci_enable_device | 没做 | 做了 | 做了 |
| pci_request_regions | 没做 | 做了 | 做了 |
| pci_iomap | 没做 | 做了 | 做了 |
| BAR2 MMIO 读写 | 没做 | 读了一个 dword | **实现了完整协议头** |
| misc device | 没做 | 没做 | **做了** |
| 用户态交互 | 没有 | 没有 | **有了（ioctl + read/write）** |

### 3. 为什么 BAR0 不动，只动 BAR2

```
ivshmem (1af4:1110) 的 BAR：
  BAR0: 256 字节 = 0x100   → QEMU 硬件级寄存器窗口
                               （Interrupt State / MSI capability 等）
                               ⚠️ 乱写会导致硬件行为异常
  BAR2: 4MB = 0x400000    → 共享内存区域
                               ✅ 我们自己定义的协议头就放在这里
```

**day24 的核心原则：不碰 BAR0 硬件寄存器，只把 BAR2 当作一块我们可以自由定义的共享内存来使用。**

---

## 二、共享内存协议设计

### 1. 为什么需要"协议"

共享内存是两块地址空间（驱动侧 + 用户态侧）都能访问的同一块物理内存。**没有协议，两边就无法解释数据。**

```
共享内存 = 一张白纸
协议 = 在白纸上画格子、写标题
                           ↓
         驱动写"hello"     用户态读"hello"
         靠的是协议规定的   靠的是协议规定的
         写入格式           读取格式
```

### 2. 协议头布局

BAR2 起始处定义了一个 32 字节（0x00~0x1F）的协议头：

```
BAR2 布局：
+0x00  MAGIC     (4 bytes)  = 0x44593234 ("DY24")  ← 标识这是 day24 的协议
+0x04  VERSION   (4 bytes)  = 1                   ← 版本号，协议升级时递增
+0x08  SEQ        (4 bytes)  = 序列号，每次写入+1   ← 防重放/版本跟踪
+0x0c  STATE     (4 bytes)  = 状态枚举             ← EMPTY / READY / USER_WRITTEN / USER_TOUCHED
+0x10  LEN       (4 bytes)  = 当前 payload 长度     ← 有效数据长度
+0x14~+0x1F  (reserved)
+0x20  PAYLOAD   (~4MB-0x20) ← 实际数据区
```

**为什么 MAGIC 是 `0x44593234`？**

`0x44='D'`, `0x59='Y'`, `0x32='2'`, `0x34='4'` —— 小端序存储的 "DY24" 字符串，便于调试时直接在对端 hex dump 中辨认。

### 3. 状态机

```
用户态进程 A                    共享内存                    用户态进程 B
     │                             │                             │
     │  write("hello")            │                             │
     │ ──────────────────────────► │                             │
     │   STATE=USER_WRITTEN        │                             │
     │   LEN=5                     │                             │
     │   [hello...............]    │                             │
     │                             │                             │
     │                             │      read()                 │
     │                             │ ───────────────────────────►│
     │                             │        "hello"               │
     │                             │                             │
     │                             │                             │
     │  ioctl(CLEAR_PAYLOAD)       │                             │
     │ ──────────────────────────► │                             │
     │   STATE=EMPTY               │                             │
     │   LEN=0                     │                             │
     │   [.......................] │ (清零)                      │
```

---

## 二-补充：BAR2 共享内存 — 到底哪块是共享的

### 1. 为什么 BAR2 叫"共享"内存

**"共享"意味着两块不同的地址空间都能访问同一块物理内存。**

```
QEMU 进程地址空间              Linux 内核地址空间              用户态进程地址空间
       │                            │                            │
       │                     ┌──────┴──────┐                     │
       │                     │   BAR2 MMIO  │                     │
       │                     │ 同一块物理内存 │                    │
       │                     └──────┬──────┘                     │
       │                            │                            │
       ├────────────────────────────┼────────────────────────────┤
       │     驱动读写 (kernel)       │    read/write (user)       │
       │     协议头初始化            │    ioctl 控制              │
       │     memcpy_toio/fromio     │    llseek 游标            │
       └────────────────────────────┴────────────────────────────┘
```

**BAR0 不是共享内存**：BAR0 是 QEMU 模拟的硬件寄存器（256字节），驱动只能读不能写。

**BAR2 才是共享内存**：QEMU 划出 4MB 物理内存，Guest OS 和 QEMU 都能访问同一块区域。

### 2. BAR2 内部地址三层寻址

```
第一层：PCI BAR 地址（物理地址）
  BAR2 start = 0xFB000000（假设）
  BAR2 end   = 0xFB3FFFFF（4MB）

第二层：MMIO 映射（虚拟地址 = pci_iomap 返回）
  d->bar[2].vaddr = 0xFFFF88880000（内核虚拟地址）

第三层：协议偏移（相对于 BAR2 起始）
  +0x00  MAGIC     = *(vaddr + 0x00) = readl(vaddr + 0x00)
  +0x04  VERSION   = *(vaddr + 0x04)
  +0x08  SEQ       = *(vaddr + 0x08)
  +0x0c  STATE     = *(vaddr + 0x0c)
  +0x10  LEN       = *(vaddr + 0x10)
  +0x20  PAYLOAD   = *(vaddr + 0x20)  ← 用户态 read/write 的数据区
```

**为什么需要三层寻址**

```
BAR2 start (物理地址)  ──→  QEMU 知道这块内存归 ivshmem 设备
       ↓ pci_iomap
vaddr (虚拟地址)        ──→  CPU 才能访问（MMU 转换）
       ↓ + offset
实际 MMIO 地址         ──→  readl/writel 的目标
```

---

## 三、MMIO 详解

### 1. 什么是 MMIO

**MMIO = Memory-Mapped I/O**（内存映射I/O）

PCI设备的每个BAR本质上是一段物理地址空间。MMIO就是通过 `pci_iomap()` 把这段物理地址映射到内核的虚拟地址空间，之后**用读写内存的方式操作PCI设备寄存器**。

```
CPU 视角（虚拟地址）:
  ┌────────────────────────────────────────┐
  │  0xffff0000  ──→  BAR0 (256B 寄存器)   │  ← readl/writel 读写
  │  0xffff1000  ──→  BAR2 (4MB 共享内存)   │  ← memcpy_toio/memcpy_fromio 读写
  └────────────────────────────────────────┘
                    ↓ ioremap / pci_iomap
硬件视角（物理地址）:
  ┌────────────────────────────────────────┐
  │  物理地址 0xFEB10000  ──→  BAR0        │
  │  物理地址 0xFB000000  ──→  BAR2        │
  └────────────────────────────────────────┘
                    ↓ PCI 总线
  ┌────────────────────────────────────────┐
  │         QEMU ivshmem 设备             │
  └────────────────────────────────────────┘
```

**为什么叫"内存映射"**

传统I/O用专门指令（如 x86 的 `in/out`），而MMIO直接把设备寄存器映射到内存地址空间，**用普通内存访问指令就能操作设备**。

### 2. 为什么 day24 重点在 BAR2 不动 BAR0

```
ivshmem 的 BAR：
  BAR0 (256字节): QEMU 硬件级寄存器窗口
                   （Interrupt State / MSI capability 等）
                   ⚠️ 乱写会导致硬件行为异常，驱动不应该碰
  BAR2 (4MB):     共享内存区域
                   ✅ 我们自由定义的协议头就放在这里
```

**day24 的核心原则：不碰 BAR0 硬件寄存器，只把 BAR2 当作一块我们可以自由定义的共享内存来使用。**

### 3. readl/writel vs memcpy_toio/fromio

### 3.1 为什么协议头用 readl/writel，payload 用 memcpy

```
协议头字段（4 字节对齐的单个寄存器）：
  → 必须用 readl() / writel()
  → 保证原子性，避免部分写入

payload 数据（任意长度、任意内容）：
  → 必须用 memcpy_toio() / memcpy_fromio()
  → 避免一次写一个字节的低效
```

**类比**：
- 读/写协议头 = 查地图坐标（一次读一整个格子）
- 读/写 payload = 抄一篇文档（一次复制一批字节）

### 3.2 readl/writel 的屏障语义

```c
// 写操作前可能需要的屏障（arch 特定）
writel(val, bar2_vaddr + off);
mb();  // memory barrier — 确保写操作对总线可见

// 读操作同理
mb();
val = readl(bar2_vaddr + off);
```

### 3.3 `__iomem` 指针为什么不能用普通指针

```c
void __iomem *bar2_vaddr;  // __iomem 标记 = 告知编译器这是 MMIO 地址

// 错误示例：
// *(volatile u32 *)bar2_vaddr = val;  // 可能被编译器优化成非 MMIO 访问

// 正确示例：
writel(val, bar2_vaddr);   // 明确告知这是 MMIO，编译器不优化
u32 val = readl(bar2_vaddr);
```

---

## 四、misc 字符设备 — 为什么需要它

### 1. 裸 BAR 映射 vs 通过 misc device 访问

```
裸 BAR 映射的问题：
  - 需要 root 权限直接读写 /dev/mem
  - 无法做权限检查
  - 无法在驱动层缓存状态（如 seq、state）
  - 用户态不知道协议格式

通过 misc device 的好处：
  - /dev/day24_ivshmem0 自动创建
  - 驱动控制所有访问（ioctl 白名单、边界检查）
  - 可以维护协议状态（seq 自增、LEN 更新）
  - 普通用户可操作（权限可配置）
```

### 2. file_operations 分工

```
day24_fops:
  .open      → 关联 file->private_data = day24_dev
  .llseek    → 在 payload 区域内游走（防止越界）
  .read      → 从 BAR2 payload 区复制到用户态
  .write     → 从用户态复制到 BAR2 payload 区
  .ioctl     → 读写协议头字段（CLEAR_PAYLOAD）
```

### 3. ioctl 命令设计

```c
// 所有 ioctl 都经过驱动审查，不会直接操作裸 BAR

DAY24_IOC_GET_INFO      → 读取所有协议头字段 + BAR 信息（只读）
DAY24_IOC_MMIO_READ32   → 读取指定偏移的 32-bit 值（限协议头区域）
DAY24_IOC_MMIO_WRITE32  → 写入指定偏移的 32-bit 值（仅限 SEQ/STATE/LEN）
DAY24_IOC_CLEAR_PAYLOAD → 重置 payload：清零 + STATE=EMPTY + LEN=0 + SEQ++
```

### 4. register_chrdev vs miscdevice 深度对比

#### 4.1 两种注册方式的完整代码对比

#### 方式 A：register_chrdev（传统方式）

```c
// 1. 定义文件操作集合
static const struct file_operations day24_fops = {
    .owner = THIS_MODULE,
    .open = day24_open,
    .read = day24_read,
    .write = day24_write,
    .unlocked_ioctl = day24_ioctl,
    .llseek = day24_llseek,
};

// 2. 注册字符设备（需要手动分配主设备号）
#define DAY24_MAJOR 240  // 挑一个没人用的号码，太难了！
int rc = register_chrdev(DAY24_MAJOR, "day24_chrdev", &day24_fops);
if (rc < 0) {
    printk(KERN_ERR "register_chrdev failed: %d\n", rc);
    return rc;
}

// 3. 创建 class（在 /sys/class/ 下）
struct class *cls = class_create(THIS_MODULE, "day24_class");
if (IS_ERR(cls)) {
    unregister_chrdev(DAY24_MAJOR, "day24_chrdev");
    return PTR_ERR(cls);
}

// 4. 创建设备（在 /dev/ 下自动创建节点）
struct device *dev = device_create(cls, NULL,
    MKDEV(DAY24_MAJOR, 0),   // 主设备号+次设备号
    NULL,                    // 父设备
    "day24_chrdev%d", 0);    // /dev/day24_chrdev0

// remove 中需要逆序释放：
device_destroy(cls, MKDEV(DAY24_MAJOR, 0));
class_destroy(cls);
unregister_chrdev(DAY24_MAJOR, "day24_chrdev");
```

#### 方式 B：miscdevice（day24 实际用法）

```c
// 1. 定义文件操作集合（完全相同）
static const struct file_operations day24_fops = {
    .owner = THIS_MODULE,
    .open = day24_open,
    .read = day24_read,
    .write = day24_write,
    .unlocked_ioctl = day24_ioctl,
    .llseek = day24_llseek,
};

// 2. 定义 miscdevice
static struct miscdevice day24_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,  // 内核自动分配次设备号
    .name  = "day24_ivshmem0",     // → /dev/day24_ivshmem0
    .fops  = &day24_fops,
    .parent = &pdev->dev,          // 父设备（可选）
};

// 3. probe 中一行搞定
int rc = misc_register(&day24_miscdev);
if (rc) {
    dev_err(&pdev->dev, "misc_register failed: %d\n", rc);
    return rc;
}

// remove 中一行搞定：
misc_deregister(&day24_miscdev);
// 无需手动创建/销毁 device、class、chrdev
```

#### 4.2 核心差异对比

| 对比维度 | register_chrdev | miscdevice |
|---|---|---|
| **主设备号** | 需手动指定或动态分配后才知道 | 不关心，内核分配 |
| **次设备号** | 需手动管理（多个设备时复杂） | 自动分配（MISC_DYNAMIC_MINOR） |
| **设备节点** | 需手动 device_create | 自动创建 /dev/name |
| **sysfs class** | 需手动 class_create/destroy | misc子系统自动处理 |
| **代码行数** | 20+ 行 | 2-3 行 |
| **probe/remove 对称性** | 容易遗漏（class、device、chrdev 三处） | 天然对称 |
| **设备层次关系** | 需要手动指定 parent | 可通过 .parent 指定 |
| **适合场景** | 多设备、需要固定主设备号 | 单设备、简单设备 |
| **错误处理复杂度** | 高（漏一处就泄漏） | 低 |

#### 4.3 主设备号的知识门槛

```
register_chrdev 的坑：
  → 主设备号 0-255 是内核预留的，不能随便用
  → 固定主设备号需要去 linux/Documentation/admin-guide/devices.txt 查
  → 动态分配的话 register_chrdev(0, ...) 返回值就是主设备号
  → 但 sysfs/class/ 里的符号链接不会自动建好
  → 还需要手动处理 udev 规则

miscdevice 的优势：
  → 完全屏蔽了设备号管理的复杂度
  → /dev/name 由内核 misc 子系统统一管理
  → udev 自动接收来自 misc 类的 uevent，自动创建节点
```

#### 4.4 为什么 day24 选 miscdevice

```
day24 选择 miscdevice 的原因：

1. 只有一个设备（/dev/day24_ivshmem0）
   → 不需要管理多个次设备号

2. 代码简洁性
   → probe 中 misc_register() 一行
   → remove 中 misc_deregister() 一行
   → 不可能遗漏资源释放

3. 与 PCI driver 集成自然
   → .parent = &pdev->dev
   → /sys/class/misc/ 下能看到
   → /sys/bus/pci/devices/.../ 关系清晰

4. 实验性质
   → 重点在 PCI BAR 协议，不在字符设备框架
   → miscdevice 让实验代码更少、更不容易出错
```

#### 4.5 什么时候必须用 register_chrdev（或其变体）

```
必须用 register_chrdev 或 cdev 的场景：

1. 需要固定主设备号
   → 例如已知的知名设备（/dev/null 用主设备号 1）
   → 需要和特定用户态工具配合（工具硬编码了主设备号）

2. 需要同时管理多个设备节点
   → 例如一个驱动创建 4 个 char device
   → 每次 open 需要根据次设备号区分是哪个实例
   → miscdevice 只适合单设备

3. 需要自己管理设备号分配策略
   → 例如动态分配一批主设备号用于热插拔

实际驱动中的选择（Linux 源码参考）：
  miscdevice：drivers/char/misc.c（null、zero、random 等简单设备）
  register_chrdev + cdev：块设备、网络设备、大部分真实硬件驱动
```

#### 4.6 两者在 Linux 内核中的关系

```
Linux 字符设备注册层次：

用户态 open("/dev/day24_ivshmem0")
           ↓
         VFS 层（struct inode）
           ↓
    设备号 (major, minor)
           ↓
    chrdevs[major] → cdev → file_operations
           ↑
           │
    ┌──────┴───────┐
    │              │
misc_register   register_chrdev
    │              │
    ↓              ↓
  misc 层      char 层
  (misc.c)    (char_dev.c)

本质：miscdevice 是 register_chrdev 的"高级封装"，
      misc 子系统在底层调用了 register_chrdev，
      但自动处理了次设备号分配和设备节点创建。
```

---

## 五、day24 代码核心解析

### 1. 协议初始化（probe 中）

```c
// day24_proto_init_if_needed() — 只初始化一次
magic = day24_proto_read32(d, DAY24_PROTO_OFF_MAGIC);
version = day24_proto_read32(d, DAY24_PROTO_OFF_VERSION);

if (magic == DAY24_PROTO_MAGIC && version == DAY24_PROTO_VERSION) {
    // 协议已存在，说明是重启后重新映射，不覆盖
    dev_info(...);
    return;
}

// 首次初始化：写入所有字段
day24_proto_write32(d, DAY24_PROTO_OFF_MAGIC, DAY24_PROTO_MAGIC);  // "DY24"
day24_proto_write32(d, DAY24_PROTO_OFF_VERSION, DAY24_PROTO_VERSION);  // 1
day24_proto_write32(d, DAY24_PROTO_OFF_SEQ, 0);
day24_proto_write32(d, DAY24_PROTO_OFF_STATE, DAY24_STATE_READY);
day24_proto_write32(d, DAY24_PROTO_OFF_LEN, 0);
memset_io(payload, 0, capacity);  // 清零 payload 区
```

**为什么需要 init-if-needed？**
```
QEMU 重启 / 驱动重新加载：
  → BAR2 物理内存内容还在（取决于 QEMU 共享内存后端）
  → 但驱动内存映射是新的
  → 如果已经初始化过（magic 匹配），就不重复写入
  → 保证协议状态持久性
```

### 2. read 系统调用路径

```c
static ssize_t day24_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    // 1. 获取当前 payload 长度（从协议头的 LEN 字段）
    payload_len = day24_proto_read32(d, DAY24_PROTO_OFF_LEN);

    // 2. 用 llseek 定位的 *ppos 作为偏移（在 payload 区内）
    // 3. 用 memcpy_fromio 复制（MMIO 地址 → 内核栈）
    memcpy_fromio(kbuf, day24_bar2_ptr(d, DAY24_PROTO_PAYLOAD_OFF + (u32)*ppos), to_copy);

    // 4. 复制到用户态（内核栈 → 用户空间）
    copy_to_user(buf, kbuf, to_copy);

    // 5. 更新文件偏移
    *ppos += to_copy;
    return to_copy;
}
```

### 3. write 系统调用路径

```c
static ssize_t day24_write(struct file *file, const char __user *buf, size_t count, ...)
{
    // 1. 边界检查：不让写超过 payload 容量
    if (*ppos >= cap) return -ENOSPC;

    // 2. 用户空间 → 内核栈
    kbuf = memdup_user(buf, to_copy);

    // 3. 内核栈 → MMIO（BARC2 payload 区）
    memcpy_toio(day24_bar2_ptr(d, DAY24_PROTO_PAYLOAD_OFF + (u32)*ppos), kbuf, to_copy);

    // 4. 更新协议头：LEN、STATE、SEQ
    day24_proto_write32(d, DAY24_PROTO_OFF_LEN, new_len);
    day24_proto_write32(d, DAY24_PROTO_OFF_STATE, DAY24_STATE_USER_WRITTEN);
    day24_proto_write32(d, DAY24_PROTO_OFF_SEQ, seq + 1);

    return to_copy;
}
```

### 4. 用户态工具的对应关系

```
用户态程序                  驱动操作                      BAR2 内存
────────────────────────────────────────────────────────────────
mmio-tool info         → ioctl(GET_INFO)            → 读所有协议头字段
mmio-tool mmio-read 0  → ioctl(MMIO_READ32)          → readl(BAR2+0x00)
mmio-tool mmio-write  → ioctl(MMIO_WRITE32)          → writel(BAR2+0x0c)
mmio-tool shm-write   → write()                     → memcpy_toio(payload)
mmio-tool shm-read    → read()                      → memcpy_fromio(payload)
mmio-tool clear       → ioctl(CLEAR_PAYLOAD)        → memset_io(payload)
```

### 5. 完整调用链：从 probe 到用户态交互

**阶段一：驱动加载（probe）**

```
insmod demo.ko
  → module_pci_driver(day24_pci_driver)
  → pci_register_driver()
  
[QEMU PCI 总线枚举，发现 1af4:1110 设备]
  → 内核查找匹配的 pci_device_id
  → 调用 day24_probe(pdev, id)
      ① kzalloc 分配 day24_dev 结构体
      ② pci_enable_device()  使能设备
      ③ pci_request_regions()  声明 BAR0/BAR2
      ④ pci_set_master()
      ⑤ day24_dump_bar()  打印 BAR 信息
      ⑥ day24_map_bar(0/2)  映射 BAR0/BAR2
      ⑦ readl(BAR0)  验证 MMIO 可访问
      ⑧ day24_proto_init_if_needed()  初始化协议（init-if-needed）
      ⑨ misc_register(&d->miscdev)  ← 注册字符设备
          → /dev/day24_ivshmem0 自动创建
```

**阶段二：用户态操作（通过 day24_fops）**

```
open("/dev/day24_ivshmem0")
  → VFS 根据设备号找到 miscdevice
  → 调用 day24_fops.open
  → file->private_data = d（day24_dev 指针）

ioctl(GET_INFO, arg)
  → VFS 找到 day24_fops.unlocked_ioctl
  → 调用 day24_ioctl(GET_INFO)
      → 读所有协议头字段
      → copy_to_user() 返回给用户态

read(fd, buf, 10)
  → VFS 找到 day24_fops.read
  → 调用 day24_read()
      → day24_proto_read32(LEN)  获取有效数据长度
      → memcpy_fromio(kbuf, BAR2+0x20+*ppos)  MMIO → 内核栈
      → copy_to_user(buf, kbuf)  内核栈 → 用户态

write(fd, "hello", 5)
  → VFS 找到 day24_fops.write
  → 调用 day24_write()
      → memdup_user(buf)  用户态 → 内核栈
      → memcpy_toio(BAR2+0x20+*ppos, kbuf)  内核栈 → MMIO
      → day24_proto_write32(LEN, new_len)  更新协议头
      → day24_proto_write32(STATE, USER_WRITTEN)
      → day24_proto_write32(SEQ, seq+1)

llseek(fd, off, SEEK_SET)
  → VFS 找到 day24_fops.llseek
  → 调用 day24_llseek()
      → 检查游标不越界
      → file->f_pos = newpos

close(fd)
  → VFS 调用 day24_fops.release（day24 没定义，用默认）
```

**数据结构关系**

```
day24_dev (私有数据结构，probe 中分配)
  ├── pdev               ← PCI 设备指针
  ├── bar[].vaddr        ← BAR0/BAR2 的 MMIO 虚拟地址
  ├── bar[].start/end/len  ← BAR 物理地址信息
  ├── miscdev            ← miscdevice 描述符
  │      └── .fops = &day24_fops   ← 关键链接点
  ├── lock               ← 保护协议头的互斥锁
  └── device_enabled / regions_claimed  ← 资源标志

day24_fops (file_operations)
  ├── .owner = THIS_MODULE
  ├── .open = day24_open     ← 建立 file→day24_dev 关联
  ├── .llseek = day24_llseek ← payload 游标控制
  ├── .read = day24_read     ← BAR2 → 用户态
  ├── .write = day24_write   ← 用户态 → BAR2
  └── .unlocked_ioctl = day24_ioctl  ← 协议头读写/状态管理
```

**为什么 probe 执行完后用户态才能操作**

```
probe 执行中： misc_register() 还没调用
              /dev/day24_ivshmem0 还不存在
              用户态 open() 会失败（No such device）

probe 执行完： misc_register() 成功
              /dev/day24_ivshmem0 已创建
              用户态可以 open + read/write/ioctl
```

---

## 六、安全检查与边界保护

### 1. MMIO 写入白名单

```c
// 不是 BAR2 的任意偏移都能写！
static bool day24_mmio_offset_allowed(u32 off)
{
    switch (off) {
    case DAY24_PROTO_OFF_SEQ:    // 0x08  ✓
    case DAY24_PROTO_OFF_STATE:  // 0x0c  ✓
    case DAY24_PROTO_OFF_LEN:    // 0x10  ✓
        return true;
    default:
        return false;  // MAGIC/VERSION/PADDING 不可随意改！
    }
}
```

### 2. 读写的边界检查

```c
// MMIO READ/WRITE 边界
if ((mmio.offset & 0x3) ||                        // 必须 4 字节对齐
    mmio.offset >= DAY24_PROTO_PAYLOAD_OFF ||     // 不能超出协议头
    mmio.offset + sizeof(u32) > d->bar[2].len)    // 不能超出 BAR2 总长
    return -EINVAL;

// read/write 的边界（基于 llseek 的游标）
if (*ppos >= payload_capacity)
    return 0;  // EOF
```

---

## 七、day24 在 W4 中的位置

```
W4 (PCIe 基础)
├── day22: 设备枚举可见性
│           理解 PCI enum / lspci / stub 骨架
├── day23: BAR/MMIO 资源接管
│           pci_enable / request_regions / iomap
├── day24: ivshmem MMIO 协议 ← 今天
│           BAR2 共享内存 + 协议头 + misc device + ioctl
├── day25: MSI 中断
│           pci_alloc_irq_vectors / request_irq / doorbell
├── day26-27: 深入学习
└── day28: W4 收口

W5 (DMA 性能)
├── day29: DMA coherent buffer
├── day30: mmap 零拷贝
└── ...
```

---

## 八、一句话总结 day24

> **day24 的目标是在 day23 接管 BAR 资源的基础上，在 BAR2 共享内存上定义一个最小可验证的通信协议（magic/version/seq/state/len/payload），并通过 misc 字符设备 + ioctl + read/write 暴露给用户态。做完这一天，你已经实现了一个"驱动侧写协议头，用户态读写字节"的完整共享内存通信通道。**
