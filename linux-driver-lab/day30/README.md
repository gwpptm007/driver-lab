# day30：mmap 零拷贝访问 coherent DMA buffer

## 1. 今日定位

- 周期：W5
- 前置基线：day29 已经完成 **QEMU EDU coherent DMA round-trip**
- 今日主题：把 day29 内核独占的 DMA buffer 暴露给用户态，通过 `mmap()` 做一次真正的零拷贝可见实验

Day29 的重点是“DMA 通不通”；  
Day30 的重点是“**用户态是否真的直接看到了 DMA buffer，并能在不额外 copy 的前提下完成读写验证**”。

---

## 2. 这一天为什么值得单独做

day29 里虽然已经有 coherent DMA buffer，但 buffer 的拥有者仍然是内核：

- 内核填 pattern
- 内核发起 DMA
- 内核比对 src/dst
- 用户态只是发 ioctl 和看结果

到了 day30，学习重点要升级为：

- 内核仍负责 `dma_alloc_coherent()`、MMIO、IRQ 和 DMA 编程
- 用户态通过 `mmap()` 直接看到同一块 DMA buffer
- 用户态自己写 src / 清 dst / 读 dst / 做数据比对
- 内核从“主角”退到“DMA 发起者 + 边界守门员”

这一步走通之后，后面的“更真实的数据面共享”才有坚实基础。

---

## 3. day30 一句话目标

**把 coherent DMA buffer 映射给用户态，让用户态直接操作 src/dst 区域，再由内核只负责发起 EDU DMA 和处理中断，最终完成一次零拷贝 round-trip 验证。**

---

## 4. 基于当前代码，我建议的最小闭环

1. 驱动在 `probe()` 中继续完成：
   - `dma_set_mask_and_coherent()`
   - `dma_alloc_coherent()`
   - `pci_iomap()`
   - IRQ 注册
   - 字符设备注册

2. 新增字符设备 `mmap` 回调：
   - 只允许 `offset == 0`
   - 只允许 `length == PAGE_ALIGN(dma_bytes)`
   - 通过 `dma_mmap_coherent()` 暴露 DMA buffer

3. 用户态工具：
   - `mmap()` 整页 DMA buffer
   - 在 `src_off` 写入 pattern
   - 在 `dst_off` 清零
   - 通过 ioctl 触发两段 EDU DMA
   - 直接比对映射区里的 src/dst

4. guest 自动化：
   - 打印 `lspci`
   - 加载驱动
   - 打印 `tool info`
   - 做非法 `mmap` 长度与 offset 验证
   - 做一次 `mmap-verify`
   - 打印 driver result、device state、interrupts、dmesg
   - 自动关机并归档 records

---

## 5. 我对 day30 的实现边界建议

为了把主链路做稳，day30 暂时不追这些复杂项：

- 不做部分页映射
- 不做多段 offset 映射
- 不做多进程并发 mmap
- 不做 cache attribute 研究
- 不做复杂 scatter-gather

Day30 的成功标准不是“把 mmap 做到很泛化”，而是：

**用最小、最清晰、最容易解释的方式，把 DMA buffer -> 用户态映射 这条链路吃透。**

---

## 6. day30 当前目录里的核心文件

- `driver/day30_edu_mmap.c`：内核驱动主实现
- `include/day30_edu_uapi.h`：用户态/内核共享 ioctl 结构
- `tools/day30_edu_mmap_tool.c`：guest 用户态验证工具
- `guest/init.day30`：guest 自动化入口
- `scripts/`：宿主构建、QEMU 运行、records 归档
- `docs/`：任务分析、设计、风险和排障说明

---

## 7. day30 验收口径

### 必过项
- `mmap()` 成功
- 用户态直接写 src / 读 dst
- ioctl 触发的两段 DMA 都完成
- 用户态比对 `src == dst`
- 非法 `mmap` 长度被拒绝
- 非法 `mmap` offset 被拒绝
- guest 流程完整结束
- 无 panic / oops / DMA mapping error

### 当前记录方式
day30 的 records 重点不是只看驱动 dmesg，而是同时看：

- 用户态 `mmap-verify` 输出
- 驱动 `GET_RESULT` 输出
- `/dev/day30_edu0` 状态快照
- `proc/interrupts`
- `serial.log` / `qemu.stderr.log`

---

## 8. 和前后天的关系

- 前一天 day29：证明 coherent DMA round-trip 已成立
- 今天 day30：证明这块 buffer 能被用户态直接访问和验证
- 后一天 day31：再往更强的共享/同步模型推进时，就有了“mmap 零拷贝”的稳定基线

## 9. 当前这版 records 的结论

基于当前包内 `records/day30-local-001`：

- day30 主链路已经通过；
- `mmap-verify` 成功，`verify_ok=1`；
- DMA round-trip 成功，`run_ok=1`，`irq_delta=2`；
- `invalid offset` 失败路径已通过；
- `invalid length` 当前 records 使用的是旧样例 `2048`，会被页对齐扩成合法整页映射，因此这条记录应解读为“样例无效”，而不是“驱动未检查长度”。

更完整的分析见：`docs/07_TEST_RESULT_ANALYSIS.md`。
