# day30 quick commands

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day30
source env/day30.env
make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
make backend
sudo -E make run
```

查看结果：

```bash
cat records/${RUN_ID}/run-summary.md
cat records/${RUN_ID}/mmap-verify.txt
cat records/${RUN_ID}/run-result.txt
cat records/${RUN_ID}/invalid-mmap-len.txt
cat records/${RUN_ID}/invalid-mmap-offset.txt
cat records/${RUN_ID}/dmesg-driver.txt
```
