# day18：第二轮分类裁剪（独立实验目录）

## 1. 今日定位

- 周期：W3 收口日
- 前置输入：day17 的 baseline / round1 / round2b 裁剪实验
- 今日目标：在 day17 基础上，把"分类裁剪"从"删项"升级为"结构化分类表达"

换句话说，day18 的重点已经不只是"最终变小多少"，而是：

> **把裁剪逻辑做成可解释、可复用、可回滚的结构。**

---

## 2. day18 输出什么

day18 最终关心 4 件事：

1. **独立目录骨架**：所有代码、脚本、文档、records 独立落在 `day18/`
2. **三组 profile 对照**：baseline / round2b_legacy / classified
3. **分类表达方法**：required / platform / debug / perf / trim 五类
4. **legacy 等价性验证**：classified 与 round2b_legacy 的配置是否一致

---

## 3. 当前 day18 做成了什么

### 3.1 三组 profile

- `baseline`：只保留 tracing / function_graph / perf baseline
- `round2b_legacy`：沿用 day17 的 legacy 方案（PCI + SCSI + NET 关闭）
- `classified`：Day18 的分类表达（required / platform / debug / perf / trim）

### 3.2 配置分层

```
day18/config/
├── 10_required.fragment     # 系统必须项
├── 20_platform.fragment    # arm64+virt 平台项
├── 30_debug.fragment       # 调试项
├── 40_perf.fragment        # perf 相关项
├── 90_trim_day18.fragment  # 本轮可删除项
└── category_manifest.csv    # 分类清单
```

### 3.3 独立脚本链

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

---

## 4. 技术路线

### 路线 A：复用 day17 已验证的执行骨架

Day18 不重新发明执行链，继续复用：

- arm64 / QEMU virt 启动
- BusyBox initramfs
- demo_regmap.ko 验证
- debugfs / tracing / function_graph
- perf 基础验证

### 路线 B：把"第二轮裁剪"从删项升级成分类表达

把配置分成五类后，解释配置时不再是"这个留了、那个关了"，而是能说清楚：

- 这是系统能活着的必须项
- 这是 arm64+virt 平台项
- 这是调试项
- 这是 perf 项
- 这是当前实验确实可删除的 trim 项

### 路线 C：保留 legacy 对照组

保留 `round2b_legacy`，便于做 equivalence 检查，回答"day18 是表达升级，还是最终结果也变了"。

---

## 5. 推荐执行顺序

### 5.1 先生成分类总览

```bash
cd linux-driver-lab/day18
python3 export_category_view.py
```

### 5.2 验证 classified 配置是否落到 `.config`

```bash
PROFILE=classified BUILD_KERNEL=no ./apply_config.sh
```

### 5.3 跑三轮 profile 闭环采样

```bash
PERF_REQUIRED=yes PERF_MODE=auto ./run_profile_collect.sh baseline
PERF_REQUIRED=yes PERF_MODE=auto ./run_profile_collect.sh round2b_legacy
PERF_REQUIRED=yes PERF_MODE=auto ./run_profile_collect.sh classified
```

### 5.4 汇总三轮对比结果

```bash
./run_compare_profiles.sh
```

### 5.5 检查 legacy 与 classified 是否等价

```bash
./check_profile_equivalence.sh
```

---

## 6. day18 和前后天的关系

- 输入：day17 的 baseline / round1 / round2b 实验结果
- 输出：分类裁剪方法论 + 三组 profile 对照证据
- 后续：进入 W4（PCIe 基本功）

---

## 7. 验收看什么

day18 的验收不只看"能不能启动"，而要看四层：

### 7.1 功能层

- `boot_ok=yes`
- `debugfs_ok=yes`
- `tracing_ok=yes`
- `function_graph_ok=yes`
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
- classified 的 fragment 链清晰

### 7.4 对照层

- baseline vs round2b_legacy
- baseline vs classified
- round2b_legacy vs classified

---

## 8. 当前注意事项

当前 `records/` 需要你在本地环境继续跑真实结果，独立目录 + 分类裁剪实现 + 文档与脚本骨架已完成。

---

## 9. 推荐先看哪些文档

1. `START_HERE.md`
2. `FIRST_RUN.md`
3. `docs/01_day18_requirement_analysis.md`
4. `docs/07_execution_steps_and_validation.md`
5. `docs/08_perf_build_analysis.md`
6. `DIRECTORY_TREE.md`

---

## 10. 本轮 records 验收结论

基于 `records/20260315-142352-day18-baseline-arm64-virt` 等三轮记录及 compare/equivalence 汇总文件，本轮 Day18 判定为：

- **独立目录目标通过**：所有脚本、配置、文档、records、compare/equivalence 都独立落在 `day18/`
- **功能闭环通过**：三组 profile 都能启动，`demo_regmap.ko` 能加载，`debugfs/tracing/function_graph/perf` 全部可用
- **分类表达目标通过**：`round2b_legacy` 与 `classified` 的 `kernel.config` 完全一致，说明 Day18 已把 legacy 结果重构成分类表达
- **收益量化结论有限**：本轮数据证明了"方法和表达重构成立"，而不是"裁剪收益显著"

> **Day18 通过。其通过点在于：独立目录、三组 profile 闭环、records 证据链、以及 legacy → classified 的等价重构均已完成。**
