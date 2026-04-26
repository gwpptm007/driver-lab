# START_HERE

## 推荐阅读顺序

1. `README.md`
2. `docs/01_GOAL_AND_SCOPE.md`
3. `docs/02_PATCH_POINT_SELECTION.md`
4. `docs/03_CODE_PATH_AND_STATS_SURFACE.md`
5. `docs/04_BASELINE_AND_BEFORE_AFTER_PLAN.md`
6. `docs/05_PATCH_EXECUTION_FLOW.md`
7. `docs/06_VALIDATION_AND_REVIEW.md`
8. `reports/ethtool_patch_exec_board.md`

## 建议开工顺序

### 第 1 步：确认 baseline 已经存在
优先复用：
- `../lab-virtio-net-runtime-observe/` 中已有的 records
- 或重新跑一轮轻量 baseline

### 第 2 步：选 patch 点
优先从：
- stats 展示
- ethtool 可见性
- 低风险 control-plane 输出

中选一个小点，不要一开始改重语义。

### 第 3 步：生成本轮 records 目录
```bash
./scripts/bootstrap_record_dir.sh
```

### 第 4 步：收集 before
```bash
./scripts/collect_before.sh <ifname> <record-dir>
```

### 第 5 步：形成 patch
可以先用：
```bash
./scripts/create_patch_stub.sh
```
生成补丁说明骨架，再结合实际 git diff 形成真实 patch。

### 第 6 步：收集 after
```bash
./scripts/collect_after.sh <ifname> <record-dir>
./scripts/diff_stats.sh <record-dir>
```

## 当前最推荐的 patch 类型

### 首选
- ethtool stats 语义梳理与展示增强
- 对已有统计项做更清晰的 before/after 对照

### 次选
- 轻量控制面增强
- 明确某一 capability 对外暴露的解释和验证路径

### 当前不推荐
- 直接改重的收发主路径语义
- 大范围调整 XDP / offload 行为
