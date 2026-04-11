# day30 验收清单

## 1. 通过口径

Day30 的“通过”必须同时覆盖 **驱动侧** 和 **用户态侧**，不能只看其中一边。

---

## 2. 必须满足

### A. 设备与驱动 bring-up
- [ ] `lspci -nn` 能看到 `1234:11e8`
- [ ] probe 成功
- [ ] `dma_set_mask_and_coherent()` 成功
- [ ] `dma_alloc_coherent()` 成功
- [ ] `/dev/day30_edu0` 创建成功

### B. mmap 主链路
- [ ] 用户态 `mmap()` 成功
- [ ] 用户态能直接写 `src_off`
- [ ] 用户态能直接读 `dst_off`

### C. DMA 主链路
- [ ] stage1 RAM -> EDU 成功
- [ ] stage2 EDU -> RAM 成功
- [ ] `run_ok=1`
- [ ] `run_error=0`
- [ ] IRQ 计数有增长

### D. 用户态零拷贝验证
- [ ] `verify_ok=1`
- [ ] `mismatch_index=-1`

### E. 失败路径
- [ ] 非法 `mmap` 长度被拒绝
- [ ] 非法 `mmap` offset 被拒绝

### F. 稳定性
- [ ] `===DAY30:COMPLETE===`
- [ ] 无 `Kernel panic`
- [ ] 无 `Oops`
- [ ] 无 `DMA mapping error`

---

## 3. 证据文件

验收时至少要提供：

- `records/<RUN_ID>/lspci-nn.txt`
- `records/<RUN_ID>/tool-info.txt`
- `records/<RUN_ID>/mmap-verify.txt`
- `records/<RUN_ID>/run-result.txt`
- `records/<RUN_ID>/invalid-mmap-len.txt`
- `records/<RUN_ID>/invalid-mmap-offset.txt`
- `records/<RUN_ID>/dmesg-driver.txt`
- `records/<RUN_ID>/run-summary.md`

---

## 4. 不通过时优先排查

1. `mmap` 长度/offset 是否满足驱动限制
2. guest `/init` 是否把正确 `.ko` 打进 rootfs
3. 用户态是否真的通过 mmap 在操作 DMA buffer，而不是偷偷走了其他缓冲
4. `run_ok=1` 和 `verify_ok=1` 是否被混为一谈


---

## 5. 基于当前 records/day30-local-001 的结果解读

### 已明确通过
- 设备枚举与 probe 通过
- `dma_alloc_coherent()` 通过
- 用户态 `mmap()` 主路径通过
- 两段 DMA 通过，`run_ok=1`
- 用户态零拷贝 compare 通过，`verify_ok=1`
- 非法 offset 拒绝通过
- guest 完整结束，且无 panic / oops / DMA mapping error

### 当前最需要说明的点
`invalid-mmap-len.txt` 中的 “unexpected success: invalid length 2048 should be rejected”
不应直接解读为“驱动没有检查长度”。

原因是：在 4KB 页系统里，`mmap(2048)` 建立出来的 VMA 长度仍会是 `4096`，
这和 day30 当前允许的整页映射完全一致，因此这个样例本身不再是非法请求。

### 当前建议口径
- 如果按 day30 核心学习目标衡量：本轮已经通过。
- 如果按原始清单逐项补证：非法长度样例应改为 `4097` 后再补一轮记录。
