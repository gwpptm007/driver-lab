# Day16 RESULTS_ROUND1

## 1. round1 结果总评

Day16 round1 已通过。

既实现了裁剪，又没有破坏 Day15 baseline 主链路。

---

## 2. 与 Day15 baseline 对比

### Day15 baseline

- `image_bytes = 39799296`
- `boot_ms = 2008`
- `memfree_kib = 968564`
- `memavailable_kib = 938172`
- `slab_kib = 12252`
- `pagetables_kib = 104`

### Day16 round1

- `image_bytes = 38130176`
- `boot_ms = 2021`
- `memfree_kib = 969716`
- `memavailable_kib = 939280`
- `slab_kib = 12108`
- `pagetables_kib = 68`

### 变化

- Image 缩小：`-1669120 bytes`（约 `-1630 KiB`，约 `4.2%`）
- 启动时间基本持平：`+13 ms`
- `slab_kib` 和 `pagetables_kib` 小幅下降

---

## 3. 功能回归结果

round1 后仍保持：

- `debugfs_ok=yes`
- `tracing_ok=yes`
- `function_graph_ok=yes`
- `trace_smoke_ok=yes`
- `insmod_ok=yes`
- `snapshot_ok=yes`
- `trigger_ok=yes`
- `dmesg_warn=no`

## 4. 结论

round1 成功裁掉了一批与当前实验无关的大块驱动，并保持了 baseline 主链路完好。
