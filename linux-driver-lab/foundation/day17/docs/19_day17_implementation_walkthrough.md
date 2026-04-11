# Day17 实现过程与代码走读

## 1. 文档目标

本文站在“代码怎么串起来”的角度，对 Day17 的实现过程做一次统一走读。

目标不是重复测试命令，而是回答：

1. Day17 的代码链是如何从 profile 一路走到 records 的
2. 哪些脚本负责“准备配置”，哪些脚本负责“真正构建”
3. perf 为什么会被并到 Day17，而不是单独放在外部
4. round compare 的 evidence 为什么必须存在

---

## 2. Day17 的实现主线

Day17 的实现可以拆成四条子链：

1. **配置链**：`apply_config.sh` 负责 profile 与 fragment
2. **构建链**：`build.sh` 负责模块、rootfs、perf、dtb、QEMU
3. **采样链**：`guest_collect.sh` + `host_collect.sh`
4. **对比链**：`run_profile_collect.sh` + `run_compare_rounds.sh` + `compare_results.py`

这四条链组合起来，形成一个完整闭环：

- profile 决定内核配置
- build 产生产物并启动 guest
- collect 采样功能与资源指标
- compare 汇总 profile 差异和 evidence

---

## 3. 配置链：为什么要有 apply_config.sh

### 3.1 它解决的不是“编译内核”，而是“表达 Day17 的 profile”

如果没有 `apply_config.sh`，那么 baseline / round1 / round2b 之间的差异就会散落在：
- 若干 fragment 文件里
- 若干手工命令里
- 若干 README 说明里

这样复测时很容易失控。

因此 `apply_config.sh` 的职责不是尽可能做所有事情，而是：

> **把 Day17 的 profile 逻辑变成一个显式入口。**

### 3.2 它的核心工作

1. 识别 `PROFILE=baseline|round1|round2b`
2. 构建 fragment 链
3. 把 fragment 逐条应用到当前 `.config`
4. 通过 `olddefconfig` 收敛依赖关系
5. 为后续 build 提供一份稳定、可追溯的配置

### 3.3 为什么这里要做 fragment 叠加，而不是整份 .config 覆盖

因为 Day17 的目标是“可读、可比较、可分层”，不是“每个 profile 都放一份完整 .config”。

fragment 叠加的好处是：
- baseline 与 trim 层次清晰
- 哪个 profile 改了什么，一眼能看出来
- 后续还能继续叠 round3 / round4

---

## 4. 构建链：build.sh 为什么最重

`build.sh` 是 Day17 的主构建入口，它承接了前面 day15/day16 分散的职责。

### 4.1 它做的事情

1. 编译 `demo_regmap.ko`
2. 准备 BusyBox rootfs
3. 保证 rootfs 里有最小命令集
4. 自动发现或构建 perf
5. 把 perf 与动态依赖拷进 rootfs
6. 生成 `rootfs.img`
7. 生成 `virt-day17.dtb`
8. 默认启动 QEMU

### 4.2 为什么 perf 也放进 build.sh

perf 集成过程中，真正困难的不是“把 perf 文件拷进 guest”，而是：
- loader 路径
- libc / libm
- sysroot 与 target 路径映射
- guest 里 `/bin/true` 这类 smoke workload 依赖

这些逻辑都属于“最终 rootfs 怎么组出来”，因此放进 build.sh 更合理。

### 4.3 为什么最后还保留 build_perf.sh

`build_perf.sh` 的作用是把 perf 这件事拆成一个单独可观察的小链：
- 先把 perf 本体编出来
- 再交给 build.sh 注入 rootfs

这样调试时更容易区分：
- 是 perf 本体没编出来
- 还是 perf 注入 rootfs 失败
- 还是 guest 运行时依赖缺失

---

## 5. 采样链：guest_collect 与 host_collect 怎么分工

### 5.1 guest_collect.sh 的职责

它是 Day17 的“被测逻辑执行者”，负责在 guest 内实际做这些动作：

- 检查 `debugfs`
- 检查 `tracing`
- 检查 `function_graph`
- 装载 `demo_regmap.ko`
- 读取 snapshot
- 检查 perf 并运行 smoke
- 生成 `metrics.env`
- 把文本块打印到串口 marker 中

它并不负责“保存到宿主机文件系统”，它只负责：

> **在 guest 里完成最小实验验证，并把结果通过串口吐出来。**

### 5.2 host_collect.sh 的职责

它是 Day17 的“外部自动化与归档者”，负责：

- 启动 QEMU
- 等 prompt
- 与 shell 做握手
- 注入 `day17_guest_collect.sh`
- 从 `serial.log` 中提取 env block 与 file block
- 写成宿主机侧文件
- 合并为 `metrics.env` / `baseline.csv`

### 5.3 为什么需要 marker 机制

串口日志天然是杂乱的：
- 有 prompt
- 有回显
- 有 dmesg
- 有命令输出

如果没有 marker，就很难可靠地把：
- `metrics.env`
- `snapshot.txt`
- `perf_stat.txt`
从 `serial.log` 中抽出来。

因此 Day17 采用的办法是：
- `__DAY17_ENV_BEGIN__ / __DAY17_ENV_END__`
- `__DAY17_FILE_BEGIN__ name / __DAY17_FILE_END__ name`

这使得 host 侧能稳定地从一条串口流里分块抽取结果。

---

## 6. 对比链：为什么要有 evidence

### 6.1 没有 evidence 时会发生什么

一开始 round compare 最大的问题是：
- baseline / round1 / round2b 都 PASS
- image_kib 看起来差不多
- 但不知道 profile 到底有没有真正改到 `.config`

如果没有 evidence，就只能靠猜：
- 是 fragment 没生效？
- 还是已经生效，只是裁掉的东西本来不影响当前镜像？

### 6.2 build_evidence 解决了什么

每轮保存这些文件后：
- `kernel.config`
- `kernel.config.focus.txt`
- `applied_fragments.txt`
- `Image.sha256`
- `rootfs.img.sha256`
- `artifact_evidence.env`

就能直接回答：
- profile 传没传进去
- `.config` 变没变
- `Image` 变没变
- rootfs 变没变

### 6.3 为什么 compare_results.py 必须升级

Day17 不只是要“收一张 compare.csv”，还要把结论写清楚：
- baseline vs round1 到底差了什么
- round1 vs round2b 到底差了什么
- config diff 是不是空
- image hash 是否真的变了

所以 compare_results.py 最终承担了三件事：

1. 汇总 profile 的 metrics
2. 计算 evidence hash
3. 生成 Markdown + diff 结果，方便人读

---

## 7. round1 / round2b 为什么前面长期不生效

Day17 round compare 在中期出现过一个典型误区：

> 以为“fragment 不同”就等于“最终 `.config` 会不同”。

实际上不成立。

当时的问题是：
- round1 / round2b 的 fragment 记录下来了
- profile 传递链也没问题
- 但很多要裁的项，在 baseline 中本来就是 `n`

所以最终结果是：
- `applied_fragments.txt` 不同
- `kernel.config` 一样
- `Image` 一样

这个阶段最大的教训是：

> **裁剪对比不是只看 fragment 文件写了什么，而是要看当前 baseline 里哪些项真实为 y/m。**

---

## 8. fix5 为什么有效

fix5 不再继续尝试“可能无关的 SoC 子项”，而是直接选择 baseline 中当前真实为 `y` 的顶层项：

- round1：去掉 `PCI` + `SCSI`
- round2b：在 round1 基础上继续去掉 `NET`

这样带来的好处是：

1. baseline vs round1 一定会产生 `.config` 差异
2. round1 vs round2b 一定会继续产生 `.config` 差异
3. 能先确认 profile 差异链真的打通
4. 再去评估裁剪收益

事实证明这一步是关键拐点。

---

## 9. 当前代码最值得优先读的文件

### 第一优先级
- `README.md`
- `docs/17_day17_final_summary_and_round_compare.md`
- `docs/18_day17_final_test_process.md`

先建立全局概念。

### 第二优先级
- `apply_config.sh`
- `build.sh`
- `collect/guest_collect.sh`
- `collect/host_collect.sh`

看主执行链。

### 第三优先级
- `run_profile_collect.sh`
- `run_compare_rounds.sh`
- `compare_results.py`
- `check_round_profiles.sh`

看对比与 evidence 链。

### 第四优先级
- `config/trace_baseline.fragment`
- `config/trim_round1.fragment`
- `config/trim_round2b.fragment`

结合 diff 看 profile 差异。

---

## 10. 当前代码状态总结

现在的 Day17 已经不是零散脚本集合，而是一套层次比较清晰的工程结构：

- `apply_config.sh`：表达 profile
- `build.sh`：表达产物构建
- `guest_collect.sh`：表达 guest 内实验验证
- `host_collect.sh`：表达串口自动化与归档
- `compare_results.py`：表达 profile 差异与 evidence

因此现在理解 Day17 的最好方式不是“逐文件死读”，而是按这四条链去看。

---

## 11. 一句话总结

> **Day17 的实现过程，本质上是把 baseline、perf、round compare、evidence 四条链从分散状态逐步收口成一个独立实验目录；当前代码已经具备工程化闭环，而不再只是若干脚本和手工命令的集合。**
