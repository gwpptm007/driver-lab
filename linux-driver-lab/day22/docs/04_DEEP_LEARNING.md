# Day22 PCI 枚举与驱动骨架深度指南 - W4 起点

## 一、Day22 是什么？

Day22 是 W4（PCIe 基础）的第一天，也是整个驱动实验室的**新篇章**。

**核心目标**：验证 QEMU 模拟的 `ivshmem-plain` 设备已被 Linux PCI core 成功枚举，并理解 **PCI 驱动骨架**。

Day22 不做实际的资源操作（不使能设备、不映射 BAR、不申请中断），只验证：
1. 设备能被 `lspci` 看见
2. 驱动骨架的 probe/remove 生命周期能跑通

---

## 二、W4 的学习路径

### 2.1 W4 整体架构

```
W4 (PCIe 基础 - day22-28)
├── day22: 设备枚举可见性     ← 今天
│           理解 lspci、sysfs、PCI enum
├── day23: BAR/MMIO 资源接管
│           pci_enable_device / pci_request_regions / pci_iomap
├── day24: ivshmem 寄存器
│           读写 BAR 映射的共享内存
├── day25: MSI 中断
│           pci_alloc_irq_vectors / request_irq
├── day26-27: 深入学习
├── day28: W4 收口

W5 (DMA + 性能 - day29-35)
├── day29: DMA coherent buffer ← 真正用 PCI DMA
├── day30: mmap 零拷贝
├── day31: benchmarking
├── day32: perf + ftrace
├── day33: ftrace function_graph
├── day34: 稳定性测试
└── day35: 阶段报告收口
```

### 2.2 为什么先学 PCIe？

```
字符设备（W1-W3）：
  - 简单，门槛低
  - 但与真实硬件关系不大

PCIe 驱动（W4）：
  - 真实硬件总线
  - 涉及 MMIO、DMA、中断、电源管理
  - 面试高频考点

DMA + 性能（W5）：
  - PCIe 的进阶应用
  - benchmark + 性能分析
```

---

## 三、PCI 枚举原理

### 3.1 PCI 系统结构

```
┌─────────────────────────────────────────────────────────────────────┐
│                         PCI 系统拓扑                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│    ┌─────────────────┐                                               │
│    │      CPU        │                                               │
│    └────────┬────────┘                                               │
│             │                                                        │
│    ┌────────▼────────┐                                               │
│    │   PCI Host      │  ← 主板上的 PCI 控制器                        │
│    │   Bridge        │    通常是南桥或芯片组的一部分                  │
│    └────────┬────────┘                                               │
│             │                                                        │
│    ┌────────▼────────┐                                               │
│    │   PCI Bus 0     │                                               │
│    │                  │                                               │
│    │  ┌────────────┐ │                                               │
│    │  │ ivshmem    │ │  ← QEMU 模拟的共享内存设备                   │
│    │  │ 1af4:1110  │ │    vendor=0x1af4 (Red Hat)                   │
│    │  └────────────┘ │    device=0x1110 (ivshmem-plain)             │
│    │                  │                                               │
│    └──────────────────┘                                               │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.2 Linux PCI 枚举过程

```
1. 系统启动 → PCI Host Bridge 驱动初始化
   ↓
2. 扫描 Bus 0 上的所有设备（通过 config space）
   ↓
3. 对每个设备读取 Vendor ID / Device ID
   ↓
4. 如果 Vendor ID = 0xFFFF → 无设备，跳过
   ↓
5. 如果读到有效 ID → 分配 struct pci_dev
   ↓
6. 读取 BAR（Base Address Registers）
   ↓
7. 分配 IRQ 中断线
   ↓
8. 注册到 pci_bus
```

### 3.3 为什么需要 BAR？

```
BAR（Base Address Registers）是 PCI 设备的"门牌号"：

设备在 config space 中声明：
  BAR0: 我需要 256 字节的 MMIO 地址范围
  BAR2: 我需要 4MB 的共享内存地址范围

系统枚举时：
  1. 分配地址：BAR0 = 0xFE010000, BAR2 = 0xFE100000
  2. 写入设备的 config space
  3. 驱动读取 BAR 得到地址，就可以访问设备寄存器

为什么需要这个机制？
  → PCI 地址是系统统一分配的，不是设备自己定的
  → 类似于 DHCP 给设备分配 IP，而不是设备自己指定 IP
```

---

## 四、ivshmem 设备详解

### 4.1 两种 ivshmem 设备

```
QEMU 提供两种 ivshmem 设备：

1. ivshmem-plain (1af4:1110)
   - 简单的共享内存区域
   - 无中断能力
   - day22-24 使用这个

2. ivshmem-doorbell (1af4:1111)
   - 带 doorbell 中断（类似 PCIe MSI）
   - 支持虚拟机之间通过共享内存通信
   - day25+ 使用这个
```

### 4.2 QEMU 命令行参数

```bash
# ivshmem-plain 的 QEMU 配置
-device ivshmem-plain,memdev=ivshmem,size=4M,id=ivshmem0

# ivshmem-doorbell 的 QEMU 配置
# -device ivshmem-doorbell,memdev=ivshmem,vectors=4,ioeventfd=off,id=ivshmem0
```

**参数解释**：
- `memdev=ivshmem`：指定共享内存对象
- `size=4M`：共享内存大小
- `id`：QEMU 内部设备 ID

### 4.3 ivshmem 的 PCI BAR 布局

```
BAR0 (256 bytes)：寄存器
  - 包含设备 ID、版本号等配置寄存器

BAR2 (4MB)：共享内存
  - QEMU 分配的共享内存区域
  - guest 和 host 可以共享访问
```

---

## 五、lspci 工具详解

### 5.1 lspci 是什么？

```
lspci 是用户态查看 PCI 枚举结果的标准工具。

它读取 /sys/bus/pci/ sysfs 文件系统，展示设备信息。
```

### 5.2 常用选项

```bash
# 基本输出（设备概览）
lspci -nn
# 输出示例：
# 00:02.0 Device [1af4:1110]: Intel Corp Device 1110

# 详细输出（含 BAR、IRQ）
lspci -vv -nn
# 输出示例：
# 00:02.0 Device [1af4:1110]
#         Region 0: Memory at <addr> [size=256]
#         Region 2: Memory at <addr> [size=4M]
#         Interrupt: pin A routed to IRQ 32

# 仅显示特定设备
lspci -d 1af4:1110
```

### 5.3 为什么 guest 需要静态编译的 lspci？

```
宿主机：x86_64 Ubuntu
目标：arm64 QEMU guest

问题：
  不能直接 cp /usr/bin/lspci 到 guest
  因为架构不同！（x86_64 vs arm64）

解决方案：
  静态链接：在宿主机上编译一个 aarch64 静态链接的 lspci
  静态链接 = 把所有依赖库（libpci）都打包进一个可执行文件
  好处：最小 rootfs 也能运行

验证静态链接：
  file ~/workspace/kernel-src/pciutils-3.9.0/output/aarch64-linux-gnu/bin/lspci
  # 应该输出：ELF 64-bit LSB executable, ARM aarch64
```

### 5.4 lspci 工作原理

```
lspci 读取 sysfs 文件系统：

/sys/bus/pci/devices/
├── 0000:00:00.0/          ← PCI host bridge
│   ├── vendor             ← 读取得到 "0x1234"
│   ├── device             ← 读取得到 "0x1100"
│   ├── class              ← 读取得到 "0x060000"
│   └── resource          ← BAR 信息
│
└── 0000:00:02.0/          ← ivshmem
    ├── vendor             ← "0x1af4"
    ├── device             ← "0x1110"
    ├── resource          ← BAR 信息
    └── ...
```

---

## 六、PCI 驱动骨架

### 6.1 pci_driver 的七个组成部分

```
┌─────────────────────────────────────────────────────────────────────┐
│                    pci_driver 骨架                                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  1. pci_device_id 数组                                                │
│     → 告诉内核"我匹配哪些 vendor:device"                             │
│                                                                      │
│  2. pci_driver 结构体                                                │
│     → 注册到内核的核心结构，包含 name/id_table/probe/remove          │
│                                                                      │
│  3. probe()                                                         │
│     → 设备插入时调用（枚举成功匹配后）                               │
│                                                                      │
│  4. remove()                                                        │
│     → 设备拔出或驱动卸载时调用                                       │
│                                                                      │
│  5. module_pci_driver()                                             │
│     → 模块入口（简化写法，等价于 init+exit）                         │
│                                                                      │
│  6. day22_stub_dev 结构体                                           │
│     → 驱动私有数据                                                   │
│                                                                      │
│  7. day22_dump_resources()                                          │
│     → 读取并打印 BAR 信息                                           │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 6.2 pci_device_id 数组

```c
// 定义支持的设备列表
static const struct pci_device_id day22_pci_ids[] = {
    { PCI_DEVICE(DAY22_IVSHMEM_VENDOR_ID, DAY22_IVSHMEM_DEVICE_ID) },
    // ↑ 匹配 vendor=0x1af4, device=0x1110
    { 0, }  // 结束标记，必须有
};
MODULE_DEVICE_TABLE(pci, day22_pci_ids);

// MODULE_DEVICE_TABLE 的作用：
//   把 id 列表导出到 sysfs，
//   让 PCI bus 在枚举时能匹配到这个驱动
```

### 6.3 pci_driver 结构体

```c
static struct pci_driver day22_pci_driver = {
    .name = DRV_NAME,           // 驱动名称（用于日志）
    .id_table = day22_pci_ids,  // 匹配的设备列表
    .probe = day22_probe,        // 设备插入回调
    .remove = day22_remove,      // 设备拔出回调
};

// 注意：内核用 id_table 做匹配，
//       所以 probe/remove 不需要再自己判断 vendor/device
```

### 6.4 probe() 函数详解

```c
static int day22_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    // 参数：
    //   pdev - 枚举到的 PCI 设备
    //   id   - 匹配的 pci_device_id 条目

    // ① 分配私有数据结构（devm_* 版本，失败自动释放）
    struct day22_stub_dev *sdev;
    sdev = devm_kzalloc(&pdev->dev, sizeof(*sdev), GFP_KERNEL);
    if (!sdev)
        return -ENOMEM;

    // ② 把私有数据存到 pdev 里（之后通过 pci_get_drvdata 取回）
    pci_set_drvdata(pdev, sdev);

    // ③ 打印 BAR 信息（只读，不做映射）
    day22_dump_resources(pdev);

    return 0;  // 返回 0 表示成功
}
```

### 6.5 remove() 函数详解

```c
static void day22_remove(struct pci_dev *pdev)
{
    // 由于使用的是 devm_kzalloc()，内核会自动释放内存
    // 不需要手动 free

    // 注意：如果 day23 做了 pci_request_regions()，
    //       remove 需要调用 pci_release_regions()
}
```

### 6.6 day22_dump_resources()

```c
// 读取并打印 BAR 信息
static void day22_dump_resources(struct pci_dev *pdev)
{
    int bar;

    for (bar = 0; bar < PCI_STD_NUM_BARS; bar++) {
        resource_size_t start = pci_resource_start(pdev, bar);
        resource_size_t end = pci_resource_end(pdev, bar);
        resource_size_t len = pci_resource_len(pdev, bar);
        unsigned long flags = pci_resource_flags(pdev, bar);

        if (!len)
            continue;

        dev_info(&pdev->dev,
                 "BAR%d: start=%pa end=%pa len=%pa flags=0x%lx\n",
                 bar, &start, &end, &len, flags);
    }
}

// pci_resource_* 是 Linux 内核标准 API：
//   pci_resource_start()  → 获取 BAR 起始地址
//   pci_resource_end()    → 获取 BAR 结束地址
//   pci_resource_len()    → 获取 BAR 长度
//   pci_resource_flags()  → 获取 BAR 属性（Memory/IO 等）
```

---

## 七、devm_* 机制

### 7.1 什么是 devm_*？

```
devm_* = "device managed" 资源

特点：
  - 与普通资源的区别：不需要手动释放
  - 失败时自动释放：devm_kzalloc() 失败不需要 cleanup
  - remove 时自动释放：设备拔出时内核自动 free

示例：
  普通：kzalloc() → kfree()  // 必须配对
  devm：devm_kzalloc() → 自动  // 不需要手动 free
```

### 7.2 为什么 day22 用 devm_kzalloc？

```c
sdev = devm_kzalloc(&pdev->dev, sizeof(*sdev), GFP_KERNEL);
if (!sdev)
    return -ENOMEM;
// 如果后续 probe 失败返回，
// 内核会自动释放这块内存
// 不需要我们在 err_free_sdev 标签里手动 kfree
```

---

## 八、day22 的局限性

### 8.1 今天不做的事

```c
// day22 只做打印，不做以下操作：

// ❌ 不使能 PCI 设备
pci_enable_device(pdev);

// ❌ 不请求 BAR 资源
pci_request_regions(pdev, DRV_NAME);

// ❌ 不映射 BAR 到虚拟地址
pci_iomap(pdev, 0, bar0_len);

// ❌ 不分配 MSI 中断向量
pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);

// ❌ 不注册中断处理
request_irq(irq_vector, handler, 0, DRV_NAME, sdev);

// 这些都留到 day23/day25 逐步补上
```

### 8.2 资源申请对照表

```
day22（今天）：
  devm_kzalloc        ✅ 已做
  pci_set_drvdata     ✅ 已做
  day22_dump_resources ✅ 已做

day23：
  pci_enable_device
  pci_request_regions
  pci_iomap
  pci_set_master

day25：
  pci_alloc_irq_vectors
  request_irq
```

---

## 九、W4 后续预告

### 9.1 day23：资源接管

```
day23 会在 day22 基础上做：

1. pci_enable_device()
   → 使能 PCI 设备（打开设备的 config space 访问）

2. pci_request_regions()
   → 请求 BAR 资源独占访问

3. pci_iomap()
   → 将 BAR 映射到内核虚拟地址

4. 验证：对 BAR 读写
```

### 9.2 day24-25：寄存器与中断

```
day24：
  - ivshmem 寄存器布局
  - 读写共享内存

day25：
  - MSI 中断机制
  - pci_alloc_irq_vectors + request_irq
```

---

## 十、面试要会讲的五句话

1. **"PCI 枚举是 Linux PCI core 在系统启动时扫描 PCI bus、读取 vendor/device ID、分配地址资源的过程"**
   → 理解 PCI 系统的起点

2. **"BAR 是 PCI 设备用来声明自己需要哪些地址范围的寄存器，系统分配地址后写入 BAR"**
   → 理解 PCI 地址分配机制

3. **"pci_driver 的 id_table 告诉内核匹配哪些设备，probe 在匹配成功后被调用"**
   → 理解 PCI 驱动框架

4. **"devm_kzalloc 是 device managed 版本，失败时自动释放，不需要手动 free"**
   → 理解 devres 机制

5. **"day22 只做设备发现和骨架验证，day23 才开始真正的资源操作"**
   → 理解 W4 的渐进式学习设计

---

## 十一、验收标准

### 11.1 必须满足

- `lspci -nn` 输出中有 `1af4:1110`
- `lspci -vv -nn` 输出中有 BAR0 (256 bytes) 和 BAR2 (4M)
- `dmesg` 中有 `probe enter: vendor=1af4 device=1110`
- 驱动模块 `day22_ivshmem_stub` 已加载

### 11.2 关键证据

```
records/<RUN_ID>/lspci-nn.txt：
  应包含 1af4:1110

records/<RUN_ID>/lspci-vv-nn.txt：
  应包含 Region 0: Memory at <addr> [size=256]
  应包含 Region 2: Memory at <addr> [size=4M]

records/<RUN_ID>/dmesg-pci.txt：
  应包含 probe enter: vendor=1af4 device=1110
```

---

## 附录：probe 的对称性原则

```
probe 中申请的所有资源，必须在 remove 中按逆序释放：

probe 顺序：                          remove 顺序：
1. devm_kzalloc (私有数据)         1. (devm 自动释放)
2. pci_enable_device                 2. pci_disable_device
3. pci_request_regions               3. pci_release_regions
4. pci_iomap (BAR 映射)             4. pci_iounmap
5. pci_alloc_irq_vectors            5. pci_free_irq_vectors
6. request_irq (中断处理)             6. free_irq

day22 只做了第 1 步，所以 remove 几乎为空。
day23 会做 2-4 步，remove 需要补上对应的释放。
day25 会做 5-6 步，remove 需要补上对应的释放。
```
