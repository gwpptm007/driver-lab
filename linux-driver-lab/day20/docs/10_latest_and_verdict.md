# Day20 最新结果入口与判定

## 1. 为什么要补这一层

当 `records/` 开始累计多次 dry-run、smoke、trace、perf、stress 之后，
只靠人工翻目录会越来越慢。

所以 Day20 这一版补了两个更顺手的入口：

- 最新一条记录是谁
- 最新一条记录现在是 PASS / FAIL / READY / MISSING_INPUTS

这能让你先做“快速判断”，再决定要不要进 record 深挖。

## 2. 新增的输出文件

执行：

```bash
./run_day20_summary.sh
```

现在除了原来的：

- `output/day20_records_summary.csv`
- `output/day20_records_summary.md`

还会生成：

- `output/day20_latest_record.txt`
- `output/day20_latest_report.md`
- `output/day20_mode_summary.md`

## 3. verdict 的含义

### READY
最近一次是 dry-run，而且运行件已经齐，可以开始真实回归。

### MISSING_INPUTS
最近一次是 dry-run，但 `Image/rootfs/dtb/module` 还没齐。

### PASS
最近一次真实回归完成，而且当前 `fail_keys/missing_keys` 为空。

### FAIL
最近一次真实回归完成，但至少有一个关键检查项失败，或者缺失。

### UNKNOWN
当前信息不足，需要直接打开 `summary.txt` 看。

## 4. 新的查看入口

### 看最新结果

```bash
./run_day20_latest.sh
```

### 看指定 record

```bash
./run_day20_latest.sh 20260315-xxxxxx-day20-all-arm64-virt
```

## 5. 推荐使用顺序

1. 先跑 `./run_day20_summary.sh`
2. 再看 `output/day20_latest_report.md`
3. 再跑 `./run_day20_latest.sh`
4. 如果需要，再进入 `records/<record_dir>/` 深挖
