# Day18 - 第二轮分类裁剪（独立实验目录）

## 1. Day18 的原始任务是什么

根据 `docs/W3_REVIEW.md`，Day18 的原始要求不是继续“凭感觉删配置”，而是做 **第二轮分类裁剪**：

- D16：粗裁，去明显无关项
- D18：分类裁剪，把配置分成几个角色桶，解释为什么保留、为什么删除

也就是说，Day18 的重点已经不只是“最终变小多少”，而是：

> **把裁剪逻辑做成可解释、可复用、可回滚的结构。**

---

## 2. Day18 在本仓库里的定位

Day17 已经完成了四件事：

- baseline 独立执行链
- perf 集成
- round compare
- evidence 证据链

Day18 继续往前走时，不应该再去改 day17 目录，而应该：

- 把所有代码、脚本、文档、records 都独立落在 `day18/`
- 在 day18 内把“第二轮分类裁剪”做成新的 profile / fragment / 文档体系
- 保留一个 legacy 对照组，方便解释 day17 → day18 的演进

所以 Day18 的最终定位是：

> **一个独立的分类裁剪实验目录：既能沿用 day17 的执行骨架，又能用 Day18 的分类方法解释最终 `.config`。**

---

## 3. 当前 day18 做成了什么

### 3.1 profile 设计

当前 day18 提供三个 profile：

- `baseline`
  - 只保留 tracing / function_graph / perf baseline
- `round2b_legacy`
  - 沿用 day17 的 legacy 方案：`PCI + SCSI + NET` 关闭
- `classified`
  - 采用 Day18 的分类表达：
    - required
    - platform
    - debug
    - perf
    - trim

### 3.2 配置分层

当前配置分层在 `day18/config/`：

- `trace_baseline.fragment`
- `trim_round1.fragment`
- `trim_round2b.fragment`
- `10_required.fragment`
- `20_platform.fragment`
- `30_debug.fragment`
- `40_perf.fragment`
- `90_trim_day18.fragment`
- `category_manifest.csv`

### 3.3 独立脚本链

day18 目录内已经独立具备：

- `apply_config.sh`
- `build.sh`
- `build_perf.sh`
- `run_qemu.sh`
- `run_profile_collect.sh`
- `run_compare_profiles.sh`
- `check_profile_equivalence.sh`
- `compare_results.py`
- `export_category_view.py`
- `collect/guest_collect.sh`
- `collect/host_collect.sh`

### 3.4 证据链增强

相比 day17，day18 额外保存：

- `kernel.savedefconfig`
- `category_manifest.csv`
- `classified` 与 `round2b_legacy` 的对照结果

---

## 4. Day18 技术路线

### 路线 A：保留 day17 的可运行骨架

Day18 不重新发明执行链，继续复用这些已经验证过的能力：

- arm64 / QEMU virt 启动
- BusyBox initramfs
- demo_regmap.ko 验证
- debugfs / tracing / function_graph
- perf 基础验证
- host / guest 采样与 records 归档

### 路线 B：把“第二轮裁剪”从删项升级成分类表达

Day18 把配置分成五类：

1. required
2. platform
3. debug
4. perf
5. trim

这样以后解释配置时，不再只是：

- “这个我留了”
- “那个我关了”

而是能直接说：

- 这是系统能活着的必须项
- 这是 arm64+virt 平台项
- 这是调试项
- 这是 perf 项
- 这是当前实验确实可删除的 trim 项

### 路线 C：保留 legacy 对照组

Day18 没有直接抛弃 day17 的 round2b，而是保留 `round2b_legacy`：

- 便于连续性
- 便于做 equivalence 检查
- 便于回答“day18 是表达升级，还是最终结果也变了”

---

## 5. 需要掌握哪些知识点

Day18 建议重点掌握这些知识点：

### 5.1 内核配置知识

- `.config` / fragment / olddefconfig / savedefconfig 的关系
- 顶层 Kconfig 开关与子符号联动
- 为什么有些配置改了 `.config`，但最终 `Image` 变化很小

### 5.2 启动链知识

- arm64 `Image` 启动方式
- QEMU virt + PL011 + GIC
- initramfs / `/init` / BusyBox 启动流程

### 5.3 设备模型知识

- OF / Device Tree
- platform_driver / of_match
- IRQ domain / GIC
- regmap / regmap-mmio

### 5.4 可观测性知识

- debugfs
- ftrace
- function_graph
- perf events
- 为什么这些能力要在裁剪时被单独归类保留

### 5.5 工程化知识

- baseline / legacy / classified 对照
- evidence 证据链
- 配置哈希 / 产物哈希
- 可回滚配置表达

---

## 6. 推荐执行顺序

### 6.1 先生成分类总览

```bash
cd linux-driver-lab/day18
python3 export_category_view.py
```

### 6.2 先只看 classified 最终配置是否能落到 `.config`

```bash
PROFILE=classified BUILD_KERNEL=no ./apply_config.sh
```

### 6.3 跑三轮 profile 闭环采样

```bash
PERF_REQUIRED=yes PERF_MODE=auto ./run_profile_collect.sh baseline
PERF_REQUIRED=yes PERF_MODE=auto ./run_profile_collect.sh round2b_legacy
PERF_REQUIRED=yes PERF_MODE=auto ./run_profile_collect.sh classified
```

这三次分别在验证：

- baseline：作为最小可观测性参考线
- round2b_legacy：作为 day17 第二轮旧写法对照组
- classified：作为 day18 分类表达结果

### 6.4 汇总三轮 profile 对比结果

```bash
./run_compare_profiles.sh
```

这一步不是重新测试功能，而是把三组最新 records 聚合成：

- compare csv
- compare md
- config diff 文件

用于回答“谁更小、谁更稳、谁和 baseline 差多少”。

### 6.5 检查 legacy 与 classified 是否等价

```bash
./check_profile_equivalence.sh
```

这一步专门回答 Day18 的核心问题：

- classified 是不是只是“分类化重构”
- 它和 round2b_legacy 的最终配置是不是一致

---


## 6.1 建议优先阅读的文档

第一次进入 Day18，建议按这个顺序阅读：

1. `START_HERE.md`
2. `FIRST_RUN.md`
3. `docs/01_day18_requirement_analysis.md`
4. `docs/07_execution_steps_and_validation.md`
5. `docs/08_perf_build_analysis.md`
6. `DIRECTORY_TREE.md`

这样可以先搞清楚：

- Day18 原始需求是什么
- 每一步命令在验证什么
- 当前 perf 构建结果意味着什么
- 整个目录是怎么分层的

## 7. 验收看什么

Day18 的验收不只看“能不能启动”，而要看四层：

### 7.1 功能层

- `boot_ok=yes`
- `debugfs_ok=yes`
- `tracing_ok=yes`
- `function_graph_ok=yes`
- `trace_smoke_ok=yes`
- `perf_bin_ok=yes`
- `perf_smoke_ok=yes`
- `insmod_ok=yes`

### 7.2 产物层

- `kernel_config_sha256`
- `kernel_savedefconfig_sha256`
- `kernel_image_sha256`
- `rootfs_img_sha256`

### 7.3 解释层

- `category_manifest.csv` 存在
- `docs/02_category_matrix.md` 可生成
- `classified` 的 fragment 链清晰

### 7.4 对照层

- baseline vs round2b_legacy
- baseline vs classified
- round2b_legacy vs classified

---

## 8. 当前注意事项

这次 day18 代码目录已经独立好了，但 **没有在这里实际跑内核编译与 QEMU 测试**，所以：

- `records/` 目前还是空骨架
- 需要你在本地环境继续跑真实结果

也就是说，这一版是：

> **独立目录 + 分类裁剪实现 + 文档与脚本骨架已完成；真实 records 需要你本地继续采样。**

---

## 9. 推荐先看哪些文档

- `START_HERE.md`
- `FIRST_RUN.md`
- `docs/01_day18_requirement_analysis.md`
- `docs/02_category_matrix.md`
- `docs/03_day18_technical_route.md`
- `docs/04_day18_profile_design.md`
- `docs/05_day18_learning_points.md`
- `docs/06_day18_acceptance_and_next_steps.md`


## 6.2 推荐补读

- `docs/07_execution_steps_and_validation.md`
- `docs/10_script_measurement_and_matching.md`


## 7. 本轮 records 验收结论（基于 2026-03-15 最新结果）

本轮已经实际执行并完成以下 5 个脚本：

```bash
PERF_REQUIRED=yes PERF_MODE=auto ./run_profile_collect.sh baseline
PERF_REQUIRED=yes PERF_MODE=auto ./run_profile_collect.sh round2b_legacy
PERF_REQUIRED=yes PERF_MODE=auto ./run_profile_collect.sh classified
./run_compare_profiles.sh
./check_profile_equivalence.sh
```

基于 `records/20260315-142352-day18-baseline-arm64-virt`、
`records/20260315-142411-day18-round2b_legacy-arm64-virt`、
`records/20260315-142432-day18-classified-arm64-virt` 以及 compare/equivalence 汇总文件，
本轮 Day18 可以判定为：

- **独立目录目标通过**：所有脚本、配置、文档、records、compare/equivalence 都独立落在 `day18/`
- **功能闭环通过**：三组 profile 都能启动，`demo_regmap.ko` 能加载，`debugfs/tracing/function_graph/perf` 全部可用
- **分类表达目标通过**：`round2b_legacy` 与 `classified` 的 `kernel.config` 完全一致，说明 Day18 已把 legacy 结果重构成分类表达
- **收益量化结论有限**：当前 `baseline / round2b_legacy / classified` 的最终 `kernel.config` 与 `Image` sha256 相同，因此本轮更适合得出“功能与方法通过”的结论，而不是“裁剪收益显著”

建议把 Day18 的最终表述写成：

> **Day18 通过。其通过点在于：独立目录、三组 profile 闭环、records 证据链、以及 legacy → classified 的等价重构均已完成。**
>
> **需要额外说明的是：本轮数据更强地证明了“方法和表达重构成立”，而不是“baseline 相比 trimmed profile 有明显收益差异”。**
