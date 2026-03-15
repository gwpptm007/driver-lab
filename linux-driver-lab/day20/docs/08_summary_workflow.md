# Day20 汇总视图与排查入口

## 为什么要补这一层

Day20 不是只会“跑一次脚本”。
真正有工程价值的是：

- 每次跑完后，能快速知道最新一轮是过还是没过；
- 多次 records 放在一起时，能横向看趋势；
- 打开 `output/` 就能先看摘要，再决定进哪个 record 细看。

所以这一步补了：

- `run_day20_summary.sh`
- `summarize_day20_records.py`
- `output/day20_records_summary.csv`
- `output/day20_records_summary.md`

## 使用方式

```bash
cd linux-driver-lab/day20
./run_day20_summary.sh
```

或者直接跑回归：

```bash
MODE=all ./run_day20_regression.sh
```

`run_day20_regression.sh` 结束后会顺手刷新一次 summary。

## 先看哪里

建议先看：

1. `output/day20_records_summary.md`
2. `output/day20_records_summary.csv`
3. 再进最新的 `records/<timestamp>-day20-.../`

## 这一层解决了什么问题

它解决的不是“能不能跑”，而是“跑完后能不能快速读结果”。

没有 summary 时，Day20 很容易变成：

- records 越积越多
- 但每次都得手工点进去找 `summary.txt`
- 很难快速对比 smoke / trace / perf / stress 的变化

补上 summary 后，Day20 的自动化链就更完整了。


## 补充

现在 summary workflow 不只输出总表，还会输出 latest report 和 mode 维度汇总。
