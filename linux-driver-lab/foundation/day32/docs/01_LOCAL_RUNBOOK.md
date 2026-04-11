# Day32 Local Runbook

## 1. 一次性准备

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day32
chmod +x scripts/*.sh
chmod +x guest/init.day32
source env/day32.env
source env/local_wq7.env 2>/dev/null || true
```

## 2. 获取 pciutils 并构建 guest lspci

```bash
make fetch-pciutils
make build-lspci
```

## 3. 默认 full 流程

```bash
make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
make backend
sudo -E make run
```

## 4. 宿主 perf baseline / optimized

```bash
sudo -E make perf-baseline
sudo -E make perf-optimized
make compare-perf
```

## 5. 重点查看

```bash
cat records/day32-local-001/run-summary.md
cat records/day32-local-001/compare-mmap.txt
cat output/day32_perf_summary.md
tail -n 120 records/day32-local-001/serial.log
```
