# Day29 深度学习指南 - DMA Coherent 最小闭环

## 一、Day29 是什么？

Day29 是 W5（Week 5）的起点，正式进入 **DMA（Direct Memory Access）** 学习。

**核心目标**：驱动通过 `dma_alloc_coherent()` 申请一块 DMA buffer，让 EDU 设备把数据从 RAM 搬走再搬回来，验证数据一致性。

---

## 二、W4 → W5 的过渡

| 阶段 | 核心能力 | Day29 如何承接 |
|------|-----------|----------------|
| W4 | PCI 枚举、MMIO、MSI 中断 | EDU 设备不变，沿用 1234:11e8 |
| W5 | DMA 传输、mmap、bench | 新增 DMA 能力，保持字符设备接口 |

**为什么继续用 EDU 设备？**
- PCI 设备模型已熟悉（probe/remove）
- MSI 中断路径已验证（IRQ handler）
- 新增的只是 DMA 寄存器（0x80~0x98）和 DMA API

---

## 三、为什么需要 DMA？

### 3.1 CPU 参与的数据传输问题

**传统方式（PIO - Programmed I/O）**：
```
应用数据 → CPU → 设备
应用数据 ← CPU ← 设备
```
- CPU 全程参与，搬运数据
- 大数据量时 CPU 负载高
- 实时性差

**DMA 方式**：
```
应用数据 → RAM → DMA 控制器 → 设备
应用数据 ← RAM ← DMA 控制器 ← 设备
```
- CPU 只发出指令，实际搬运由 DMA 控制器完成
- CPU 在数据传输期间可以执行其他任务
- 高带宽、低延迟

### 3.2 DMA 地址 vs 虚拟地址

**关键概念**：
- **CPU 虚拟地址**（`dma_virt`）：CPU 访问内存用的地址
- **DMA 地址**（`dma_handle`）：设备访问内存用的地址

```
┌─────────────┐         ┌─────────────┐
│    CPU      │         │   设备      │
└──────┬──────┘         └──────┬──────┘
       │ 虚拟地址               │ DMA 地址
       │                        │
       ↓                        ↓
┌─────────────────────────────────────────┐
│              RAM                         │
│  ┌─────────────────────────────────┐    │
│  │  DMA coherent buffer (4KB)      │    │
│  │  - dma_virt: CPU 可访问的虚拟地址 │    │
│  │  - dma_handle: 设备可访问的 DMA 地址│   │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

**为什么不能混用？**
- 设备不知道什么是虚拟地址
- 设备只知道物理地址（或者 DMA 地址）
- `dma_virt` 是 CPU 用的，`dma_handle` 是设备用的

---

## 四、DMA API 详解

### 4.1 `dma_set_mask_and_coherent()`

```c
ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
```

**作用**：告诉设备（和内核）我们期望的 DMA 地址宽度

**参数**：
- `DMA_BIT_MASK(32)` = 0xFFFFFFFF，表示支持 32-bit DMA 地址
- `DMA_BIT_MASK(28)` = 0x0FFFFFFF，表示只支持 28-bit DMA 地址

**为什么 EDU 默认是 28-bit？**
- QEMU EDU 默认 DMA mask = 28 bits
- arm64 virt 下 guest RAM 可能不落在 28-bit 可达窗口
- Day29 自动化用 `-device edu,dma_mask=0xffffffff` 放宽到 32-bit

### 4.2 `dma_alloc_coherent()`

```c
d->dma_virt = dma_alloc_coherent(&pdev->dev, d->dma_bytes,
                                  &d->dma_handle, GFP_KERNEL);
```

**作用**：分配一块"一致性强"（coherent）的 DMA buffer

**返回两个值**：
- `dma_virt`：CPU 访问用的虚拟地址
- `dma_handle`：设备访问用的 DMA 地址

**"coherent"是什么意思？**
- CPU 写入后，设备立即能看到（无需 cache flush）
- 设备写入后，CPU 立即能看到（无需 cache invalidate）
- 操作系统保证 cache 一致性

### 4.3 `dma_free_coherent()`

```c
dma_free_coherent(&pdev->dev, d->dma_bytes, d->dma_virt, d->dma_handle);
```

**必须成对使用**，释放顺序无所谓（因为是释放，不是使用）

---

## 五、EDU DMA 寄存器布局

### 5.1 新增寄存器（相比 Day25）

| 偏移 | 名称 | 方向 | 说明 |
|------|------|------|------|
| 0x80 | DMA_SRC | 写入 | DMA 源地址（必须是 DMA 地址，不是虚拟地址） |
| 0x88 | DMA_DST | 写入 | DMA 目的地址 |
| 0x90 | DMA_COUNT | 写入 | 传输字节数 |
| 0x98 | DMA_CMD | 写入 | 命令寄存器 |

### 5.2 DMA_CMD 位定义

| 位 | 名称 | 说明 |
|----|------|------|
| bit0 | START | 写入 1 启动 DMA 传输 |
| bit1 | DIR | 0=RAM→EDU, 1=EDU→RAM |
| bit2 | IRQ | DMA 完成后触发中断 |

**第一次 DMA（RAM → EDU）**：
```
DMA_CMD = START | IRQ = 0x01 | 0x04 = 0x05
```

**第二次 DMA（EDU → RAM）**：
```
DMA_CMD = START | DIR | IRQ = 0x01 | 0x02 | 0x04 = 0x07
```

### 5.3 EDU 内部 buffer

EDU 设备内部有一个 buffer，偏移固定为 `0x40000`。

Day29 的 round-trip：
```
1. RAM (dma_handle + 0)      → EDU (0x40000)
2. EDU (0x40000)             → RAM (dma_handle + 2048)
```

---

## 六、day29_dev 结构体

### 6.1 新增字段

```c
struct day29_dev {
    /* === PCI 资源（与 Day27 相同）=== */
    struct pci_dev *pdev;
    void __iomem *bar0;
    resource_size_t bar0_start;
    resource_size_t bar0_len;

    /* === 中断资源（与 Day27 相同）=== */
    unsigned int irq_vector;
    u64 irq_count;
    u32 last_irq_status;
    u32 last_ack_value;
    spinlock_t irq_lock;

    /* === DMA 资源（新增）=== */
    void *dma_virt;           /* CPU 访问用的虚拟地址 */
    dma_addr_t dma_handle;    /* 设备访问用的 DMA 地址 */
    size_t dma_bytes;         /* buffer 大小（4096） */
    u32 dma_mask_bits;         /* DMA mask 位数（32） */

    /* === DMA 验证结果（新增）=== */
    u32 last_verify_len;      /* 最近一次验证长度 */
    u32 last_verify_seed;     /* 验证使用的 seed */
    s32 last_verify_error;    /* 验证错误码 */
    u32 last_verify_ok;       /* 验证是否成功 */
    s32 last_mismatch_index;  /* 第一个不匹配的偏移 */
    u8 last_mismatch_expected; /* 期望值 */
    u8 last_mismatch_actual;   /* 实际值 */
    u32 last_irq_delta;        /* 验证期间 IRQ 增量 */
    u32 last_dma_cmd;          /* 最近一次 DMA 命令 */

    /* === 操作锁（新增）=== */
    struct mutex op_lock;      /* 保护 DMA 操作，防止并发 */

    /* === 字符设备资源（与 Day27 相同）=== */
    dev_t devt;
    struct cdev cdev;
    struct device *device;
};
```

### 6.2 buffer 布局设计

```
4KB coherent buffer（src 和 dst 各 2048 bytes）
├── [0 ~ 2047]    src 区：存放源数据
└── [2048 ~ 4095] dst 区：存放 DMA 搬回的数据
```

**为什么分两半？**
- 同一块 buffer 内完成往返
- 避免申请第二块 buffer
- 验证时直接比较 src 和 dst

---

## 七、DMA round-trip 完整流程

### 7.1 用户态触发

```bash
day29_edu_dma_tool /dev/day29_edu0 verify 256 0x41
```

### 7.2 内核驱动执行

```
day29_do_verify(d, len=256, seed=0x41)
    │
    ├─ 1. 参数校验
    │     len > 0 && len <= 2048
    │     d->dma_virt != NULL
    │
    ├─ 2. 准备源数据
    │     src = dma_virt + 0
    │     dst = dma_virt + 2048
    │     src_dma = dma_handle + 0
    │     dst_dma = dma_handle + 2048
    │
    │     memset(dst, 0, 256)         // 清零目标区
    │     fill_pattern(src, 256, 0x41) // 填充模式
    │     // src[i] = (0x41 + i) & 0xFF
    │
    ├─ 3. 第一次 DMA：RAM → EDU
    │     program_dma(src_dma, 0x40000, 256, CMD_START | CMD_IRQ)
    │     // 等 DMA 完成（轮询 CMD START 位）
    │
    ├─ 4. 第二次 DMA：EDU → RAM
    │     program_dma(0x40000, dst_dma, 256, CMD_START | CMD_DIR | CMD_IRQ)
    │     // 等 DMA 完成
    │
    ├─ 5. 比较结果
    │     for i in 0..255:
    │         if src[i] != dst[i]:
    │             记录 mismatch_index, mismatch_expected, mismatch_actual
    │             return -EIO
    │     verify_ok = 1
    │     return 0
    │
    └─ 6. 返回验证结果
```

### 7.3 数据流图

```
┌──────────────────────────────────────────────────────────────────────┐
│                    DMA Round-Trip 数据流                              │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  coherent buffer (4KB)                                              │
│  ┌────────────────────────┬─────────────────────────┐                │
│  │    src [0~2047]        │    dst [2048~4095]      │                │
│  │    ┌─────────────┐     │    ┌─────────────┐     │                │
│  │    │ 0x41,0x42..│     │    │ 0x00,0x00.. │     │                │
│  │    └─────────────┘     │    └─────────────┘     │                │
│  └────────────────────────┴─────────────────────────┘                │
│         ↑                                              │                │
│         │                                              │                │
│         │ 1st DMA                                     │ 2nd DMA        │
│         │ (RAM→EDU)                                 │ (EDU→RAM)      │
│         │                                              │                │
│  ┌──────┴──────────────────────────────────────────────┴──────┐      │
│  │                     EDU 设备                                │      │
│  │                                                       │      │
│  │     内部 buffer (偏移 0x40000)                         │      │
│  │     ┌─────────────────────┐                           │      │
│  │     │ 0x41,0x42,0x43...  │  ←─ DMA 写入              │      │
│  │     └─────────────────────┘                           │      │
│  │                                                       │      │
│  └───────────────────────────────────────────────────────────────┘      │
│                                                                      │
│  DMA 寄存器：                                                        │
│    DMA_SRC  = dma_handle + 0        (第一次) / 0x40000 (第二次)      │
│    DMA_DST  = 0x40000               (第一次) / dma_handle + 2048     │
│    DMA_CNT  = 256                                                     │
│    DMA_CMD  = 0x05 (START|IRQ)   (第一次) / 0x07 (START|DIR|IRQ)    │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 八、完整调用链

### 8.1 模块加载（insmod）

```
insmod day29_edu_dma.ko
    ↓
day29_init()
    ├→ alloc_chrdev_region()           // 分配设备号
    ├→ class_create()                  // 创建 sysfs 类
    └→ pci_register_driver()           // 注册 PCI 驱动
    ↓
pci_bus_driver.probe()               // 发现 1234:11e8
    ↓
day29_probe()
    ├→ kzalloc(day29_dev)              // 分配私有数据
    ├→ dma_set_mask_and_coherent()     // 【新增】设置 DMA mask
    ├→ pci_enable_device()             // 启用 PCI 设备
    ├→ pci_request_regions()           // 请求 BAR 资源
    ├→ pci_iomap(BAR0)                // 映射 BAR0 MMIO
    ├→ dma_alloc_coherent()            // 【新增】分配 DMA buffer
    ├→ pci_alloc_irq_vectors()         // 分配 MSI 中断
    ├→ request_irq()                    // 注册 IRQ handler
    └→ day29_setup_chrdev()           // 创建字符设备
    ↓
"probe success"
"dma_alloc_coherent ok: virt=... dma=0x... bytes=4096"
```

### 8.2 用户触发 verify

```
day29_edu_dma_tool /dev/day29_edu0 verify 256 0x41
    ↓
open("/dev/day29_edu0")
    ↓
ioctl(RUN_VERIFY, {len=256, pattern_seed=0x41})
    ↓
day29_ioctl(cmd=RUN_VERIFY)
    ↓
day29_do_verify(d, len=256, seed=0x41)
    ├→ mutex_lock(&d->op_lock)
    ├→ fill_pattern(src, 256, 0x41)
    ├→ memset(dst, 0, 256)
    ├→ program_dma(src_dma, 0x40000, 256, START|IRQ)
    │     ├→ writel(src_dma, DMA_SRC)
    │     ├→ writel(0x40000, DMA_DST)
    │     ├→ writel(256, DMA_COUNT)
    │     ├→ writel(START|IRQ, DMA_CMD)
    │     └→ 轮询等待 DMA 完成
    ├→ program_dma(0x40000, dst_dma, 256, START|DIR|IRQ)
    │     └→ 同上
    ├→ 比较 src 和 dst，逐字节验证
    ├→ mutex_unlock(&d->op_lock)
    └→ return verify_ok ? 0 : -EIO
```

---

## 九、ioctl 命令

| 命令 | 功能 | 输入 | 输出 |
|------|------|------|------|
| GET_INFO | 获取完整状态 | 无 | struct day29_info |
| RUN_VERIFY | 执行 DMA 往返验证 | struct day29_verify_req | 0 或错误码 |
| GET_VERIFY_RESULT | 获取验证结果 | 无 | struct day29_verify_result |
| RESET_STATS | 重置统计计数 | 无 | 0 |

---

## 十、与 Day30 的关系

Day29 和 Day30 的分工：

| 特性 | Day29 | Day30 |
|------|-------|-------|
| DMA buffer | 内核分配 | 内核分配 + mmap 给用户态 |
| 验证逻辑 | 内核比较 | 逐步移到用户态 |
| 目的 | 学习 DMA API | 学习 mmap 零拷贝 |

**为什么 Day29 先在内核比较？**
- 逻辑最短，最容易验证
- Day30 可以在此基础上把"比较"移到用户态

---

## 十一、验收标准

### 11.1 必须满足

- `dma_set_mask_and_coherent()` 成功
- `dma_alloc_coherent()` 成功
- `verify_ok=1`（DMA 往返校验通过）
- 无 `DMA mapping error` / `BUG:` / `Oops:`

### 11.2 关键证据

```
verify-result.txt:
  verify_len=256
  verify_seed=0x41
  verify_ok=1           ← 必须为 1
  mismatch_index=-1      ← -1 表示无 mismatch
  irq_delta=2            ← 两次 DMA 各触发一次 IRQ
  last_dma_cmd=0x07     ← 第二次 DMA 命令
```

---

## 十二、面试要会讲的五句话

1. **"为什么 EDU 默认要求 28-bit DMA mask"**
   → QEMU EDU 默认 DMA mask 是 28 bits，arm64 virt 下 guest RAM 可能不在 28-bit 可达范围，所以 Day29 自动化用 `-device edu,dma_mask=0xffffffff` 放宽到 32-bit

2. **"dma_alloc_coherent() 返回两个地址"**
   → 返回 CPU 访问用的虚拟地址（dma_virt）和设备访问用的 DMA 地址（dma_handle），两者不是同一个东西

3. **"为什么不能把 dma_virt 写进 DMA 寄存器"**
   → 设备不认识虚拟地址，只认识 DMA 地址（通常是物理地址或总线地址）

4. **"Day29 先做 coherent，再到 Day30 做 mmap"**
   → coherent 保证 cache 一致性，mmap 把 buffer 映射给用户态，两者结合实现零拷贝 DMA

5. **"Day29 的往返两次 DMA"**
   → RAM → EDU(0x40000) → RAM，把 EDU 内部 buffer 当中转，验证数据完整性
