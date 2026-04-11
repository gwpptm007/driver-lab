# Day20 records 阅读说明

## 1. records 目录是做什么的

Day20 的 `records/` 用来保存每次自动回归的原始材料。

它和 Day17 / Day18 的 records 风格保持一致：

- 每次运行一个独立时间戳目录
- 原始串口日志保留
- pass/fail 状态单独落盘
- 原始 trace / perf / snapshot 文本单独落盘

---

## 2. 首先看哪几个文件

### `summary.txt`

第一眼先看这个。

它会告诉你：

- 当前跑的是 smoke / trace / perf / all 中哪种模式
- 关键状态项是什么
- 总体 `REGRESSION_PASS` 是 1 还是 0

### `pass_fail.env`

这是更适合脚本解析的版本。

常见字段例如：

- `DEBUGFS_OK`
- `DEMO_INSMOD_OK`
- `SNAPSHOT_OK`
- `TRIGGER_OK`
- `RMMOD_OK`
- `FGRAPH_OK`
- `PERF_OK`
- `DMESG_CLEAN`

### `host_runner.log`

用于看宿主机侧做到了哪一步：

- 是否成功启动 QEMU
- 是否等到 prompt
- 是否成功上传 guest 脚本
- 是否成功回收 guest 输出

### `serial.log`

这是最原始、最重要的现场材料。

如果：

- QEMU 没启动起来
- prompt 一直没出现
- guest 执行异常
- 输出块缺失

最先应该看它。

---

## 3. smoke 相关文件怎么看

- `smoke.log`
- `snapshot_before.txt`
- `snapshot_after.txt`
- `dmesg_tail.txt`

这几份能帮助判断：

- 模块是否真的装上了
- snapshot 是否真的可读
- trigger 之后有没有留下可对照状态
- dmesg 是否有严重问题

---

## 4. trace 相关文件怎么看

- `available_tracers.txt`
- `trace.log`
- `trace_excerpt.txt`

通常先看：

1. `available_tracers.txt` 里有没有 `function_graph`
2. `trace_excerpt.txt` 里有没有 `demo_regmap_trigger_write` 等关键函数

如果 excerpt 为空，再回头看 `trace.log` 和 `serial.log`。

---

## 5. perf 相关文件怎么看

- `perf_version.txt`
- `perf_list.txt`
- `perf_stat.txt`

判断顺序建议是：

1. `perf --version` 是否成功
2. `perf list software` 是否有输出
3. `perf stat -e task-clock -- /bin/true` 是否成功

这样能区分：

- perf 命令不存在
- perf 文件存在但不可执行
- perf 能执行但最小 smoke 不通过

---

## 6. 失败时先分层，不要一上来就改一堆

Day20 最重要的价值，不只是跑，而是分层定位：

- 启动层失败
- guest 动作层失败
- 结果回收层失败

所以当回归失败时，建议按下面顺序看：

1. `summary.txt`
2. `host_runner.log`
3. `serial.log`
4. 对应分项的原始文本文件


## 配合 output 一起看

现在 Day20 又补了一层 `output/day20_records_summary.md` 与 `output/day20_records_summary.csv`。
建议阅读顺序变成：

1. 先看 output 汇总
2. 再进最新 record
3. 最后才看 serial.log / host_runner.log 细节


## 快速入口

现在也可以不直接翻 `records/`，而是先：

```bash
./run_day20_latest.sh
```

再决定是否进入具体 record。
