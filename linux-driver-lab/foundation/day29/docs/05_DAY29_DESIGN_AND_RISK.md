# Day29 设计取舍与风险

## 1. 为什么 Day29 用 coherent，不先用 streaming DMA

因为 Day29 的目标是先把“DMA 地址是谁、CPU 地址是谁、设备怎么搬运数据”这件事学清楚。

如果一开始就上 streaming DMA，学习成本会一下子多出：

- `dma_map_single`
- `dma_unmap_single`
- cache 同步方向
- 生命周期约束

对于 Day29 来说会过重。

所以今天故意选：

- `dma_alloc_coherent`
- 单 buffer
- 往返 round-trip
- 驱动内比较

这条线最适合先把基础打牢。

---

## 2. 为什么 DMA mask 固定按 28-bit 讲

因为 EDU 的教学设备默认就是按这个口径暴露 DMA 能力。

今天最重要的不是“把 mask 调得多大”，而是你要真正理解：

- 设备支持的 DMA 地址范围有限
- 驱动需要显式告诉 DMA API：这个设备能接受多大的地址范围

所以 Day29 直接把这件事摊开做，是合理的。

---

## 3. 为什么不让用户态直接参与 buffer 比较

因为那是 Day30 的事。

Day29 还在做：

- 驱动内数据正确性
- 设备 DMA 基本语义
- 失败路径可解释

一旦把用户态映射也揉进来，问题边界会变宽。

所以 Day29 当前边界很明确：

> **只在内核内完成 DMA 一致性验证。**

---

## 4. 当前设计的主要风险

### 4.1 设备地址和 CPU 地址混淆

这是最危险、也最典型的错误。

### 4.2 把 4KB buffer 当成无限大

当前实现只把它分成两半，最大 verify 长度是 2048。

### 4.3 过度依赖中断作为完成判定

Day29 里 IRQ 只是辅助观察，真正完成判定看 command busy 位是否清零。

### 4.4 记录不完整

如果只保留最终一句 `verify ok`，后面 day30/day31 很难复盘。

所以 Day29 必须至少留下：

- tool info
- verify result
- dmesg
- run summary

---

## 5. Day29 对 Day30 的直接输入

Day30 做 `mmap` 之前，至少要确认 Day29 已经明确了：

- coherent buffer 的大小
- `src_offset` / `dst_offset`
- 用户态最需要看到的是哪一段
- 驱动当前如何做越界检查

这些内容都是 Day30 设计 `mmap` 边界时会直接复用的。
