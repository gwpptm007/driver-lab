# day22 手工命令单

## 1. 预检

```bash
cd linux-driver-lab/day22
make check
```

## 2. 只做 rootfs

```bash
make rootfs
```

## 3. 分步执行

```bash
./scripts/04_start_ivshmem_server.sh
./scripts/05_run_qemu_ivshmem.sh
./scripts/06_extract_records.sh
```

## 4. 一键执行

```bash
./scripts/07_run_all.sh
```

## 5. 看最新 records

```bash
ls -1dt records/* | head -n 1
latest=$(ls -1dt records/* | head -n 1)
find "$latest" -maxdepth 1 -type f | sort
```

## 6. 重点检查

```bash
grep -n '1af4:1110' "$latest/lspci-nn.txt"
sed -n '1,200p' "$latest/lspci-vv-nn.txt"
sed -n '1,200p' "$latest/run-summary.md"
```
