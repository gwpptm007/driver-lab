# 05_EXECUTION_FLOW

## Step 1：确认接口与环境
```bash
ethtool -i <ifname>
```

## Step 2：创建 records 目录
```bash
./scripts/bootstrap_record_dir.sh
```

## Step 3：跑 idle
```bash
./scripts/run_idle_window.sh <ifname> <record-dir>
```

## Step 4：跑 ping
```bash
./scripts/run_ping_window.sh <ifname> <peer-ip> <record-dir>
```

## Step 5：跑 iperf3
```bash
./scripts/run_iperf_window.sh <ifname> <server-ip> <record-dir>
```

## Step 6：做差异总结
```bash
./scripts/summarize_deltas.sh <record-dir>
```

## Step 7：写结论
最少补：
- `SUMMARY.md`
- `CHAIN_REVIEW_NOTE.md`
- `IDLE_PING_IPERF_COMPARE.md`
