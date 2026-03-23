# day31 LOCAL RUNBOOK

## 1. 第一次进入 day31 先做这些准备

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day31
source env/day31.env
source env/local_wq7.env   # 若你本地就是 wq7 这套目录
# 或 source env/local.<yourname>.env

chmod +x scripts/*.sh
chmod +x guest/init.day31
```

如果 `third_party/pciutils/` 还没有获取：

```bash
bash scripts/01_fetch_pciutils.sh
```

如果 day31 需要自己构建 guest 侧 `lspci`：

```bash
bash scripts/02_build_guest_lspci.sh
```

## 2. 宿主机侧推荐命令

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day31
source env/day31.env
# 需要时再 source 你自己的本机覆盖文件
# source env/local_wq7.env

make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
make backend
sudo -E make run
```

## 3. guest 里最重要的命令

假设设备节点已经创建为 `/dev/day31_edu0`：

```bash
/bin/day31_edu_bench_tool /dev/day31_edu0 info
/bin/day31_edu_bench_tool /dev/day31_edu0 mmap-verify 256 0x41
/bin/day31_edu_bench_tool /dev/day31_edu0 bench-ioctl 200 20
/bin/day31_edu_bench_tool /dev/day31_edu0 bench-mmap 256 200 20 0x41
/bin/day31_edu_bench_tool /dev/day31_edu0 bench-dma 256 200 20 0x41
/bin/day31_edu_bench_tool /dev/day31_edu0 bench-all 200 20 0x41   # 可选：完整矩阵
/bin/day31_edu_bench_tool /dev/day31_edu0 result
cat /dev/day31_edu0
```

## 4. 最关键的 records 文件

- `serial.log`
- `tool-info.txt`
- `mmap-verify.txt`
- `bench-ioctl.txt`
- `bench-mmap.txt`
- `bench-dma.txt`
- `bench-all.txt`（默认自动化会写入 skipped 提示；显式开启后才会是真实矩阵结果）
- `run-result.txt`
- `dmesg-driver.txt`
- `run-summary.md`

## 5. 判断第一轮是否值得继续

只要下面这些成立，就说明 day31 主链路有继续分析的价值：

- `mmap-verify` 成功
- `bench-ioctl` 有输出
- `bench-mmap` 有输出
- `bench-dma` 有输出
- 没有 panic / oops / DMA mapping error


## 关于 DMA bench 超时预算

当前代码默认已经采用 `QEMU_TIMEOUT_SEC=360`、`DAY31_BENCH_ITER=200`、`DAY31_BENCH_WARMUP=20`。
默认自动化不执行 `bench-all`；若要开启完整矩阵，可在宿主机执行前显式设置 `DAY31_RUN_BENCH_ALL=1`。
