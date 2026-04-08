# Day23 深度学习指南

## 一、day23 到底要学什么

### 1. 核心问题：如何接管 PCI 设备资源？

day22 验证了"设备能被看见"，day23 要回答：**驱动如何真正接管设备的 BAR 资源？**

### 2. day22 vs day23 的本质区别

| | day22 | day23 |
|---|---|---|
| 做了什么 | 只打印设备信息 | **真正接管设备资源** |
| pci_enable_device | 没做 | **做了** |
| pci_request_regions | 没做 | **做了** |
| pci_set_master | 没做 | **做了** |
| pci_iomap | 没做 | **做了** |
| remove 对称释放 | 没做 | **做了** |

### 3. 学习层次

| 层次 | 内容 | day23 要验证 |
|---|---|---|
| 资源请求 | pci_enable / request_regions | 成功返回 0 |
| BAR 映射 | pci_iomap() | 得到有效虚拟地址 |
| MMIO 读取 | readl(BAR0) | 读到设备寄存器值 |
| 资源释放 | remove 中的逆序操作 | 无泄漏 |

---

## 二、PCI 资源接管原理

### 1. PCI 设备使用资源的三个阶段

```
阶段 1: 枚举（Enumeration）
  → PCI core 扫描总线，发现设备，读取 vendor/device ID
  → 分配 struct pci_dev，但不分配实际资源

阶段 2: 资源申请（Resource Claim）  ← day23 重点
  → 驱动调用 pci_enable_device() "使能"设备
  → 驱动调用 pci_request_regions() "申请" BAR 资源独占访问
  → 驱动调用 pci_iomap() 把物理地址映射成 CPU 可访问的虚拟地址

阶段 3: 实际使用（Usage）
  → 驱动读写 MMIO / 使用 DMA / 处理中断
  → day24 会在这里继续
```

### 2. 为什么需要 pci_enable_device()

```
类比：设备刚接上电源，但开关还没打开
  ↓
pci_enable_device() = 打开设备开关
  ↓
之后才能读写设备的寄存器
```

**原理**：
- PCI 设备默认处于低功耗状态
- `pci_enable_device()` 会激活设备的 BAR 地址解码
- 不 enable 就直接访问 MMIO？硬件上根本不会响应！

### 3. 为什么需要 pci_request_regions()

```
类比：设备有多个停车场车位（BAR0, BAR1, BAR2...）
  ↓
pci_request_regions() = "这些车位我已经占了，别人不许停"
  ↓
防止两个驱动同时访问同一设备导致冲突
```

**注意**：
- 这是**声明式**的，不是真的分配地址
- 地址在 BIOS/枚举阶段已经分配好了
- 这里只是声明"这个驱动要用"
- 失败返回 `-EBUSY` 表示被其他驱动占了

### 4. 为什么需要 pci_iomap()

```
问题：BAR 存储的是物理地址，CPU 不能直接用物理地址访问内存

物理地址（BAR 里存的）：0x0000000010081000
     ↓ pci_iomap() 转换
虚拟地址（CPU 能用的）：0xFFFF800000108000
```

```c
void __iomem *bar0_vaddr;

// 参数 1: pci_dev
// 参数 2: BAR 编号（0 = BAR0）
// 参数 3: 映射长度，0 表示整个 BAR
bar0_vaddr = pci_iomap(pdev, 0, 0);

if (!bar0_vaddr) {
    // 映射失败
}

// 现在可以通过 bar0_vaddr 访问设备 MMIO 了
u32 val = readl(bar0_vaddr);  // 读 32-bit
writel(val, bar0_vaddr);       // 写 32-bit
```

### 5. 为什么需要 pci_set_master()

```
作用：让设备有能力发起 DMA 事务

PCI 设备两种模式：
  - 总线从设备（Target）：只能接收 CPU 请求
  - 总线主设备（Master）：可以主动发起总线访问（如 DMA）

ivshmem 需要是 master 才能完成共享内存数据搬运
```

---

## 三、day23 代码核心解析

### 1. probe 函数的完整流程

```c
static int day23_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    // ========== 第1步：分配私有数据结构 ==========
    d = kzalloc(sizeof(*d), GFP_KERNEL);

    // ========== 第2步：使能 PCI 设备 ==========
    rc = pci_enable_device(pdev);  // ← day23 新增
    if (rc) goto err_free;

    // ========== 第3步：申请 BAR 资源独占访问 ==========
    rc = pci_request_regions(pdev, "day23_ivshmem_probe");  // ← day23 新增
    if (rc) goto err_disable;

    // ========== 第4步：设置为主设备 ==========
    pci_set_master(pdev);  // ← day23 新增

    // ========== 第5步：读取并保存 BAR 信息 ==========
    day23_dump_bar(pdev, d, 0);  // BAR0: 寄存器窗口
    day23_dump_bar(pdev, d, 2);  // BAR2: 共享内存

    // ========== 第6步：映射 BAR 到虚拟地址 ==========
    rc = day23_map_bar(d, 0);    // ← day23 核心操作
    if (rc) goto err_regions;

    rc = day23_map_bar(d, 2);
    if (rc) goto err_unmap;

    // ========== 第7步：验证 MMIO 可访问 ==========
    if (d->bar[0].vaddr) {
        d->bar0_first_dword = readl(d->bar[0].vaddr);  // ← 读第一个 dword
        dev_info(&pdev->dev, "BAR0 first dword=0x%08x\n",
                 d->bar0_first_dword);
    }

    return 0;  // 成功

    // ========== 错误处理（按逆序释放） ==========
err_unmap:
    day23_unmap_bars(d);        // 取消 BAR 映射
err_regions:
    pci_release_regions(pdev);  // 释放 BAR 资源
err_disable:
    pci_disable_device(pdev);   // 关闭设备
err_free:
    kfree(d);
    return rc;
}
```

### 2. remove 函数的逆序清理

```c
static void day23_remove(struct pci_dev *pdev)
{
    // ========== 逆序释放资源 ==========

    // ① 先解除 BAR 映射（最后映射的最先解除）
    day23_unmap_bars(d);

    // ② 释放 BAR 资源
    if (d->regions_claimed)
        pci_release_regions(pdev);

    // ③ 关闭设备
    if (d->device_enabled)
        pci_disable_device(pdev);

    // ④ 释放私有数据
    kfree(d);
}
```

**对称性原则**：

| probe 顺序 | remove 逆序 |
|---|---|
| kzalloc 私有数据 | kfree |
| pci_enable_device | pci_disable_device |
| pci_request_regions | pci_release_regions |
| pci_set_master | (内核自动处理) |
| pci_iomap BAR0/BAR2 | pci_iounmap |

### 3. BAR 信息结构体

```c
struct day23_bar_info {
    int index;              // BAR 编号（0-5）
    resource_size_t start;  // 起始物理地址
    resource_size_t end;    // 结束物理地址
    resource_size_t len;    // 长度
    unsigned long flags;    // 属性（IORESOURCE_MEM 等）
    void __iomem *vaddr;    // 映射后的虚拟地址 ← 关键！
};
```

### 4. BAR 类型解释

```
ivshmem (1af4:1110) 的 BAR：
  BAR0: 256 字节 = 0x100   → 寄存器窗口（MMIO）
  BAR1: （未使用）
  BAR2: 4MB = 0x400000    → 共享内存区域 ← 实际数据传输用
```

---

## 四、错误处理与调试

### 1. 常见错误返回值

| 错误 | 含义 | 常见原因 |
|---|---|---|
| `-ENOMEM` | 内存不足 | kzalloc 失败 |
| `-EBUSY` | 资源被占用 | pci_request_regions 失败 |
| `-EIO` | I/O 错误 | pci_enable_device 失败 |
| NULL (iomap) | 映射失败 | BAR 不存在或已被占用 |

### 2. 调试技巧

```bash
# 查看驱动加载时的内核日志
dmesg | grep day23

# 查看 PCI 设备状态
lspci -vv -nn -d 1af4:1110

# 查看 BAR 映射
cat /proc/iomem | grep 1af4
```

---

## 五、day23 在 W4 中的位置

```
W4 (PCIe 基础)
├── day22: 设备枚举可见性
│           理解 PCI enum / lspci / stub 骨架
├── day23: BAR/MMIO 资源接管 ← 今天
│           pci_enable / request_regions / iomap
├── day24: ivshmem MMIO 协议
│           读写 BAR 映射的共享内存
├── day25: MSI 中断
│           pci_alloc_irq_vectors / request_irq
└── ...

W5 (DMA 性能)
├── day29: DMA coherent buffer
├── day30: mmap 零拷贝
└── ...
```

---

## 六、一句话总结 day23

> **day23 的目标是真正接管 PCI 设备资源：使能设备、申请 BAR、映射 MMIO，并在 remove 中按对称顺序释放。做完这一天，驱动才算真正"开上车"，而不是只"看见车"。**
