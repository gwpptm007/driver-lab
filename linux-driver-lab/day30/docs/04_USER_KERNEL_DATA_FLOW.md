# day30 用户态 / 内核态数据流

## 1. 总体流向

```text
guest user tool
    |
    | open("/dev/day30_edu0")
    | mmap(fd, 4096, MAP_SHARED, offset=0)
    v
DMA coherent buffer (single page)
    |-- src_off : user fills pattern
    |-- dst_off : user clears to zero
    |
    | ioctl(RUN_MMAP_DMA, len, seed)
    v
kernel day30 driver
    |
    | stage1: RAM(src_off) -> EDU internal buffer
    | stage2: EDU internal buffer -> RAM(dst_off)
    | irq handler updates irq_count/status
    v
guest user tool
    |
    | compare src_off and dst_off directly
    v
verify_ok / mismatch_index / mismatch bytes
```

---

## 2. 用户态负责什么

- 打开设备
- 建立 `mmap`
- 在映射区写 pattern
- 清空 dst 区
- 触发 ioctl
- 直接在映射区做比对

Day30 的“零拷贝”就是指：
**用户态在同一块 DMA buffer 上完成写入和读取，中间不再要求内核 copy 一份出来给它。**

---

## 3. 内核态负责什么

- 管理 PCI 设备与 BAR0
- 管理 coherent DMA buffer
- 暴露 `mmap`
- 进行长度/offset 边界校验
- 编程 EDU DMA 寄存器
- 处理中断
- 记录状态供 records 留证

---

## 4. 为什么用户态比对更能体现 day30 的主题

如果 day30 仍然让内核做最终比对，那么从观感上它和 day29 差别就不大。  
而把 compare 放到用户态后，实验意义会更明确：

- 说明用户态确实直接读到了 DMA 回写结果
- 说明这块映射内存不是“只能写不能读”的假接口
- 说明零拷贝可见链路成立

---

## 5. `run_ok` 和 `verify_ok` 的区别

这是 day30 特别容易混淆的地方。

### `run_ok`
表示：
- 驱动这两段 DMA 已成功完成
- IRQ 和寄存器路径没有报错

### `verify_ok`
表示：
- 用户态在映射区比对 `src == dst` 成功

因此：
- `run_ok=1` 并不自动等于 `verify_ok=1`
- 只有两者都成立，day30 才算真的通过
