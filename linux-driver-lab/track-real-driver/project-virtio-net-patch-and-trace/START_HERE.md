# START_HERE

## 推荐阅读顺序

1. `README.md`
2. `docs/01_PROJECT_GOAL.md`
3. `docs/02_SOURCE_BASELINE.md`
4. `docs/03_PATCH_POINT_SELECTION.md`
5. `docs/04_TRACE_PLAN.md`
6. `docs/05_BEFORE_AFTER_VALIDATION.md`
7. `docs/06_EXECUTION_FLOW.md`
8. `docs/07_FINAL_CONCLUSION_TEMPLATE.md`
9. `docs/08_SHARE_SCRIPT.md`
10. `reports/project_exec_board.md`

## 建议开工顺序

### Step 1：确认基础输入已具备
优先复用：
- `../lab-virtio-net-source-dive/`
- `../lab-virtio-net-runtime-observe/`
- `../lab-virtio-net-queue-poll-observe/`

### Step 2：创建本轮 records 目录
```bash
./scripts/bootstrap_project_record.sh
```

### Step 3：收集 before
```bash
./scripts/collect_before.sh <ifname> <record-dir>
```

### Step 4：形成真实 patch
可以先用：
```bash
./scripts/create_patch_stub.sh
```
之后再替换成真实 patch。

### Step 5：运行 patch 后 workload
```bash
./scripts/run_ping_validation.sh <ifname> <peer-ip> <record-dir>
./scripts/run_iperf_validation.sh <ifname> <server-ip> <record-dir>
```

### Step 6：收集 after + diff
```bash
./scripts/collect_after.sh <ifname> <record-dir>
./scripts/diff_project_data.sh <record-dir>
```

### Step 7：补 trace / 运行期说明
```bash
./scripts/collect_trace_stub.sh <record-dir>
```

### Step 8：写收口材料
- `SUMMARY.md`
- `PATCH_REVIEW_NOTE.md`
- `TRACE_REVIEW_NOTE.md`
- `FINAL_PROJECT_NOTE.md`
