# Day35 后学习路线规划 - Linux 驱动专家养成

## 一、当前基础评估

### 1.1 已掌握技能（day01-35）

```
✓ 字符设备驱动：open/read/write/ioctl/mmap
✓ 平台驱动模型：platform_driver、Device Tree、IRQ
✓ PCIe 驱动：BAR/MMIO、DMA、MSI 中断
✓ 高级 DMA：dma_alloc_coherent、dma_mmap_coherent
✓ 性能分析：perf stat/record、ftrace function_graph
✓ 稳定性验证：并发压测、模块循环、错误注入
✓ 内核裁剪：defconfig、rootfs、busybox
```

### 1.2 知识缺口

```
网络协议栈：sk_buff、netdev_ops、ndo_start_xmit
块设备框架：request queue、blk_mq、scatter-gather
USB 驱动：URB、endpoint、gadget/host
电源管理：runtime PM、suspend/resume
实时性：PREEMPT_RT、调度延迟
内核调试：eBPF、crash/kgdb、kprobes
```

---

## 二、学习路线总览

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Day35 后学习路线                                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  第1站：网络驱动（PCIe + DMA 知识复用，2-3周）                        │
│      ↓                                                               │
│  第2站：块设备/NVMe（PCIe 知识复用，2-3周）                          │
│      ↓                                                               │
│  第3站：USB 驱动（嵌入式必备，2-3周）                                │
│      ↓                                                               │
│  第4站：内核调试/eBPF（进阶工具，持续）                              │
│                                                                      │
│  可选支线：                                                          │
│      → 电源管理（RTOS 迁移、嵌入式）                                 │
│      → 实时 Linux（工业控制、机器人）                                │
│      → 声音驱动（ALSA，较小众）                                      │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 三、第1站：网络驱动

### 3.1 为什么首选网络驱动？

```
1. 知识复用率最高：
   - PCIe 设备枚举（与 EDU 相同）
   - DMA 操作（与 EDU 相同模式）
   - MSI 中断（与 EDU 相同）

2. 面试高频考点：
   - sk_buff 的生命周期
   - DMA 环形缓冲区（TX/RX queue）
   - NAPI 轮询 vs 中断模式

3. 资料丰富：
   - Linux 内核源码（drivers/net/）
   - 《Linux 高性能服务器编程》
   - 《Understanding Linux Network Internals》
```

### 3.2 学习目标

```
初级目标：
  - 理解 netdev_ops、ndo_start_xmit、ndo_poll_controller
  - 理解 sk_buff 的分配/释放/克隆/拷贝
  - 实现一个简单的 loopback 驱动

中級目标：
  - 理解 DMA 环形缓冲区（TX/RX descriptor ring）
  - 理解 NAPI 和 interrupt coalescing
  - 对比千兆网卡 vs EDU 的 DMA 复杂度

高级目标：
  - 实现一个 virtio-net 驱动
  - 理解 macvlan/veth pair
  - eBPF XDP 入门
```

### 3.3 核心知识点

```
网络设备驱动框架：
 alloc_etherdev()       → 分配 net_device
 register_netdev()      → 注册网络设备
 unregister_netdev()    → 注销

 关键 file_operations：
  ndo_start_xmit()       → 数据包发送（核心）
  ndo_poll_controller()  → NAPI 轮询
  ndo_set_rx_mode()      → 多播/混杂模式
  ndo_change_mtu()        → MTU 修改

 sk_buff 操作：
  alloc_skb() / dev_kfree_skb()
  skb_put() / skb_push() / skb_pull()
  skb_clone() / skb_copy()

 DMA 环形缓冲区：
  TX descriptor ring（发送描述符环）
  RX descriptor ring（接收描述符环）
  DMA_ADDR_BIT_MASK vs scatter-gather
```

### 3.4 推荐练习项目

```
项目1：loopback 驱动
  → 最小化网络设备，理解框架

项目2：虚拟 veth pair + macvlan
  → 理解容器网络虚拟化

项目3：Intel/AMD 千兆网卡驱动分析
  → 复现 EDU DMA 知识，看复杂驱动

项目4：virtio-net 驱动
  → 对比 e1000/virtio-net 异同
```

### 3.5 预估时间

```
Week 1：框架 + loopback
Week 2：sk_buff + NAPI
Week 3：DMA 环形缓冲区 + 真实网卡驱动分析
```

---

## 四、第2站：块设备 / NVMe 驱动

### 4.1 为什么学习块设备？

```
1. PCIe NVMe 与 EDU 高度相似：
   - BAR MMIO 访问
   - DMA 数据传输
   - MSI 中断

2. 理解 Linux 存储栈：
   VFS → ext4/XFS → block layer → NVMe driver

3. 面试常问：
   - 请求队列如何工作
   - blk_mq vs old elevator
   - scatter-gather DMA
```

### 4.2 学习目标

```
初级目标：
  - 理解 block_device_operations
  - 理解 request queue 机制
  - 实现一个 RAM disk 驱动

中級目标：
  - 理解 blk_mq（multi-queue block layer）
  - 理解 NVMe SQ/CQ/PRP/SGL
  - 分析内核 NVMe 驱动源码

高级目标：
  - 实现一个简单的 NVMe 驱动
  - 理解 PDT（ Purpose Descriptor Table）
```

### 4.3 核心知识点

```
块设备框架：
  register_blkdev()       → 注册块设备
  blk_init_queue()        → 初始化请求队列
  blk_mq_init_map()       → multi-queue

 请求处理：
  make_request_fn()       → 合并请求
  blk_fetch_request()     → 取出请求
  blk_end_request()       → 完成请求

 NVMe 寄存器：
  CAP（Controller Capabilities）
  VS（Version）
  CSTS（Controller Status）
  SQ0TDBL / CQ0HIBL（门铃寄存器）

 NVMe DMA 结构：
  PRP（Physical Region Page）
  SGL（Scatter Gather List）
  SQ（Submission Queue）
  CQ（Completion Queue）
```

### 4.4 推荐练习项目

```
项目1：RAM disk 驱动
  → 最小化块设备，理解框架

项目2：null_blk（内核内置的块设备）
  → 分析源码，理解 blk_mq

项目3：NVMe 驱动分析
  → 对比 EDU 的简单 DMA 和 NVMe 的复杂 DMA

项目4：软 RAID（mdadm）
  → 理解块设备层软件抽象
```

### 4.5 预估时间

```
Week 1：块设备框架 + RAM disk
Week 2：request queue + blk_mq
Week 3：NVMe 寄存器 + DMA + 驱动分析
```

---

## 五、第3站：USB 驱动

### 5.1 为什么学习 USB？

```
1. 嵌入式必备总线：
   - MCU 常用 USB Device（Gadget）
   - 外设常用 USB Host

2. USB vs PCIe：
   - USB 是主板南桥集成（非直连 CPU）
   - USB 有四种传输类型（bulk/interrupt/iso/control）

3. 知识增量：
   - URB（USB Request Block）
   - endpoint descriptor
   - gadget/function/compound device
```

### 5.2 学习目标

```
初级目标：
  - 理解 USB 传输类型
  - 理解 URB 生命周期
  - 实现一个 USB bulk 传输驱动

中級目标：
  - 理解 USB gadget framework
  - 实现一个 USB serial gadget
  - 理解 USB3.0 的 burst mode
```

### 5.3 核心知识点

```
USB 主机驱动：
  usb_register() / usb_deregister()
  usb_alloc_urb() / usb_submit_urb()
  usb_fill_bulk_urb() / usb_fill_int_urb()

 USB gadget 驱动：
  usb_gadget_register_driver()
  usb_ep_alloc_request() / usb_ep_queue()
  composite_driver

 端点类型：
  BULK（大数据传输，打印机、存储）
  INTERRUPT（小额实时数据，键盘、鼠标）
  ISOCHRONOUS（音视频，流式）
  CONTROL（命令/配置，枚举）
```

### 5.4 推荐练习项目

```
项目1：USB 键盘/鼠标分析
  → hiddev 接口，理解 USB HID

项目2：USB serial gadget
  → 实现一个 USB 转串口设备

项目3：USB mass storage gadget
  → 理解 SCSI + USB bulk 传输
```

### 5.5 预估时间

```
Week 1：USB 协议基础 + gadget 框架
Week 2：URB 生命周期 + 传输类型
Week 3：真实设备驱动分析（键盘、存储）
```

---

## 六、第4站：内核调试技术

### 6.1 为什么学习内核调试？

```
1. 解决实际问题的能力：
   - kernel panic、oops、BUG
   - 死锁、竞态条件

2. 进阶必备：
   - eBPF 是云原生/内核追踪标配
   - crash analysis 是生产环境排障必备
```

### 6.2 核心知识点

```
调试工具链：

  printk + dmesg
    → 最基础的调试手段
    → loglevel、pr_debug、dev_info

  kgdb
    → 内核级 GDB 调试
    → 需要串口或网络连接

  kprobes / ftrace
    → 动态追踪（已在 day33 学过）
    → 学习 kprobe 和 kretprobe

  crash / makedumpfile
    → vmcore 分析
    → 理解 struct task_struct、slub

  eBPF
    → BCC 工具集（biolatency、funccount）
    → libbpf + CO-RE
    → bpftrace 脚本

  KASAN / KMSAN / KTSAN
    → 内存错误检测
    → UAF、buffer overflow、data race
```

### 6.3 推荐练习项目

```
项目1：复盘 day33 的 ftrace 脚本
  → 用 kprobe 追踪自定义函数

项目2：使用 bpftrace 分析系统调用
  → trace sys_enter / sys_exit

项目3：使用 crash 分析 vmcore
  → 模拟一个 kernel panic 并分析

项目4：KASAN 检测内存错误
  → 在模块中人为引入 UAF
```

### 6.4 预估时间

```
Week 1：printk + kgdb + kprobes
Week 2：crash / makedumpfile
Week 3：eBPF / bpftrace
```

---

## 七、实战项目建议

### 7.1 项目难度分级

```
🌱 入门级（1-2周）：
  - loopback 网络驱动
  - RAM disk 块设备驱动
  - USB serial gadget

🌿 中级（2-4周）：
  - virtio-net 驱动
  - NVMe 驱动分析
  - USB mass storage gadget

🌳 高级（4-8周）：
  - 完整千兆网卡驱动
  - KASAN + 内核单元测试
  - eBPF 性能分析工具
```

### 7.2 推荐组合

```
组合A（网络方向）：
  loopback → virtio-net → eBPF 调优

组合B（存储方向）：
  RAM disk → NVMe 驱动 → KASAN

组合C（嵌入式方向）：
  USB gadget → PM runtime → RTLinux
```

---

## 八、资源推荐

### 8.1 书籍

```
网络驱动：
  - 《Linux 高性能服务器编程》
  - 《Understanding Linux Network Internals》

块设备：
  - 《Linux 存储系统》

USB：
  - 《Linux Device Drivers》3rd Edition（USB 章节）
  - 《USB in a Nutshell》

内核调试：
  - 《Linux 内核调试技术》
  - 《BPF Performance Tools》
```

### 8.2 在线资源

```
Linux 源码：
  drivers/net/ethernet/      # 网卡驱动
  drivers/block/             # 块设备驱动
  drivers/usb/              # USB 驱动
  kernel/trace/             # ftrace/eBPF

内核文档：
  Documentation/networking/
  Documentation/block/
  Documentation/usb/

工具文档：
  man 8 bpftrace
  kernel docs / trace / ftrace.txt
```

### 8.3 内核源码学习技巧

```
1. 从 LDD3 示例开始：
   - skull.c（字符设备）
   - short.c（并行端口）
   - usb-skeleton.c（USB 驱动）

2. 分析真实驱动：
   - e1000e（Intel 千兆网卡）
   - nvme-scsi.c（NVMe 驱动）
   - dwc2（USB Host）

3. 对比学习：
   - EDU vs e1000e 的 DMA
   - EDU vs nvme 的 PCIe 配置
```

---

## 九、学习路线总结

### 9.1 推荐顺序

```
第1步：网络驱动（复用 PCIe + DMA，2-3周）
  ↓
第2步：块设备/NVMe（复用 PCIe，2-3周）
  ↓
第3步：USB 驱动（补充主机侧视角，2-3周）
  ↓
第4站：内核调试（进阶工具，持续）
```

### 9.2 每日学习建议

```
工作日（2-3小时）：
  - 上午：读内核文档/书籍
  - 下午：写代码/分析驱动

周末（4-6小时）：
  - 实现一个完整的小项目
  - 总结学习笔记

里程碑检查：
  - 每周末检查是否完成当周目标
  - 每2-3周检查是否进入下一站
```

### 9.3 关键成功因素

```
1. 多读源码：
   - 内核文档是起点，源码是终点
   - 从 LDD3 到真实驱动

2. 多写代码：
   - 光看不写等于没学
   - 每个知识点都要有代码验证

3. 多排障：
   - kernel panic 是最好的老师
   - 学会用工具定位问题

4. 多总结：
   - 写学习笔记
   - 对比旧知识与新知识
```

---

## 十、快速参考

```
Q：只能选一个方向，选哪个？
A：网络驱动（复用率最高，面试最常问）

Q：需要多长时间？
A：每天2-3小时，约2-3个月完成第一站

Q：需要买开发板吗？
A：初期不需要（QEMU 模拟足够），网络/存储方向后期可以买

Q：如何验证学习效果？
A：能够独立回答以下问题：
   - sk_buff 的生命周期
   - NVMe 的 SQ/CQ/PRP 是什么
   - USB 的四种传输类型
   - 如何用 eBPF 分析 syscall
```

---

*本文档由 Claude Code 生成，基于 linux-driver-lab 项目 day01-35 的学习路径*
