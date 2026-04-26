# 06_EXECUTION_FLOW

## 执行流程

### Step 1：创建 records
```bash
./scripts/bootstrap_project_record.sh
```

### Step 2：收集 before
```bash
./scripts/collect_before.sh <ifname> <record-dir>
```

### Step 3：形成 patch
```bash
./scripts/create_patch_stub.sh
./scripts/capture_git_diff.sh <kernel-src> <out-patch>
```

### Step 4：跑 validation
```bash
./scripts/run_ping_validation.sh <ifname> <peer-ip> <record-dir>
./scripts/run_iperf_validation.sh <ifname> <server-ip> <record-dir>
```

### Step 5：收集 after
```bash
./scripts/collect_after.sh <ifname> <record-dir>
./scripts/diff_project_data.sh <record-dir>
```

### Step 6：补 trace / 说明
```bash
./scripts/collect_trace_stub.sh <record-dir>
```

### Step 7：写项目收口文档
至少补：
- `SUMMARY.md`
- `PATCH_REVIEW_NOTE.md`
- `TRACE_REVIEW_NOTE.md`
- `FINAL_PROJECT_NOTE.md`
