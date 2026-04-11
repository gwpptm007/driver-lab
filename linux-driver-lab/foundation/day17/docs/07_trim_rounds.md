# 07_trim_rounds - Day17 中的裁剪轮次

## 1. round1 在 Day17 里的意义

round1 对应的是“先把明显无关的大块去掉”。

当前 `trim_round1.fragment` 主要覆盖：

- 一批明显与当前 QEMU virt baseline 无关的网卡驱动
- 声音子系统
- 部分 USB / HID / host controller 项

## 2. round2b 在 Day17 里的意义

round2b 对应的是“在 round1 基础上继续向下收，但尽量避免伤到 baseline 主链路”。

当前 `trim_round2b.fragment` 主要覆盖：

- 一批显示 / DRM 平台项
- soundwire
- I2C helper driver
- 少量 USB 平台项

## 3. Day17 为什么用 round2b 命名

因为你前面的 day16 实际收口已经不是简单的 round2，而是 round2b。Day17 里直接用
`trim_round2b.fragment`，可以减少后面阅读时的歧义。


## 4. 在 Day17 里怎么真正做 round 对比

推荐顺序：

1. 先确认 baseline 已经通过，尤其是：
   - `boot_ok=yes`
   - `trace_smoke_ok=yes`
   - `perf_bin_ok=yes`
   - `perf_smoke_ok=yes`
2. 再跑 `round1`，看功能是否仍保持不变，同时观察 `boot_ms / image_kib / rootfs_kib` 是否下降。
3. 最后跑 `round2b`，重点观察它相对 round1 是否继续带来收益，以及是否引入新的回归。

可以直接使用：

```bash
./run_compare_rounds.sh
```

也可以拆开来跑：

```bash
./run_profile_collect.sh baseline
./run_profile_collect.sh round1
./run_profile_collect.sh round2b
python3 ./compare_results.py
```
