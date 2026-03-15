# Day20 首次执行建议

## 1. 推荐顺序

第一次执行 Day20，最稳的顺序是：

1. `--dry-run`
2. `MODE=smoke`
3. `MODE=trace`
4. `MODE=perf`
5. `MODE=all`

这样做的原因是：

- 先确认输入件路径和 records 归档逻辑没问题；
- 再确认最短主线 smoke 没问题；
- 再逐步打开 trace / perf；
- 最后再尝试一次全跑。

---

## 2. dry-run 的意义

`--dry-run` 不启动 QEMU，只做：

- 自动探测 `Image`
- 自动探测 `rootfs.img`
- 自动探测 `virt-*.dtb`
- 自动探测 `demo_regmap.ko`
- 建立 `records/` 目录
- 输出 `host_plan.env` 与 `summary.txt`

这个阶段最适合排查“路径错了”“没构建出来”“day18 产物不在默认位置”等问题。

---

## 3. smoke 的价值

smoke 回归是 Day20 最关键的第一层：

- boot 到 prompt
- `debugfs` 可挂载
- `insmod` 成功
- `snapshot` 可读
- `trigger` 可写
- `rmmod` 成功
- dmesg 尾部没有严重错误模式

只要 smoke 不稳，trace / perf 都没有必要先纠结。

---

## 4. trace / perf 分开跑的原因

trace 与 perf 本来就代表两类不同能力：

- trace 代表 tracing / function_graph 主线仍然存在
- perf 代表 perf userspace + kernel config 主线仍然完整

分开跑能更快知道：

- 是 tracing 被裁坏了
- 还是 perf 集成不完整
- 还是两者都还在，只是 smoke 主线出问题

---

## 5. 这版和 Day17 / Day18 的关系

Day20 不是替代 Day17 / Day18，而是站在它们之上继续推进：

- Day17 / Day18：建立 profile、build、collect、compare 能力
- Day20：把回归动作固定成“宿主机脚本 + guest 脚本 + records 归档”

所以 Day20 是自动化层，而不是新的裁剪层。


## 跑完之后别只看 records

现在建议先执行：

```bash
./run_day20_summary.sh
```

这样能先在 `output/` 看到一层总览，再决定看哪条 record。
