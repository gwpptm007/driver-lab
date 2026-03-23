# Day29 START_HERE

## 1. 先读什么

建议按这个顺序：

1. `README.md`
2. `docs/01_LOCAL_RUNBOOK.md`
3. `docs/03_EDU_DMA_REGISTER_MAP.md`
4. `docs/04_DMA_DATA_FLOW.md`
5. 再开始执行构建与运行

## 2. 今天最重要的一句话

> 让 QEMU EDU 通过 `dma_alloc_coherent()` 申请出来的 DMA buffer，真实搬一轮数据，并且把结果校验清楚。

## 3. 最短执行路径

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day29
source env/day29.env

chmod +x scripts/*.sh
chmod +x guest/init.day29
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true

make check
# 如果 GUEST_LSPCI_BIN 不存在，再执行：make build-lspci
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
sudo chown -R "$USER:$USER" workdir
make backend
sudo -E make run
```

## 4. 最先看哪里

```bash
cat records/${RUN_ID}/run-summary.md
sed -n '1,120p' records/${RUN_ID}/dma-verify.txt
sed -n '1,120p' records/${RUN_ID}/tool-info.txt
sed -n '1,200p' records/${RUN_ID}/dmesg-driver.txt
```

## 4.1 当前包内 records 的判断

当前自带 `records/day29-local-001` 已经是一次通过样本：

- `verify_ok=1`
- `verify_error=0`
- `irq_delta=2`
- `guest flow complete: yes`
- `oops/dma-error/hung/panic found: no`

所以你下载这版后，既可以把它当学习包，也可以把它当验收参考包。

## 5. 今天不要做过头的点

- 先把 coherent DMA 最小闭环做通
- 不要抢跑到 day30 的 `mmap`
- 不要一开始就纠结性能优化
- 先留原始证据，再写总结


## 本轮已知修复

- 修复 guest rootfs 中 busybox applet 链接缺失导致的 `/init: line 8: mount: not found` 与 `Attempted to kill init!`。
- `run_qemu` 已加入 `-no-reboot` 与宿主侧超时 (`QEMU_TIMEOUT_SEC`)，避免异常时 QEMU 长时间挂住不退出。


> arm64 virt 关键点：QEMU EDU 默认仅支持 28-bit DMA；在本实验中默认通过 `-device edu,dma_mask=0xffffffff` 放宽到 32-bit，并在驱动默认值中收口为 32-bit，guest 自动化不再依赖运行时模块参数传值。
