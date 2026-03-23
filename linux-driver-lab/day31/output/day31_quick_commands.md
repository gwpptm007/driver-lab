# day31 quick commands

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day31
source env/day31.env
make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
make backend
sudo -E make run
```

guest 内单独复测：

```bash
/bin/day31_edu_bench_tool /dev/day31_edu0 mmap-verify 256 0x41
/bin/day31_edu_bench_tool /dev/day31_edu0 bench-ioctl 1000 100
/bin/day31_edu_bench_tool /dev/day31_edu0 bench-mmap 256 1000 100 0x41
/bin/day31_edu_bench_tool /dev/day31_edu0 bench-dma 256 1000 100 0x41
/bin/day31_edu_bench_tool /dev/day31_edu0 bench-all 1000 100 0x41
```
