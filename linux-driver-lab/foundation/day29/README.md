# day29：QEMU EDU + dma_alloc_coherent 最小闭环

## 1. 今天到底要做什么

Day29 是 W5 的真正起点。

到了这一天，重点已经不再是：

- PCI 设备能不能枚举
- `pci_driver` 能不能 probe
- 中断能不能打到 guest

这些在 day22 ~ day27 已经基本做实。

Day29 要进入的是：

> **驱动通过 DMA API 申请 coherent buffer，把 DMA 地址交给设备，让设备真实搬一次数据，再把结果校验清楚。**

也就是说，Day29 不是继续做 W4 的 MMIO/IRQ 练习，而是正式切到 **QEMU EDU 的 DMA 路径**。

---

## 2. 今天的最小闭环

今天只追求一个最小但完整的 DMA 闭环：

1. `pci_driver` 成功接住 QEMU EDU
2. 驱动执行 `dma_set_mask_and_coherent()`
3. 驱动执行 `dma_alloc_coherent()` 申请一块 DMA buffer
4. 驱动把 `dma_handle` 写入 EDU DMA 寄存器
5. EDU 把数据从 guest RAM 搬到设备内部 buffer
6. EDU 再把数据从设备内部 buffer 搬回 guest RAM
7. 驱动比较搬运前后的内容，确认一致
8. 无 crash、无 DMA mapping error、无明显异常日志

只要这条线跑通，Day29 就通过。

---

## 3. 为什么还要继续用 EDU

因为 EDU 正好能把 Day29 的知识点压得很清楚：

- PCI 设备还是你前几天已经熟悉的 `1234:11e8`
- BAR0 还是 1MB MMIO 空间
- IRQ 逻辑你也已经在 day25/day27 验证过
- 新增的只是 **DMA source/destination/count/cmd** 这一小组寄存器

这样你在 Day29 真正新增要理解的内容，主要只有三件事：

- `dma_set_mask_and_coherent`
- `dma_alloc_coherent`
- `dma_handle` 和 CPU 虚拟地址不是一回事

这就是最适合学习日的节奏。

---

## 4. 当前 day29 目录里已经给你的东西

这次 day29 不只是计划文档，而是已经补成了一个可执行的学习包：

- `driver/day29_edu_dma.c`
  - 最小 DMA coherent 驱动骨架
- `tools/day29_edu_dma_tool.c`
  - guest 侧用户态验证工具
- `guest/init.day29`
  - guest 启动后自动跑验证
- `scripts/*.sh`
  - 检查、构建、打包 rootfs、运行 QEMU、提取 records
- `docs/*.md`
  - 原理、寄存器、数据流、风险、排障

也就是说，今天不是停留在“讲计划”，而是已经把 Day29 的实验框架搭起来了。

---

## 5. 推荐执行顺序

先补执行位，再按下面顺序跑：

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

如果你已经准备好 `third_party/pciutils` 并且交叉工具链也完整，以上流程就会走到 guest 自动验证并把 records 落下来。

---

## 6. 最先看哪些证据

Day29 跑完后，先看：

```bash
cat records/${RUN_ID}/run-summary.md
sed -n '1,120p' records/${RUN_ID}/dma-verify.txt
sed -n '1,120p' records/${RUN_ID}/tool-info.txt
sed -n '1,160p' records/${RUN_ID}/dmesg-driver.txt
```

你要重点确认：

- `lspci-nn.txt` 中有 `1234:11e8`
- `tool-info.txt` 中能看到 `dma_handle` / `dma_bytes`
- `verify-result.txt` 中有 `verify_ok=1`
- `dma-verify.txt` / `dmesg-driver.txt` 中能看到 `verify ok`
- `dmesg-driver.txt` 中能看到 `dma_alloc_coherent ok`
- 没有 `DMA mapping error` / `BUG:` / `Oops:` / `Kernel panic`

---

## 7. 你今天真正要学会的表达

做完 Day29 后，面试/复盘里至少要能说清楚：

1. 为什么 EDU 默认要求关注 28-bit DMA mask
2. 为什么 `dma_alloc_coherent()` 返回的是两样东西：
   - CPU 访问用的虚拟地址
   - 设备访问用的 DMA 地址
3. 为什么不能把 `dma_virt` 直接写进设备 DMA 地址寄存器
4. 为什么 Day29 先做 coherent，再到 Day30 做 `mmap`
5. 为什么今天的验证是“设备内 buffer 做中转、往返各搬一次再比较”

这几句讲顺了，Day29 的价值就出来了。

---

## 8. 当日验收口径

### 必须满足

- `dma_set_mask_and_coherent()` 成功
- `dma_alloc_coherent()` 成功
- DMA 往返校验通过
- 无 crash、无 panic、无明显 DMA API 错误

### 建议额外补充

- 记录一次失败样本（例如长度越界 / rootfs 权限问题 / lspci 缺失）
- 记录 `dma_handle`、buffer 大小、verify 长度
- 为 day30 的 `mmap` 预留好输出材料

---


## 8.1 当前 records/day29-local-001 的验收结论

基于当前包内自带的 `records/day29-local-001`，Day29 已经达到本日通过口径：

- `lspci-nn.txt` 中可以看到 `1234:11e8`
- `dmesg-driver.txt` 中有 `probe success`、`dma mask set to 32 bits`、`dma_alloc_coherent ok`
- `dma-verify.txt` / `verify-result.txt` 中有 `verify ok` 与 `verify_ok=1`
- `verify_error=0`、`mismatch_index=-1`、`irq_delta=2`
- `serial.log` 中出现 `===DAY29:COMPLETE===`
- `run-summary.md` 中显示 `guest flow complete: yes`、`oops/dma-error/hung/panic found: no`

因此，这一版 Day29 可以按 **验收通过版** 理解，而不是“只差一点点”的半成品。

## 9. 和前后天的关系

- 前一天：day28 只是 W4 收口，不新增设备逻辑
- 今天：day29 把 DMA coherent 真正做通
- 后一天：day30 在今天的 coherent buffer 基础上继续做 `mmap`

所以 Day29 是 W5 的地基。


## 本轮已知修复

- 修复 guest rootfs 中 busybox applet 链接缺失导致的 `/init: line 8: mount: not found` 与 `Attempted to kill init!`。
- `run_qemu` 已加入 `-no-reboot` 与宿主侧超时 (`QEMU_TIMEOUT_SEC`)，避免异常时 QEMU 长时间挂住不退出。


> arm64 virt 关键点：QEMU EDU 默认仅支持 28-bit DMA；在本实验中默认通过 `-device edu,dma_mask=0xffffffff` 放宽到 32-bit，并在驱动默认值中收口为 32-bit，guest 自动化不再依赖运行时模块参数传值。
