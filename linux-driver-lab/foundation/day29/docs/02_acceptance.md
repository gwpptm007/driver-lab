# Day29 验收说明

## 1. 必须满足

- [x] `lspci -nn` 中可见 `1234:11e8`
- [x] 驱动 `probe` 成功
- [x] `dma_set_mask_and_coherent()` 成功
- [x] `dma_alloc_coherent()` 成功
- [x] 往返 DMA 校验通过（`verify_ok=1`）
- [x] 无 `DMA mapping error`
- [x] 无 `BUG:` / `Oops:` / `Kernel panic`
- [x] guest 串口中出现 `===DAY29:COMPLETE===`

## 2. 建议额外补充

- [x] 记录本次 verify 的长度和 seed
- [x] 记录 `dma_handle` 与 buffer 大小
- [x] 写出“为什么不是直接把虚拟地址给设备”的说明
- [ ] 记录一次长度非法的失败样本（可选补充）

## 3. 优先查看的证据文件

```bash
cat records/${RUN_ID}/run-summary.md
sed -n '1,120p' records/${RUN_ID}/tool-info.txt
sed -n '1,120p' records/${RUN_ID}/dma-verify.txt
sed -n '1,120p' records/${RUN_ID}/verify-result.txt
sed -n '1,200p' records/${RUN_ID}/dmesg-driver.txt
```

## 4. 基于当前 records/day29-local-001 的逐条判定

### 4.1 设备存在

`lspci-nn.txt` 中出现：

- `1234:11e8`

说明 QEMU `-device edu` 已被 guest 正确枚举。

### 4.2 probe 与 DMA API 通过

`dmesg-driver.txt` 中出现：

- `probe success`
- `dma mask set to 32 bits`
- `dma_alloc_coherent ok`

说明：

- `pci_enable_device()` / BAR 映射链路已经通过
- 驱动成功把 DMA mask 收口到 32-bit
- 一致性 DMA buffer 已申请成功

### 4.3 往返 DMA 校验通过

`dma-verify.txt` / `verify-result.txt` 中出现：

- `verify ok: len=256 seed=0x41 irq_delta=2`
- `verify_ok=1`
- `verify_error=0`
- `mismatch_index=-1`

说明本轮 `RAM -> EDU internal buffer -> RAM` 的 round-trip 校验已经闭环。

### 4.4 中断链路通过

`dma-verify.txt` 中可以看到两次 IRQ：

- `irq handler ... count=1`
- `irq handler ... count=2`

同时 `verify-result.txt` 中 `irq_delta=2`，说明两段 DMA 都完成并产生中断。

### 4.5 全流程跑完且无异常

`serial.log` 中出现：

- `===DAY29:COMPLETE===`

`run-summary.md` 中显示：

- `guest flow complete: yes`
- `oops/dma-error/hung/panic found: no`

说明本轮自动化、guest 退出、records 提取都已完成，且未发现 panic / oops / DMA mapping error。

## 5. 本轮最终结论

基于当前包内的 `records/day29-local-001`，Day29 已经达到本日“验收通过”口径：

- 平台 bring-up 通过
- PCI 枚举通过
- probe 通过
- DMA coherent 申请通过
- MSI 中断通过
- round-trip 校验通过
- records 证据链完整

所以这版可以作为 **day29 验收通过版** 继续往 day30 演进。
