# START_HERE

## 推荐阅读顺序

1. `README.md`
2. `docs/01_GOAL_AND_SCOPE.md`
3. `docs/02_WHY_E1000E_AFTER_VIRTIO.md`
4. `docs/03_COMPARE_DIMENSIONS.md`
5. `docs/04_READING_ORDER.md`
6. `docs/05_STAGE_AND_VIRTIO_MAPPING.md`
7. `docs/06_OBSERVE_AND_VALIDATE.md`
8. `docs/07_ACCEPTANCE_AND_REVIEW.md`
9. `docs/08_SHARE_NOTES.md`
10. `reports/e1000e_compare_exec_board.md`

## 建议开工顺序

### 第 1 步：确认环境里是否有 e1000 / e1000e
先跑：
```bash
ethtool -i <ifname>
lspci -nnk | grep -A 3 -i ethernet
```

### 第 2 步：创建本轮 records 目录
```bash
./scripts/bootstrap_record_dir.sh
```

### 第 3 步：导出函数索引和关键片段
```bash
./scripts/collect_e1000e_symbols.sh
./scripts/build_function_index.sh
./scripts/extract_probe_path.sh
./scripts/extract_txrx_seed.sh
```

### 第 4 步：人工阅读并补记录
- `records/<ts>/SUMMARY.md`
- `records/<ts>/FUNCTION_NOTE.md`
- `records/<ts>/COMPARE_NOTE.md`

### 第 5 步：输出对照报告
```bash
./scripts/generate_compare_report.sh
```
