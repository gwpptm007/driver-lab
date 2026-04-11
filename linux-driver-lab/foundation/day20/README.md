# Day20 - 回归自动化最终总结版

## Day20 是什么

Day20 对应 W3 主线里的 **“把回归自动化”**。

它不是继续做新的内核裁剪，而是把 Day15~Day18 已经建立起来的 arm64 + QEMU virt + BusyBox + `demo_regmap.ko` 这条主线，整理成一套可重复执行、可快速判定、可留档、可交付的回归套件目录。

当前这版已经收成 **“最终总结版”**：

- 独立目录
- 需求分析文档
- 回归项清单
- 宿主机 / guest 脚本分层
- records 归档
- latest / summary / verify / suite 统一入口
- 交付状态输出
- 最终总结与验收清单

---

## 当前阶段一句话结论

> **Day20 的回归套件结构、入口、自检、归档和总结链路已经成立；当前包内缺 `Image/rootfs/dtb`，因此“套件已可交付、真实回归待补运行件后执行”。**

---

## Day20 原始需求

来自 `docs/W3_REVIEW.md` 的核心含义：

- 输出回归清单
- 建立自动化脚本结构
- 能对启动、驱动 demo、tracing、perf、压力项做自动检查

---

## 当前阶段产物

### 文档侧

- `START_HERE.md`
- `FIRST_RUN.md`
- `DIRECTORY_TREE.md`
- `FINAL_SUMMARY.md`
- `docs/01_day20_plan.md` ~ `docs/14_final_wrapup.md`

### 脚本侧

- `run_day20_regression.sh`
- `run_day20_regression.py`
- `run_day20_summary.sh`
- `run_day20_latest.sh`
- `run_day20_verify.sh`
- `run_day20_suite.sh`
- `summarize_day20_records.py`
- `inspect_day20_record.py`
- `verify_day20_suite.py`
- `guest/guest_day20_common.sh`
- `guest/guest_day20_smoke.sh`
- `guest/guest_day20_trace.sh`
- `guest/guest_day20_perf.sh`
- `guest/guest_day20_stress.sh`

### 输出侧

- `output/day20_delivery_status.md`
- `output/day20_records_summary.md`
- `output/day20_latest_report.md`
- `output/day20_mode_summary.md`
- `output/day20_records_index.md`
- `output/day20_final_summary.md`

---

## 你现在最该先用哪几个命令

### 1. 先看套件与交付状态

```bash
./run_day20_suite.sh verify
```

### 2. 先看最近一次结论

```bash
./run_day20_suite.sh latest
```

### 3. 刷新汇总

```bash
./run_day20_suite.sh summary
```

### 4. 先看运行件是否齐

```bash
./run_day20_suite.sh dry-run
```

### 5. 真正跑完整回归

```bash
./run_day20_suite.sh all
```

---

## 建议阅读顺序

1. 先看 `FINAL_SUMMARY.md`
2. 再看 `output/day20_final_summary.md`
3. 再看 `output/day20_delivery_status.md`
4. 然后看 `START_HERE.md`
5. 再看 `FIRST_RUN.md`
6. 最后按需翻 `docs/` 细节说明

---

## 这版 Day20 的价值

这版已经能把问题分成四层：

- 套件结构是否齐
- 运行件是否齐
- 最近一次回归是否通过
- 失败后先看哪里

也就是说，它已经不是“有脚本”，而是：

> **有统一入口、有 records、有 summary/latest/verify、有交付状态、有最终总结的回归套件目录。**
