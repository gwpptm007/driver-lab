# Day22 深度学习指南

## 一、day22 到底要学什么

### 1. 核心问题：PCI 设备是怎么被"看见"的？

在写驱动之前，必须先理解：**Linux 是怎么发现并枚举 PCI 设备的？**

这是所有 PCI 驱动的起点——设备都看不见，后面的 BAR 映射、MSI 中断、DMA 都是空谈。

### 2. 学习层次

| 层次 | 内容 | day22 要验证 |
|---|---|---|
| 硬件层 | QEMU 模拟的 ivshmem 设备 | 设备 ID `1af4:1110` 存在 |
| BIOS/固件层 | ACPI / host bridge | PCI host bridge 初始化日志 |
| Linux PCI Core 层 | `pci_bus` 枚举 | `lspci` 能看到设备 |
| 驱动层 | 还没到这步 | stub 只是骨架 |

### 3. 为什么选 `ivshmem-plain`

两种 ivshmem 设备：
- **ivshmem-plain** (`1af4:1110`)：简单共享内存，无 doorbell 中断
- **ivshmem-doorbell** (`1af4:1111`)：带 doorbell 中断，更复杂

day22 用 plain 是因为：**今天只验证枚举，不验证中断**

---

## 二、PCI 枚举原理

### 1. PCI 系统结构

```
    ┌─────────────────┐
    │   CPU           │
    └────────┬────────┘
             │
    ┌────────▼────────┐
    │  PCI Host       │  ← 主板上的 PCI 控制器
    │  Bridge         │
    └────────┬────────┘
             │ bus 0
    ┌────────▼────────┐
    │  PCI Bus 0      │
    │                 │
    │  ┌──────────┐   │  ← ivshmem (1af4:1110) 在这条总线上
    │  │ ivshmem  │   │
    │  └──────────┘   │
    └─────────────────┘
```

### 2. Linux PCI 枚举过程（简化）

```
1. 系统启动 → PCI Host Bridge 驱动初始化
2. 扫描 Bus 0 上的所有设备
3. 对每个设备读取 vendor ID / device ID
4. 如果 vendor ID = 0xFFFF（无效），说明没有设备
5. 如果读到有效 ID，分配 struct pci_dev
6. 读取 BAR（Base Address Registers）
7. 注册到 pci_bus
```

### 3. 为什么需要 BAR（Base Address Registers）

每个 PCI 设备需要告诉系统"我用了哪些内存地址"，这就是 BAR：

```
设备上的寄存器：
  BAR0: 我使用 256 字节的 MMIO，起始地址由系统分配
  BAR1: 我使用 4MB 的共享内存，起始地址由系统分配
  BAR2: 我使用 1 个 IRQ 线
```

系统枚举时会把实际地址"写入"这些 BAR，之后驱动就可以通过这些地址访问设备。

---

## 三、为什么需要 `lspci`

### 1. `lspci` 是用户态查看 PCI 枚举结果的标准工具

```bash
# 简化输出
$ lspci -nn
00:02.0 Device [1af4:1110]: Intel ...  # -nn = 显示 vendor:device ID

# 详细输出（含 BAR）
$ lspci -vv -nn
00:02.0 Device [1af4:1110]
        Region 0: Memory at <addr> [size=256]    # BAR0
        Region 2: Memory at <addr> [size=4M]      # BAR2
        Interrupt: pin A routed to IRQ 32
```

### 2. `lspci` 工作原理

`lspci` 读取 `/sys/bus/pci/` sysfs 文件系统：

```
/sys/bus/pci/devices/
├── 0000:00:00.0/     ← host bridge
│   ├── vendor        ← 读取得到 "0x1234"
│   ├── device        ← 读取得到 "0x1100"
│   ├── class         ← 读取得到 "0x060000"
│   ├── resource      ← 读取得到 BAR 信息
│   └── config        ← 原始 PCI config space
│
└── 0000:00:02.0/     ← ivshmem
    ├── vendor        ← "0x1af4"
    ├── device        ← "0x1110"
    ├── resource      ← BAR 信息
    └── ...
```

### 3. 为什么 guest 需要静态编译的 `lspci`

```
宿主机：x86_64 Ubuntu        目标：arm64 QEMU guest
    │                              │
    │  不能直接 cp /usr/bin/lspci  │
    │  因为架构不同！              │
    │                              │
    └───► 需要 aarch64 架构的 ────►│
           静态链接 lspci
```

**动态链接**：`lspci` 依赖 `libpci` 等动态库，guest 是最小 rootfs 没有这些库
**静态链接**：把依赖的库全部打包进一个可执行文件，最小 rootfs 也能运行

---

## 四、day22 代码引用关系

### 1. `lspci` 在 day22 中的使用

```
guest/init.day22 执行流程：
    │
    ├── lspci -nn          → records/lspci-nn.txt
    ├── lspci -vv -nn      → records/lspci-vv-nn.txt
    └── 对比 pci_sysfs_dump 输出
```

### 2. `pci_sysfs_dump.c` — 直接读 sysfs 的备选工具

```c
// 作用：不依赖 lspci，直接读 /sys/bus/pci/devices/
// 遍历每个设备目录，读取 vendor/device/resource/config

// 例如读取 vendor
char path[256];
sprintf(path, "/sys/bus/pci/devices/%s/vendor", bdf);
// 打开并读取，得到 "0x1af4"
```

### 3. `day22_ivshmem_stub.c` — 驱动骨架

```c
// 只是注册了 pci_driver，还没做任何资源操作
static const struct pci_device_id day22_pci_ids[] = {
    { PCI_DEVICE(0x1af4, 0x1110) },  // 匹配 ivshmem-plain
    { 0, }
};

// 当 kernel 枚举到 1af4:1110 时，probe 被调用
static int day22_probe(struct pci_dev *pdev, ...)
{
    // 此时设备已经被 PCI core 发现
    // 但驱动还没有做 pci_enable_device / request_regions

    dev_info(&pdev->dev, "probe enter: vendor=%04x device=%04x\n",
             pdev->vendor, pdev->device);
    // 打印 BAR 信息（从 sysfs 读的，还没映射）
    day22_dump_resources(pdev);
    return 0;
}
```

---

## 五、day22 在 W4 中的位置

```
W4 (PCIe 基础)
├── day22: 设备枚举可见性 ← 今天
│           理解 lspci、sysfs、PCI enum
├── day23: BAR/MMIO 资源接管
│           pci_enable_device / pci_request_regions / pci_iomap
├── day24: ivshmem 寄存器
│           读写 BAR 映射的共享内存
├── day25: MSI 中断
│           pci_alloc_irq_vectors / request_irq
├── day26-27: 深入学习
├── day28: W4 收口

W5 (DMA 性能)
├── day29: DMA coherent buffer ← 真正用 PCI DMA
├── day30: mmap 零拷贝
└── ...
```

---

## 六、一句话总结 day22

> **day22 的目标是验证 QEMU 已经把 ivshmem-plain 设备成功枚举到 PCI bus 上，并且你能通过 `lspci` / `sysfs` 看到它的 vendor ID、device ID 和 BAR 信息。驱动（`day22_ivshmem_stub.c`）只是预先放好的骨架，今天不接管任何实际资源。**
