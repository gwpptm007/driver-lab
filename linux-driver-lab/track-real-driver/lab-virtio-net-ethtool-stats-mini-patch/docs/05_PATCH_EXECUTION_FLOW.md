# 05_PATCH_EXECUTION_FLOW

## 标准执行流

### Step 1：确定 patch 点
输出：
- `PATCH_POINT_NOTE.md`

### Step 2：创建 records 目录
运行：
```bash
./scripts/bootstrap_record_dir.sh
```

### Step 3：收集 before
运行：
```bash
./scripts/collect_before.sh <ifname> <record-dir>
```

### Step 4：形成 patch
可先生成模板：
```bash
./scripts/create_patch_stub.sh
```

然后结合实际源码改动形成：
- `patches/0001-virtio_net-xxx.patch`

### Step 5：编译 / 部署 / 启动实验环境
这里沿用你当前真实实验机的实际部署方式。

### Step 6：收集 after
运行：
```bash
./scripts/collect_after.sh <ifname> <record-dir>
./scripts/diff_stats.sh <record-dir>
```

### Step 7：写结论
至少补：
- `SUMMARY.md`
- `PATCH_REVIEW_NOTE.md`
- `BEFORE_AFTER.md`

## 推荐 workload

### 第一轮
- ping

### 第二轮
- iperf3

### 当前不建议
- 过重、过复杂、变量太多的混合 workload
