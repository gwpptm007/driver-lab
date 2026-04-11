# day34 本地运行手册

## 1. 本地准备

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day34
chmod +x scripts/*.sh
chmod +x guest/init.day34
source env/day34.env
source env/local_wq7.env 2>/dev/null || true
```

## 2. 获取 pciutils（如未已有）

```bash
make fetch-pciutils
make build-lspci
```

## 3. 构建并运行

```bash
make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
make backend
sudo -E make run
```

## 4. 运行完成后重点查看

```bash
cat records/day34-local-001/run-summary.md
cat records/day34-local-001/concurrent-stress.txt
cat records/day34-local-001/module-loop.txt
cat records/day34-local-001/fault-invalid-len.txt
cat records/day34-local-001/fault-mmap-offset.txt
cat records/day34-local-001/run-result.txt
```

## 5. 常见调参

- 并发 worker 数量：`DAY34_CONCURRENCY_WORKERS`
- mmap worker 迭代：`DAY34_CONCURRENCY_ITERS`
- ioctl worker 迭代：`DAY34_IOCTL_ITERS`
- 模块循环次数：`DAY34_MODULE_LOOPS`
- QEMU 超时：`QEMU_TIMEOUT_SEC`
