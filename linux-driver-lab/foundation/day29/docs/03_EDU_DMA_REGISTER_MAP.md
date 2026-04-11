# QEMU EDU DMA 寄存器与 Day29 关注点

## 1. Day29 只关心哪几个寄存器

在 EDU 上，Day29 只需要关注下面这一组：

- `0x80`：DMA source address
- `0x88`：DMA destination address
- `0x90`：DMA transfer count
- `0x98`：DMA command register

再加上中断相关：

- `0x24`：IRQ status
- `0x64`：IRQ ack

以及设备内部 DMA buffer 的偏移：

- `0x40000`

---

## 2. Day29 里这些地址分别意味着什么

### 2.1 `0x80` / `0x88`

这两个寄存器不是“内核虚拟地址寄存器”。

它们需要的是 **设备可见的 DMA 地址**。

所以在 Day29 驱动里，应该写进去的是：

- `dma_handle + src_offset`
- `dma_handle + dst_offset`

而不是：

- `dma_virt + src_offset`
- `dma_virt + dst_offset`

### 2.2 `0x40000`

这不是 BAR0 的 CPU 访问虚拟地址，
而是 EDU 设备内部 4KB buffer 的设备侧偏移地址。

Day29 的 round-trip 验证就是：

1. RAM -> EDU `0x40000`
2. EDU `0x40000` -> RAM

---

## 3. DMA command register 怎么看

当前 Day29 只用三个位：

- bit0：start
- bit1：direction
  - `0`：RAM -> EDU
  - `1`：EDU -> RAM
- bit2：raise interrupt after DMA finished

所以：

### 3.1 第一次拷贝（RAM -> EDU）

```text
cmd = start | irq
```

### 3.2 第二次拷贝（EDU -> RAM）

```text
cmd = start | direction | irq
```

---

## 4. Day29 为什么采用“往返两次”

如果只做一次 RAM -> EDU，驱动很难在 guest 里直接验证“设备内部到底拿到了什么”。

但如果再做一次 EDU -> RAM，就可以把结果搬回同一块 coherent buffer 的另一段，
然后直接在驱动里比较：

- `src[0:len]`
- `dst[0:len]`

这就是 Day29 当前最小但最靠谱的验证方式。

---

## 5. 当前驱动的布局设计

当前 day29 驱动把 4KB coherent buffer 分成两半：

- `src_offset = 0`
- `dst_offset = 2048`

所以：

- 最大 verify 长度 = 2048

这么设计的目的很简单：

- 保证源和目标都在同一块 coherent buffer 内
- 避免再申请第二块 buffer
- 数据流更容易讲清楚

---

## 6. Day29 里中断的角色

Day29 不是以“中断实验”为主，所以 DMA 完成并不靠复杂中断同步。

当前设计里：

- DMA command 设置了 IRQ 位
- 驱动仍然会在 IRQ handler 里 ACK
- 但 DMA 是否完成，主判断仍以 command bit0 清零为准

这样做的好处是：

- 验证逻辑更稳定
- 仍然能复用 day25/day27 已经学过的 IRQ 路径
- records 里还能看到 DMA 相关 IRQ 迹象
