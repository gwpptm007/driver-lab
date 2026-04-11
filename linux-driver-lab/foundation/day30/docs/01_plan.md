# day30 详细计划

## 1. 今日主题

**mmap 零拷贝访问 coherent DMA buffer**

这是在 day29 已经打通 QEMU EDU coherent DMA round-trip 之后，自然衔接出来的一天。  
day29 解决的是“DMA 能不能通”；day30 要解决的是“**用户态能不能直接使用同一块 DMA buffer**”。

---

## 2. 为什么 day30 要单独立项

如果只是把一个 `mmap` 回调顺手塞进 day29，看起来省事，但学习价值会被冲淡：

- day29 的重点是 `dma_alloc_coherent()`、DMA mask、DMA 寄存器编程、IRQ bring-up
- day30 的重点是字符设备 `mmap`、页对齐、映射边界、用户态/内核职责切分

把 day30 独立成一整天，后续复盘会很清晰：

- 哪一天解决了 DMA bring-up
- 哪一天解决了零拷贝映射
- 哪一天开始更复杂的共享与同步

---

## 3. 核心目标

把 day29 里的 coherent DMA buffer 映射给用户态，实现：

- 用户态 `mmap()` 成功
- 用户态直接写 `src_off`
- 用户态直接读 `dst_off`
- 内核仅通过 ioctl 发起 EDU DMA
- 用户态自己完成 `src` / `dst` 比对

这就是 day30 的最小闭环。

---

## 4. Day29 -> Day30 的关键边界变化

### Day29
- buffer 属于内核
- pattern 由内核写
- 比对由内核做
- 用户态只是“触发器”

### Day30
- buffer 仍由内核分配
- 但映射给用户态
- pattern 由用户态写
- 比对由用户态做
- 内核退化成“DMA 发起者 + 中断处理者 + 边界守门员”

这也是 day30 最值得学的地方。

---

## 5. 当前实现路线

### 步骤 1：复制 day29 成熟骨架
保留这些成熟部分：

- QEMU EDU 设备模型
- coherent DMA 分配
- `probe/remove`
- IRQ 注册
- 字符设备注册
- 宿主构建脚本、guest init、records 提取框架

### 步骤 2：驱动新增 `mmap`
最小收口规则：

- 只允许 `offset == 0`
- 只允许 `length == PAGE_ALIGN(dma_bytes)`
- 通过 `dma_mmap_coherent()` 建立映射

### 步骤 3：用户态工具接管验证
工具做：

- `mmap()`
- 写 src pattern
- 清 dst
- ioctl 触发 DMA
- 用户态直接比较映射区

### 步骤 4：补非法映射路径
至少覆盖：

- 非法长度
- 非法 offset

### 步骤 5：补文档与 records
保证每一步都能落到：

- `serial.log`
- `mmap-verify.txt`
- `run-result.txt`
- `dmesg-driver.txt`
- `run-summary.md`

---

## 6. 为什么先不做更复杂的功能

### 不做部分页映射
因为 day30 的重点不是复杂 VMA 切片，而是把主链路做通。

### 不做多进程并发
因为当前最需要的是“单进程稳定可解释”，不是并发正确性。

### 不做复杂 cache 场景
因为当前 buffer 由 `dma_alloc_coherent()` 分配，day30 要先把 coherent 路线吃透。

---

## 7. 我对 day30 的最终建议路线

### 一句话
**先做“整页 `mmap` + 用户态零拷贝 verify”，再谈泛化。**

### 落地顺序
1. 驱动 `mmap` + ioctl 完整
2. 用户态工具接管 pattern 与 compare
3. guest 自动化可跑通
4. records 自动提取
5. 文档沉淀

---

## 8. 当天结束前必须看到的证据

- `mmap-verify.txt` 里出现：
  - `verify_ok=1`
  - `mismatch_index=-1`
- `run-result.txt` 里出现：
  - `run_ok=1`
  - `run_error=0`
- `invalid-mmap-len.txt` 里出现：
  - 拒绝非法长度
- `invalid-mmap-offset.txt` 里出现：
  - 拒绝非法 offset
- `serial.log` 里出现：
  - `===DAY30:COMPLETE===`
