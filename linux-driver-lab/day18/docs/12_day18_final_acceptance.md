# Day18 最终验收结论（交付版）

## 1. 交付范围

本次交付包含：

- `day18/` 独立目录下的全部脚本、配置、文档
- 三组 profile 的 latest records
- compare 汇总结果
- equivalence 验证结果

本次 records 为去除大文件 `Image`、`rootfs.img` 的轻量交付版本；
这不影响验收，因为：

- `artifact_evidence.env` 保留了 sha256 与字节数
- `Image.file.txt` / `rootfs.img.file.txt` 保留了 file 结果
- compare/equivalence 仍然可以完成结论判读

## 2. 最终判定

**判定：通过。**

但这里的“通过”需要精确理解为：

### 2.1 已通过的目标

1. **独立目录目标通过**
   - Day18 的代码、脚本、配置、文档、records 都独立在 `day18/` 下。

2. **功能闭环目标通过**
   - baseline / round2b_legacy / classified 三组 profile 全部 PASS。
   - `boot_ok/debugfs_ok/tracing_ok/function_graph_ok/trace_smoke_ok/perf_bin_ok/perf_smoke_ok/insmod_ok/snapshot_ok/trigger_ok` 全部通过。

3. **分类表达重构目标通过**
   - `round2b_legacy` 与 `classified` 的 `kernel.config` 完全一致。
   - 说明 Day18 已成功把 legacy trim 表达重构为分类表达。

### 2.2 不应夸大的结论

本轮不能夸大为：

- “baseline 相比 trimmed profile 已经被量化证明有明显差异”
- “Day18 已经证明新的分类写法比 baseline 更小、更快”

因为本轮 records 中：

- 三组 `kernel.config` sha256 相同
- 三组 `Image` sha256 相同
- `compare` 的 config diff 也是 0

所以更稳妥的结论是：

> Day18 本轮通过，主要证明的是“独立目录 + 功能闭环 + legacy/classified 等价重构”成立。

## 3. 为什么可以判通过

### 3.1 records 提供了完整闭环证据

- `serial.log`：证明 boot、guest 采集命令、模块加载、perf 执行都发生过
- `metrics.env`：给出 yes/no 验收项
- `dmesg_tail.txt`：证明 `demo_regmap` probe、IRQ、worker 链路成功
- `mount.txt` / `filesystems.txt` / `available_tracers.txt`：证明 debugfs/tracefs/function_graph 可用
- `perf_version.txt` / `perf_stat.txt` / `perf_manifest.txt`：证明 perf 真能在 guest 里运行
- `artifact_evidence.env`：证明 `.config / Image / rootfs / ko` 产物已归档
- `compare-*.md`：说明三组 profile 都 PASS
- `equivalence-*.txt`：说明 legacy/classified 最终配置等价

### 3.2 这已经覆盖了 Day18 的核心验收面

Day18 的核心不是“继续删配置直到极限”，而是：

- 建立独立目录
- 做三组 profile 对照
- 让分类表达成立
- 保持 tracing/function_graph/perf 可用
- 用 records 形成可复核证据链

这几项在本轮都已经满足。

## 4. 建议写入对外总结的话术

建议采用下面这段表述：

> Day18 已完成独立目录化，并完成 baseline、round2b_legacy、classified 三组 profile 的真实执行与 records 归档。
> 
> 本轮验证表明：系统可正常启动，demo_regmap 模块可加载并触发，中断/worker/debugfs 链路可观测，debugfs/tracing/function_graph/perf 均可用；同时 `round2b_legacy` 与 `classified` 的最终 `kernel.config` 完全一致，证明 Day18 的分类表达重构成立。
> 
> 因此，Day18 可以判定为通过。本轮更强地证明了“功能与方法通过”，而不是“收益量化差异已经显著体现”。

## 5. 后续如需继续优化的方向

如果后续想把 Day18 继续做得更“硬”，建议补两项：

1. 每个 profile 都从统一干净 baseline 重新生成 `.config`
2. 补齐 `savedefconfig` 证据链

但这两项不影响本轮“交付版通过”的判定。
