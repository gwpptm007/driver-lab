# Day29 DMA 数据流拆解

## 1. 从用户态看到的流程

用户态工具执行：

```bash
/bin/day29_edu_dma_tool /dev/day29_edu0 verify 256 0x41
```

表面上只是一次 `verify`，但内核里实际做了两次 DMA。

---

## 2. 驱动里的主路径

### 第一步：校验参数

驱动先检查：

- `len > 0`
- `len <= 2048`
- coherent buffer 已存在

### 第二步：准备源数据

驱动在 coherent buffer 的 `src` 区填充一个固定模式：

```text
byte[i] = (seed + i) & 0xff
```

然后把 `dst` 区清零。

### 第三步：第一次 DMA（RAM -> EDU）

驱动编程：

- DMA source = `dma_handle + src_offset`
- DMA destination = `0x40000`
- count = `len`
- cmd = `start | irq`

这一步把 guest RAM 的数据搬到 EDU 内部 buffer。

### 第四步：第二次 DMA（EDU -> RAM）

驱动再编程：

- DMA source = `0x40000`
- DMA destination = `dma_handle + dst_offset`
- count = `len`
- cmd = `start | direction | irq`

这一步把设备内部 buffer 再搬回 guest RAM 的 `dst` 区。

### 第五步：内核里做比较

最后驱动比较：

- `src[0:len]`
- `dst[0:len]`

如果逐字节一致，说明整个 round-trip 成功。

---

## 3. 为什么 Day29 要在内核里比较

因为今天的目标是学习 DMA API 和设备编程，不是学习用户态数据通道。

把比较动作放在内核里有三个好处：

1. 逻辑最短
2. 最容易定位问题
3. Day30 做 `mmap` 时，刚好可以把“比较逻辑逐步往用户态移”

所以 Day29 是有意先把验证收在内核里的。

---

## 4. 失败时最可能卡在哪

### 4.1 一次 DMA 都没发出去

常见现象：

- command bit 一直忙
- 直接 timeout

优先怀疑：

- source/destination 填错
- 长度非法
- mask / dma address 处理不对

### 4.2 第一次成功，第二次失败

常见现象：

- RAM -> EDU 正常
- EDU -> RAM 后比较不一致

优先怀疑：

- direction bit 设反了
- destination address 写错了
- 没清楚 `dst` 区，导致旧数据干扰判断

### 4.3 两次 DMA 都结束了，但比较失败

优先怀疑：

- 比较区间错了
- `src_offset` / `dst_offset` 重叠
- 写寄存器时把 `dma_virt` 当成了 DMA 地址

---

## 5. Day29 做完后，你应该能复述的版本

> 我在 QEMU EDU 上通过 `dma_set_mask_and_coherent()` 设置 28-bit coherent DMA mask，
> 用 `dma_alloc_coherent()` 申请 4KB DMA buffer，
> 再把这块 buffer 的 `dma_handle` 分别作为 DMA source/destination 编程给设备，
> 通过一次 RAM->EDU 和一次 EDU->RAM 的 round-trip，把结果搬回同一块 coherent buffer 的另一段，
> 最后在驱动里逐字节比较，验证 DMA 数据一致性。
