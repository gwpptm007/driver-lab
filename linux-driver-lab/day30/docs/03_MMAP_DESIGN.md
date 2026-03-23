# day30 mmap 设计说明

## 1. 为什么选 coherent DMA buffer

Day29 已经证明了：

- `dma_alloc_coherent()` 能在当前 arm64 + QEMU EDU 环境下稳定工作
- EDU DMA 两段 round-trip 可以完成
- coherent 路线的 bring-up 风险已经被 day29 吃过一遍

因此 day30 最自然的设计就是：

- **继续用 coherent DMA buffer**
- 不重新引入别的内存分配路径
- 把注意力集中在 `mmap` 与用户态直接访问上

---

## 2. DMA buffer 布局

day30 继续沿用 day29 的 4KB 单页布局：

- `dma_bytes = 4096`
- `src_off = 0`
- `dst_off = 2048`
- `max_verify_len = 2048`

这样做的好处：

- 一页映射最简单
- src/dst 不重叠
- 结构和 day29 对照很清晰
- 用户态只需要一个整页 `mmap()`

---

## 3. mmap 边界策略

当前 day30 故意采取“最严格、最保守”的策略：

### 只允许 `offset == 0`
原因：
- Day30 重点不是 VMA 切片
- 先把整页映射主链路做稳

### 只允许 `length == PAGE_ALIGN(dma_bytes)`
原因：
- coherent DMA buffer 当前就是一页
- 不允许半页或其他任意长度映射，能显著降低边界复杂度

### 用 `dma_mmap_coherent()`
原因：
- 分配方式和映射方式是一套
- 对当前实验目标最自然
- 最容易解释和复盘

---

## 4. ioctl 设计

Day30 不再沿用 day29 的“内核填充 + 内核比对”模式，而是分成两层：

### 驱动层只负责
- 提供基础信息
- 发起两段 DMA
- 记录 IRQ / cmd / error / mmap 状态

### 用户态工具负责
- `mmap()`
- 写 pattern
- 清 dst
- DMA 后比较 src/dst

这就是 day30 的职责切分。

---

## 5. 为什么还保留字符设备 read() 与 info/result ioctl

虽然 day30 的主角是 `mmap()`，但依然保留：

- `cat /dev/day30_edu0`
- `GET_INFO`
- `GET_RESULT`

原因：

1. guest init 里直接 `cat` 能留下可读状态快照
2. records 提取更容易自动化
3. 后续 day31 仍然能复用这套交互面

---

## 6. 这版 day30 有意不做的事

- 不支持 partial mapping
- 不支持 offset mapping
- 不支持多个进程同时映射
- 不支持复杂同步原语

因为这一天的教学目标不是“做成一个完整共享内存框架”，而是**把零拷贝 mmap 主链路做得可解释、可验证、可复盘**。
