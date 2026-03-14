# 09_result_reading - 结果怎么看

## 1. 先看哪个文件

第一优先级：

- `baseline.csv`
- `metrics.env`
- `serial.log`

如果你在做 perf 集成，第二优先级直接看：

- `perf_version.txt`
- `perf_list.txt`
- `perf_stat.txt`
- `perf_manifest.txt`

---

## 2. 重点关注哪些字段

- `boot_ms`
- `memtotal_kib`
- `memfree_kib`
- `slab_kib`
- `modules_loaded_count`
- `tracing_ok`
- `function_graph_ok`
- `trace_smoke_ok`
- `perf_bin_ok`
- `perf_smoke_ok`
- `insmod_ok`
- `snapshot_ok`
- `trigger_ok`
- `dmesg_warn`

---

## 3. 怎样理解 perf_bin_ok / perf_smoke_ok

### 场景 A：你显式做了 perf 集成

如果你是这样构建的：

```bash
PERF_REQUIRED=yes PERF_MODE=auto ./build.sh
```

那么结果应该按“强校验”来看：

- `perf_bin_ok=yes`：guest 里能找到并执行 perf
- `perf_smoke_ok=yes`：`perf stat -e task-clock -- true` 能通过

如果这时仍然是 `no/pending`，就说明 perf 集成没有真正完成。

### 场景 B：你显式跳过 perf

如果你是：

```bash
PERF_MODE=skip ./build.sh
```

那么 `perf_bin_ok=no`、`perf_smoke_ok=pending` 是预期内现象，不表示 baseline 失败。

---

## 4. perf 相关文本怎么用

### `perf_version.txt`
用来看 perf 本体是否能启动。很多“文件明明存在却报 not found”的问题，在这里会直接暴露成 loader/缺库报错。

### `perf_list.txt`
用来看 perf 至少是否能枚举 software events。

### `perf_stat.txt`
这是最终的最小冒烟结果。只要它通过，通常说明 perf 二进制和依赖链已经基本完整。

### `perf_manifest.txt`
这是 Day17 build.sh 生成的依赖清单。它最适合回答两个问题：

- perf 本体到底是从哪里来的
- 哪些解释器/依赖被打进了 rootfs
