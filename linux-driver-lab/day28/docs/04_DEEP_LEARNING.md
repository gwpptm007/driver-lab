# Day28 深度学习指南 - W4 完整收口

## 一、Day28 是什么？

Day28 是 W4（Week 4）的**收口总结日**，不是新实验日。

**核心任务**：把 day22~day27 的真实实验结果整理成可交付、可复现、可复核的阶段性文档。

| 类型 | 内容 |
|------|------|
| 新实验 | 无（day22~day27 已完成） |
| 新驱动 | 无 |
| 新文档 | W4 总结、证据索引、复现指南 |
| 新脚本 | 证据收集脚本、摘要生成脚本 |

---

## 二、W4 学习链全景图

### 2.1 六天实验的递进关系

```
W4: PCIe 基础知识 (day22 → day27)
═══════════════════════════════════════════════════════════════

day22                    day23                    day24
┌──────────────┐        ┌──────────────┐        ┌──────────────┐
│ PCI 总线枚举  │   →    │ pci_driver  │   →    │ MMIO 读写    │
│ 设备可见性    │        │ 骨架驱动     │        │ BAR0/BAR2    │
└──────────────┘        └──────────────┘        └──────────────┘
  目标:                    目标:                    目标:
  QEMU PCI 设备            驱动接住设备            BAR0 寄存器
  能被 lspci 看到          probe/remove           BAR2 共享内存
                            资源分配/释放          协议读写

                          day25                    day26
                         ┌──────────────┐        ┌──────────────┐
                    →    │ MSI 中断     │   →    │ 用户态工具   │
                          │ 触发与计数   │        │ 接口友好     │
                          └──────────────┘        └──────────────┘
                            目标:                    目标:
                            EDU 设备                ioctl/read/write
                            MSI 中断触发            清晰错误码
                            irq_count 增长          CLI 工具

                                                      day27
                                                     ┌──────────────┐
                                                →    │ 循环稳定性   │
                                                      │ 200 次循环   │
                                                      └──────────────┘
                                                        目标:
                                                        insmod/rmmod
                                                        200 次稳定
                                                        无 oops/panic

═══════════════════════════════════════════════════════════════
  W4 完整学习链: 设备可见 → 驱动接住 → MMIO → MSI → 工具 → 稳定性
```

### 2.2 每天的核心验证点

| day | 核心验证 | 关键证据 |
|-----|----------|----------|
| day22 | 设备可见 | `lspci -nn` 显示 `1af4:1110` |
| day23 | probe/remove | `dmesg` 显示 `probe success` |
| day24 | MMIO 读写 | `mmio-read-after.txt` 有数据 |
| day25 | MSI 中断 | `irq_count` 增长 |
| day26 | 工具接口 | `ioctl/read/write` 都工作 |
| day27 | 循环稳定 | `loop=200, pass=200, fail=0` |

---

## 三、W4 六天实验详解

### 3.1 day22：PCI 总线与 ivshmem 设备枚举

**目标**：确认 QEMU PCI 设备能被操作系统识别

**做了什么**：
- QEMU 启动时模拟 ivshmem 设备（1af4:1110）
- Guest Linux PCI 总线枚举时发现设备
- `lspci -vv` 能看到设备详细信息

**关键证据**：
```
serial.log 中出现:
  pci 0000:00:02.0: [1af4:1110]
  ===DAY22:LSPCI_VV_NN:BEGIN===
  ===DAY22:DMESG_PCI:BEGIN===
  ===DAY22:COMPLETE===
```

**注意**：day22 的 `run-summary.md` 旧版有误判（写成"否"），但 `serial.log` 原始数据是正确的。

---

### 3.2 day23：pci_driver 骨架驱动

**目标**：写出能接住 PCI 设备的驱动骨架

**做了什么**：
- 定义 `pci_driver` 结构体
- 实现 `probe()` 和 `remove()` 函数
- 分配/释放 BAR0 和 BAR2 资源
- 打印设备信息

**关键验证**：
- `insmod` 成功
- `probe` 打印 BAR0/BAR2 信息
- `rmmod` 成功（remove 对称性）

**关键证据**：
```
dmesg-probe.txt:
  probe enter: 1af4:1110
  BAR0: start=0x... len=0x...
  BAR2: start=0x... len=0x...
  probe success
```

---

### 3.3 day24：MMIO 与共享内存读写

**目标**：打通 BAR0 寄存器读写、BAR2 共享内存读写协议

**做了什么**：
- `probe` 中映射 BAR0 和 BAR2
- 实现 `read()` 从 BAR2 读取数据
- 实现 `write()` 向 BAR2 写入协议头+数据
- 添加 LIVENESS 验证（写入测试值读回取反）

**BAR2 共享内存布局**：
```
BAR2 (4MB QEMU 创建的共享内存)
├── 协议头 (前 64 字节)
│   ├── magic:     0x冠名
│   ├── version:   版本号
│   ├── seq:       序列号
│   └── ...
└── 数据 payload (剩余空间)
```

**关键证据**：
```
mmio-info.txt:        BAR0/BAR2 地址信息
mmio-write.txt:       写入前状态
mmio-read-after.txt:  读取到写入的数据
shm-write.txt:        共享内存写入
shm-read.txt:         共享内存读取
```

---

### 3.4 day25：MSI 中断触发

**目标**：完成 EDU 设备 MSI 中断闭环

**做了什么**：
- 沿用 EDU 设备（1234:11e8）
- `pci_alloc_irq_vectors` 分配 MSI 向量
- `request_irq` 注册中断处理函数
- `write()` 触发 IRQ_RAISE 寄存器
- 中断处理函数读 IRQ_STATUS、写 IRQ_ACK

**MSI 中断流程**：
```
用户 write("1")
    ↓
驱动 writel(1, BAR0 + IRQ_RAISE)
    ↓
EDU 硬件发送 MSI（写特殊内存地址）
    ↓
CPU 接收中断，调用 irq_handler
    ↓
handler 读 IRQ_STATUS、计数+1、写 IRQ_ACK
```

**关键证据**：
```
irq-count-before.txt → irq-count-after.txt:  0 → 1
proc-interrupts-before.txt → after.txt:      0 → 1
dmesg:  irq handler: irq=XX status=0x... count=1
```

---

### 3.5 day26：用户态工具接口

**目标**：将驱动接口打磨成用户态友好工具

**做了什么**：
- `read()` 返回可读文本状态
- `write()` 接受整数（十进制/十六进制）触发中断
- `ioctl()` 返回结构化数据（GET_INFO、GET_IRQ_COUNT、GET_IRQ_STATUS、RESET_STATS）
- 完整 CLI 工具 `day26_edu_tool`

**接口对比**：
| 操作 | Day25 | Day26 |
|------|-------|-------|
| 触发中断 | ioctl(TRIGGER_IRQ) | write("1") |
| 获取计数 | ioctl(GET_IRQ_COUNT) | ioctl 或 read |
| 获取状态 | ioctl(GET_IRQ_STATUS) | read() 文本 |

**关键证据**：
```
info-before.txt / info-after.txt:     结构化信息
read-state-before.txt / after.txt:    文本状态
trigger.txt:                         "triggered value=0x00000001"
invalid-trigger-zero.txt:            rc=5 (错误码)
```

---

### 3.6 day27：200 次循环稳定性

**目标**：验证驱动在重复 insmod/rmmod 下稳定

**做了什么**：
- 最小化 probe（去掉 LIVENESS 验证）
- 严格对称 remove（8 步逐项核对）
- MSI/LEGACY 回退设计
- 200 次循环 smoke 测试

**remove 对称性检查表**：
```
probe:                     remove:
kzalloc              →     kfree
pci_enable_device    →     pci_disable_device
pci_request_regions   →     pci_release_regions
pci_iomap            →     pci_iounmap
pci_alloc_irq_vectors →     pci_free_irq_vectors
request_irq          →     free_irq
setup_chrdev        →     destroy_chrdev
```

**关键证据**：
```
loop-summary.txt:
  loop_count=200
  pass=200
  fail=0

dmesg (200 次循环):
  probe enter / probe success  (200 次)
  irq handler: count=1..200    (200 次)
  remove enter / remove leave  (200 次)
```

---

## 四、W4 证据收集模式

### 4.1 标准目录结构

```
dayXX/
├── driver/                 # 驱动源码
│   ├── demo.c
│   └── include/
├── tools/                  # 用户态工具
│   └── demo_tool.c
├── guest/                  # QEMU guest 脚本
│   └── init.dayXX
├── records/               # 原始证据
│   └── dayXX-local-XXX/
│       ├── dmesg-*.txt    # 内核日志
│       ├── lspci-*.txt    # PCI 信息
│       ├── serial.log     # 串口输出
│       └── run-summary.md # 运行总结
└── docs/                  # 文档
```

### 4.2 证据类型说明

| 证据类型 | 用途 | 示例 |
|----------|------|------|
| `dmesg-*.txt` | 内核日志 | `probe success`, `irq handler` |
| `lspci-nn.txt` | PCI 设备列表 | `1234:11e8` |
| `lspci-vv-nn.txt` | PCI 设备详情 | BAR 地址、中断信息 |
| `serial.log` | 串口完整输出 | 自动化流程日志 |
| `proc-interrupts-*.txt` | 中断统计 | irq_count 变化 |
| `run-summary.md` | 自动化判断 | pass/fail |

### 4.3 证据自动收集脚本

```bash
# day28 提供的自动化脚本
cd day28
bash scripts/01_collect_w4_evidence.sh    # 扫描 day22~27 的 records
python3 scripts/02_generate_w4_summary.py  # 生成 W4 总结
```

---

## 五、W4 验收标准

### 5.1 各天验收条件

| day | 验收条件 |
|-----|----------|
| day22 | `lspci -nn` 能看到设备 ID，`serial.log` 有 COMPLETE marker |
| day23 | `insmod` 成功，`probe` 打印 BAR 信息，`rmmod` 成功 |
| day24 | `mmio-read-after.txt` 能读到写入的数据，`shm-read.txt` 有数据 |
| day25 | `irq_count` 从 0 增长到 1，`proc/interrupts` 有记录 |
| day26 | `ioctl info`、`read-state`、`trigger` 都返回正确结果 |
| day27 | `loop=200, pass=200, fail=0`，无 `oops/panic/hung` |

### 5.2 已知限制

| 限制 | 说明 |
|------|------|
| QEMU 虚拟设备 | 主要在 QEMU 环境中验证，非真实硬件 |
| 无真实 DMA | W4 只到 MSI 中断，还没到 DMA |
| 无 perf/ftrace | W5 才会涉及性能分析工具 |
| 无 mmap | W5 才会涉及内存映射 |

---

## 六、W4 → W5 过渡

### 6.1 W5 将要学习的内容

根据 CLAUDE.md，W5 (day29~35) 的主题是 **DMA + 性能分析**：

```
W5: DMA 与性能分析 (day29 → day35)
═══════════════════════════════════════════════════════════════

主要内容:
  - QEMU EDU 设备 DMA 能力
  - dma_alloc_coherent (DMA 缓冲区分配)
  - mmap (内存映射，实现零拷贝)
  - benchmarking (吞吐/延迟测试)
  - perf (性能分析)
  - ftrace function_graph (调用路径分析)
  - 稳定性测试

W4 已具备的基础:
  ✓ PCIe 设备枚举
  ✓ BAR/MMIO 读写
  ✓ MSI 中断机制
  ✓ 字符设备接口
  ✓ 循环稳定性
  ✗ DMA 传输
  ✗ mmap 零拷贝
  ✗ 性能基准测试
```

### 6.2 W4 学到的技能对 W5 的帮助

| W4 技能 | W5 如何应用 |
|---------|-------------|
| PCI probe/remove | DMA 设备的 probe 更复杂，但模式相同 |
| BAR MMIO 映射 | DMA 描述符寄存器映射 |
| MSI 中断 | DMA 完成中断（比 MSI 更复杂） |
| 字符设备接口 | DMA buffer 的 mmap 需要 file_operations |
| 循环稳定性测试 | DMA 传输稳定性测试 |
| 证据收集模式 | DMA 性能数据收集 |

---

## 七、W4 关键概念总结

### 7.1 PCIe 核心概念

| 概念 | 说明 |
|------|------|
| PCI 枚举 | BIOS/内核扫描 PCI 总线，发现设备 |
| BAR | Base Address Register，设备寄存器/内存地址窗口 |
| MMIO | Memory-Mapped I/O，寄存器通过内存访问 |
| MSI | Message Signaled Interrupt，设备写特殊地址触发中断 |
| probe/remove | 驱动加载/卸载时分配/释放资源 |

### 7.2 驱动模型概念

| 概念 | 说明 |
|------|------|
| pci_driver | PCI 设备驱动结构体 |
| struct pci_dev | PCI 设备实例 |
| pci_iomap | 将 BAR 映射到内核虚拟地址 |
| request_irq | 注册中断处理函数 |
| cdev | 字符设备结构体 |

### 7.3 测试验证概念

| 概念 | 说明 |
|------|------|
| smoke test | 最小测试，验证基本功能 |
| 循环测试 | 重复执行，暴露长期稳定性问题 |
| 证据归档 | 保留原始日志，供复核 |
| 对称性 | 分配/释放必须严格对称 |

---

## 八、day28 文档索引

| 文档 | 内容 |
|------|------|
| `docs/01_plan.md` | day28 实施计划 |
| `docs/01_LOCAL_RUNBOOK.md` | 本地复现手册 |
| `docs/02_W4_RESULTS_AND_ACCEPTANCE.md` | W4 结果与验收标准 |
| `docs/03_EVIDENCE_GUIDE.md` | 证据索引使用指南 |
| `output/day28_w4_summary.md` | W4 阶段总结 |
| `output/day28_evidence_index.md` | 证据文件索引 |
| `scripts/01_collect_w4_evidence.sh` | 证据收集脚本 |
| `scripts/02_generate_w4_summary.py` | 摘要生成脚本 |
