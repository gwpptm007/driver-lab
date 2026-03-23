# day31：bench：吞吐 / 延迟 / CPU 占用


## 0. 开始前先补本地准备

如果 `third_party/pciutils/` 还没有源码，建议先执行：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day31
source env/day31.env
source env/local_wq7.env   # 或你自己的 local.<name>.env

chmod +x scripts/*.sh
chmod +x guest/init.day31

bash scripts/01_fetch_pciutils.sh
bash scripts/02_build_guest_lspci.sh
```

如果你在 day29/day30 已经构建过可用的 arm64 静态 `lspci`，也可以直接复用，不必重新编译。

## 1. 今日定位

- 周期：W5
- 后端设备：QEMU EDU（DMA-capable）
- 直接基线：day30 的 coherent DMA + `mmap` 零拷贝主链路
- 当日目标：把 day30 已经能跑通的链路，转换成可以量化比较、可重复执行、可归档的 bench 结果

Day31 不再追求“再发明一个新功能”，而是回答下面这些更像工程问题的问题：

- `ioctl` 控制路径大概有多重
- 纯 `mmap` 用户态内存访问路径大概有多快
- 真正走 EDU DMA 的端到端路径大概有多慢/多稳
- 小负载和大负载下，吞吐与延迟的变化是什么
- 零拷贝的收益到底主要体现在哪：延迟、吞吐还是 CPU 占用

---

## 2. Day31 一句话目标

**基于当前代码，完成 `ioctl / mmap / dma` 三条路径的最小 bench，输出吞吐、单次延迟分位数和 CPU 占用，并沉淀成 records 与报表模板。**

---

## 3. Day31 的三条被测路径

### 路径 A：`ioctl` 控制路径

这里不搬大块数据，重点是观察一次“用户态 → 内核态 → 返回”的控制开销。

在当前实现里，基准动作是：

- `DAY31_IOC_GET_INFO`

它不涉及 DMA，也不做大块数据搬运，所以更像“控制路径的最轻基线”。

### 路径 B：`mmap` 用户态路径

这里使用 day31 驱动暴露出来的 coherent DMA buffer 映射区，但**不让设备参与**。单次操作定义为：

1. 用户态写 `src` pattern
2. 用户态清 `dst`
3. 用户态 `memcpy(dst, src, len)`
4. 用户态 `memcmp(src, dst, len)`

这样能把“用户态直接访问映射 buffer”的成本单独量化出来。

### 路径 C：DMA 端到端路径

这里沿用 day30 的硬件数据路径，但把“计时”和“汇总”做成 bench 版。单次操作定义为：

1. 用户态写 `src` pattern
2. 用户态清 `dst`
3. 通过 `DAY31_IOC_RUN_DMA` 发起两段 DMA
4. 用户态回到映射区比较 `src / dst`

这条路径是当前 day31 最重要的 bench 主链路。

---

## 4. 当前实现的统计口径

当前 tool 会为每个模式输出：

- `iterations`
- `warmup`
- `payload_bytes`
- `success_ops`
- `failed_ops`
- `success_rate`
- `wall_total_us`
- `avg_us`
- `p50_us`
- `p95_us`
- `p99_us`
- `min_us`
- `max_us`
- `throughput_mbps`
- `cpu_user_pct`
- `cpu_sys_pct`

说明：

- `throughput_mbps` 以“逻辑 payload 字节数”为基准计算，不额外乘以 DMA 的往返次数。
- `cpu_user_pct / cpu_sys_pct` 由 bench 进程自身的 `getrusage()` 与 wall-clock 差分得到，反映的是该 bench 进程在测试窗口内的 CPU 占用情况。

---

## 5. 推荐执行顺序

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day31
source env/day31.env
source env/local.wq7.env   # 若你本地已经有

make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
make backend
sudo -E make run
```

如果你只想在 guest 里手工复测某一个 bench 子命令，优先看：

- `docs/01_LOCAL_RUNBOOK.md`
- `guest/init.day31`
- `tools/day31_edu_bench_tool.c`

---

## 6. 当前目录最重要的文件

- `driver/day31_edu_bench.c`
  - Day31 内核模块。提供 coherent DMA buffer、字符设备、`mmap`、`ioctl` 和最小统计。
- `include/day31_edu_uapi.h`
  - guest 工具与驱动共享的接口定义。
- `tools/day31_edu_bench_tool.c`
  - Day31 guest bench 工具。实现 `bench-ioctl / bench-mmap / bench-dma / bench-all`。
- `guest/init.day31`
  - initramfs 里的自动化入口。负责留证、运行 bench、输出 markers。
- `scripts/`
  - 宿主机侧构建、运行、records 提取。
- `output/day31_bench_report_template.csv`
  - bench 结果模板，可用于后续整理最终报表。

---

## 7. 当日验收建议

### 必过项

- guest 能跑完 `mmap-verify`
- guest 能跑完 `bench-ioctl`
- guest 能跑完 `bench-mmap`
- guest 能跑完 `bench-dma`
- `records/` 中能看到有效的 bench 输出
- 无 panic / oops / DMA mapping error

### bench-all 的定位

`bench-all` 在 day31 中仍然保留，但默认**不自动执行**。
原因是它会把多组 `ioctl / mmap / dma` 再跑一遍，在 QEMU EDU 场景下会显著拉长自动化时间。

需要完整矩阵时，再在宿主机显式设置：

```bash
export DAY31_RUN_BENCH_ALL=1
```

## 8. 当前代码默认值

当前 day31 默认值已经调整为：

- `QEMU_TIMEOUT_SEC=360`
- `DAY31_BENCH_ITER=200`
- `DAY31_BENCH_WARMUP=20`
- `DAY31_RUN_BENCH_ALL=0`

这样默认自动化更容易完整跑完 `mmap-verify / bench-ioctl / bench-mmap / bench-dma`。

## 9. 关于包内旧 records 的说明

包内若仍保留较早的 `records/day31-local-001`，它们更多用于展示“之前为什么会超时”。
真正复测时，应以当前代码默认值重新生成新的 records 为准。


## 10. 基于当前 records/day31-local-001 的验收结论

基于包内当前 `records/day31-local-001`，day31 默认主链路已经通过。

直接证据如下：

- `run-summary.md` 中：
  - `edu device visible: yes`
  - `probe logged: yes`
  - `dma_alloc_coherent logged: yes`
  - `mmap verify ok: yes`
  - `bench ioctl present: yes`
  - `bench mmap present: yes`
  - `bench dma present: yes`
  - `guest flow complete: yes`
  - `qemu timeout hit: no`
  - `oops/dma-error/hung/panic found: no`
- `mmap-verify.txt` 中：
  - `verify_ok=1`
  - `run_ok=1`
  - `irq_delta=2`
- `bench-ioctl.txt`、`bench-mmap.txt`、`bench-dma.txt` 都有有效统计结果。
- `bench-all.txt` 显示的是“默认跳过”，这符合当前 day31 的设计，不属于失败项。

当前这轮最值得记住的三组结果：

- `bench-ioctl`：`avg_us=15.942`，`p99_us=23.616`
- `bench-mmap`：`avg_us=0.557`，`throughput_mbps=886.949`
- `bench-dma`：`avg_us=200222.804`，`p99_us=208327.968`，`irq_delta=2`

因此，这版 day31 的准确口径应当是：

**默认 bench 主链路验收通过；`bench-all` 作为扩展矩阵能力保留，但不纳入默认自动化必过项。**
