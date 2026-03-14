# Day16 RESULTS_ROUND2B

## 1. round2b 结果总评

Day16 round2b 已通过手工验证与宿主机自动采集验证。

这说明在 round1 的基础上，继续收掉显示栈上层（DRM 相关链）之后：

- Day15 baseline 主链仍然成立
- tracing / function_graph 仍然可用
- `demo_regmap.ko` 仍可加载并工作
- 镜像体积继续小幅下降

---

## 2. round2b 运行时验证结果

### guest 手工验证

round2b 后，在 guest 内采样得到：

- `boot_ok=yes`
- `debugfs_ok=yes`
- `tracing_ok=yes`
- `function_graph_ok=yes`
- `trace_smoke_ok=yes`
- `insmod_ok=yes`
- `snapshot_ok=yes`
- `trigger_ok=yes`
- `dmesg_warn=no`

`available_tracers.txt` 仍然包含：

```text
function_graph wakeup_dl wakeup_rt wakeup irqsoff function nop
```

### host 自动采集结果

- `scenario_id=day16-round2b-arm64-virt`
- `kernel_ver=5.15.10`
- `image_bytes=37995008`
- `image_kib=37105`
- `rootfs_bytes=1209015`
- `rootfs_kib=1181`
- `boot_ms=2013`
- `memtotal_kib=999476`
- `memfree_kib=970408`
- `memavailable_kib=939956`
- `slab_kib=12056`
- `sreclaimable_kib=6680`
- `sunreclaim_kib=5376`
- `kernelstack_kib=744`
- `pagetables_kib=104`

并且功能状态保持：

- `debugfs_ok=yes`
- `tracing_ok=yes`
- `function_graph_ok=yes`
- `trace_smoke_ok=yes`
- `insmod_ok=yes`
- `snapshot_ok=yes`
- `trigger_ok=yes`
- `dmesg_warn=no`

---

## 3. 与 Day15 baseline、Day16 round1 的三轮对比

| 指标 | Day15 baseline | Day16 round1 | Day16 round2b |
|---|---:|---:|---:|
| image_bytes | 39799296 | 38130176 | 37995008 |
| image_kib | 38867 | 37237 | 37105 |
| boot_ms | 2008 | 2021 | 2013 |
| memfree_kib | 968564 | 969716 | 970408 |
| memavailable_kib | 938172 | 939280 | 939956 |
| slab_kib | 12252 | 12108 | 12056 |
| pagetables_kib | 104 | 68 | 104 |

### 3.1 Image 变化

- baseline → round1：`-1669120 bytes`（约 `-1630 KiB`）
- baseline → round2b：`-1804288 bytes`（约 `-1762 KiB`）
- round1 → round2b：`-135168 bytes`（约 `-132 KiB`）

### 3.2 启动时间变化

- baseline：`2008 ms`
- round1：`2021 ms`
- round2b：`2013 ms`

整体可视为**基本持平**。

### 3.3 运行时内存变化

总体趋势是：

- `memfree_kib` 小幅增加
- `memavailable_kib` 小幅增加
- `slab_kib` 小幅下降

说明 round1 + round2b 裁剪后，运行时内核占用有一定轻微改善。

---

## 4. round2b 的真实意义

round2b 不是简单继续追着关叶子项，而是通过排查依赖来源，最终定位到：

- `CONFIG_DRM` 顶层会拉起 `I2C_ALGOBIT`
- `DRM_SUN4I` 等残余显示链会继续拉住 `DRM_DW_HDMI`

因此 round2b 的核心动作是：

- 继续收显示栈上层
- 让 DRM 相关链路整体退出

最终结果证明这条路线是正确的。

---

## 5. round2b 结论

Day16 round2b 已通过。

可以正式得出结论：

- round1：成功裁掉网络/声音/部分 USB 大块
- round2b：成功进一步裁掉显示栈上层
- 整体上，在不破坏 Day15 baseline 主链的前提下，镜像累计缩小约 `1.76 MiB`

