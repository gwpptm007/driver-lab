# day29 DMA 记录模板

## 1. 本次环境

- RUN_ID：
- QEMU：
- 内核镜像：
- BusyBox：
- verify 长度：
- verify seed：

## 2. DMA 关键参数

- `dma_mask_bits`：
- `dma_bytes`：
- `dma_handle`：
- `src_offset`：0
- `dst_offset`：2048
- EDU 内部 buffer offset：`0x40000`

## 3. 验证结果

- `verify_ok`：
- `verify_error`：
- `mismatch_index`：
- `irq_delta`：
- `last_dma_cmd`：

## 4. 关键证据文件

- `tool-info.txt`
- `dma-verify.txt`
- `verify-result.txt`
- `dmesg-driver.txt`
- `run-summary.md`

## 5. 结论

- 今天是否通过：
- 最大收获：
- 遗留问题：
- 给 day30 的输入：


## 6. 当前包内通过样本（records/day29-local-001）

- `verify_len=256`
- `verify_seed=0x41`
- `verify_ok=1`
- `verify_error=0`
- `irq_delta=2`
- `last_dma_cmd=0x00000007`
