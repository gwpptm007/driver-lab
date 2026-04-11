# Day20 FIRST_RUN

## 1. 这一步要做什么

Day20 的目标不是“再裁一次内核”，而是验证：

- 现在这条 arm64 + QEMU virt 主线能否自动启动；
- guest 里能否自动做 smoke / trace / perf / stress 检查；
- 结果能否自动归档进 `records/`；
- 套件结构和输出入口本身是否完整。

---

## 2. 第一次建议先做 dry-run

先不要急着真正启动 QEMU，先确认脚本能找到输入件：

```bash
cd linux-driver-lab/day20
./run_day20_suite.sh dry-run
```

你应该看到：

- 新建了一个 `records/<timestamp>-day20-.../`
- 里面有 `host_plan.env`
- `summary.txt` 会写出当前自动探测到的：
  - `Image`
  - `rootfs.img`
  - `virt-*.dtb`
  - `demo_regmap.ko`

如果这一步就失败，先别看 guest 脚本，先把输入件路径对齐。

---

## 3. 先看最近一次结果和 suite 状态

```bash
./run_day20_suite.sh latest
./run_day20_suite.sh verify
```

重点看：

- `output/day20_latest_report.md`
- `output/day20_delivery_status.md`

这样你能先分清：

- 是运行件没齐；
- 还是最近一次真实回归失败；
- 还是 Day20 套件结构本身被改坏了。

---

## 4. 再跑 smoke / trace / perf / stress

```bash
./run_day20_suite.sh smoke
./run_day20_suite.sh trace
./run_day20_suite.sh perf
./run_day20_suite.sh stress
./run_day20_suite.sh all
```

---

## 5. 跑完后先看什么

优先看：

1. `output/day20_latest_report.md`
2. `output/day20_delivery_status.md`
3. `records/.../summary.txt`
4. `records/.../pass_fail.env`
5. `records/.../host_runner.log`
6. `records/.../serial.log`

如果结果不对，再继续看：

- `trace_excerpt.txt`
- `perf_stat.txt`
- `dmesg_tail.txt`
- `snapshot_before.txt`
- `snapshot_after.txt`
- `stress.log`

---

## 6. 当前这版要接受什么现实

这版 Day20 已经更接近最终交付回归套件，但仍然要接受一个现实：

- 真实回归是否能通过，仍取决于当前代码包里有没有带齐 image/rootfs/dtb/module；
- 所以 `SUITE_READY=1` 不代表 `REGRESSION_PASS=1`；
- `RUNTIME_READY=0` 也不代表脚本坏了，很多时候只是大文件没一起带进来。

这正是 Day20 增加 `verify` 和 `delivery_status` 的原因。
