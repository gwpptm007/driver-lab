# Day30 测试结果分析（基于 records/day30-local-001）

## 1. 本轮结论

基于当前上传代码自带的 `records/day30-local-001`，**day30 的主学习目标已经达成**：

- coherent DMA buffer 已成功映射给用户态；
- 用户态已直接写 `src_off`、清 `dst_off`；
- 内核已完成两段 EDU DMA；
- 用户态最终完成 `src == dst` 比对；
- guest 流程完整结束；
- 无 panic / oops / DMA mapping error。

也就是说，**day30 的 mmap 零拷贝主链路是通过的**。

同时，本轮 records 还暴露出一个很有学习价值的点：

- `invalid mmap offset` 验证通过；
- `invalid mmap len` 在当前 records 中显示为“no”，但这并不直接等价于驱动边界没收住；
- 结合代码与 `mmap(2)` 语义分析，当前样例使用的 `2048` 会在 4KB 页大小下被向上扩展成 `4096`，
  从而误变成一个合法整页映射请求。

因此，本轮更准确的判断应当是：

> **主功能验收通过；非法长度失败路径的旧样例不具备有效性，后续应用 `4097` 重新留证。**

---

## 2. 通过项分析

### 2.1 设备与驱动 bring-up 通过

证据：`records/day30-local-001/run-summary.md`

- `edu device visible: yes`
- `probe logged: yes`
- `dma_alloc_coherent logged: yes`
- `guest flow complete: yes`
- `oops/dma-error/hung/panic found: no`

这说明：

1. QEMU 中的 EDU 设备已被枚举到；
2. day30 驱动已完成 `probe()`；
3. coherent DMA buffer 已分配成功；
4. guest 自动化脚本没有在 bring-up 阶段崩掉。

### 2.2 mmap 主链路通过

证据：`records/day30-local-001/mmap-verify.txt`

- `mmap ok: len=4096 pgoff=0`
- `mmap_ok=1`
- `mmap_error=0`
- `mmap_len=4096`
- `mmap_pgoff=0`

这说明：

1. 驱动 `day30_mmap()` 的主成功路径已被真正走到；
2. 用户态并不是只调 ioctl，而是确实建立了 DMA buffer 的映射；
3. 当前 day30 的“整页映射 + offset=0”边界设计与用户态工具是一致的。

### 2.3 DMA 主链路通过

证据：`records/day30-local-001/mmap-verify.txt` 与 `run-result.txt`

- `run_dma start: len=256 seed=0x41`
- 两次 `irq handler`
- `run_dma ok: len=256 seed=0x41 irq_delta=2`
- `run_ok=1`
- `run_error=0`
- `irq_delta=2`
- `last_dma_cmd=0x00000007`

这说明：

1. stage1 `RAM -> EDU` 成功；
2. stage2 `EDU -> RAM` 成功；
3. 本轮 round-trip 期间共触发了两次 IRQ，符合两段 DMA 的预期；
4. 驱动已经不仅“能映射”，而且“能在映射的 buffer 基础上完成 DMA round-trip”。

### 2.4 用户态零拷贝验证通过

证据：`records/day30-local-001/mmap-verify.txt`

- `verify_ok=1`
- `mismatch_index=-1`
- `mismatch_expected=0x00`
- `mismatch_actual=0x00`

这说明：

1. 用户态对映射区里的 `src` 和 `dst` 完成了直接比较；
2. 本轮没有出现任何 mismatch；
3. day30 相比 day29 的关键学习目标——“把 compare 的主角切给用户态”——已经落地。

### 2.5 offset 失败路径通过

证据：`records/day30-local-001/invalid-mmap-offset.txt`

- `expected failure: invalid offset rejected, page_off=1 errno=22(Invalid argument)`

以及 `mmap-verify.txt` 前面的 dmesg：

- `mmap rejected: pgoff=1 expected=0`

这说明：

1. 驱动对 `offset != 0` 的边界收得住；
2. 非法 `pgoff` 路径已经被真正触发并留证。

---

## 3. 当前 records 里最容易误判的一项：invalid mmap len

证据：`records/day30-local-001/invalid-mmap-len.txt`

- `unexpected success: invalid length 2048 should be rejected`

如果只看这一句，很容易得出“驱动没检查 len”的结论；但结合驱动代码和 Linux `mmap()` 的页对齐语义，实际并不是这样。

### 3.1 驱动当前实现

`day30_mmap()` 明确检查：

- `vma->vm_pgoff == 0`
- `len == PAGE_ALIGN(d->dma_bytes)`，当前即 `4096`

也就是说，驱动设计上确实是“只接受整页映射”。

### 3.2 为什么 2048 没能构成非法请求

因为 `mmap()` 建立 VMA 时，长度会按页向上取整：

- 用户态请求 `2048`
- 4KB 页环境下，VMA 长度会被扩成 `4096`
- 驱动看到的 `len` 已经是 `4096`
- 于是它恰好落成了一个合法整页映射

所以这条 records 更准确的含义是：

> **当前非法长度样例选错了，不足以证明驱动 len 边界失效。**

### 3.3 正确的后续样例

为了稳定触发非法长度拒绝，应当改成：

- `4097`
- 或 `map_bytes + 1`

在 4KB 页系统里，这类请求最终 VMA 长度会变成 `8192`，从而稳定命中驱动的：

- `mmap rejected: len=8192 expected=4096`

---

## 4. 对本轮验收的建议口径

### 4.1 如果按“day30 核心学习目标”评估

可以判定：**通过**。

原因是 day30 最核心的目标已经全部成立：

- buffer 已成功映射给用户态；
- 用户态已直接写 src / 读 dst；
- DMA round-trip 已成功；
- 用户态 compare 已成功；
- guest 全流程已跑完。

### 4.2 如果按“原始验收清单逐项留证”评估

当前更准确的说法是：

- 主链路通过；
- `invalid mmap offset` 失败路径通过；
- `invalid mmap len` 这一项的**现有记录样例无效**，应使用修正后的 4097 用例复测并补证。

因此，本次交付文档里应避免把 `invalid mmap len` 当前这份旧记录写成“驱动失败”，而应写成：

> **旧样例存在页对齐误判，代码已修正为 4097，用于下一轮稳定补证。**

---

## 5. 你从这次测试真正学到的点

这次测试最有价值的，不只是 day30 主链路跑通，而是你会真正意识到：

1. `mmap()` 的长度语义不是“用户写多少，驱动就看到多少”；
2. 用户态测试样例本身也会影响对驱动边界的判断；
3. 失败路径留证时，必须同时理解：
   - 用户态参数
   - VMA 页对齐
   - 驱动最终看到的 `len/pgoff`

这正是 day30 和前几天相比，更接近真实内核/驱动联调的一点。
