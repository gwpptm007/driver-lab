# Day29 本地执行手册

## 1. Day29 的目标

Day29 只验证一件事：

> **EDU 的 DMA 路径能不能在驱动里通过 DMA coherent API 跑通，并且完成一次可解释的数据一致性校验。**

这里不是为了追求复杂协议，而是为了把 DMA API 的最小闭环学扎实。

---

## 2. 执行前你脑子里要先有哪张图

今天真正的路径是：

```text
user tool
  -> ioctl(RUN_VERIFY)
      -> driver 填充 coherent buffer 的 src 区
      -> driver 把 dma_handle 写进 EDU DMA 源/目的寄存器
      -> EDU: RAM -> EDU 内部 buffer
      -> EDU: EDU 内部 buffer -> RAM
      -> driver 比较 src/dst 两段
      -> 返回 verify result
```

其中最重要的是区分：

- `dma_virt`：CPU 在内核里访问这块 buffer 用的虚拟地址
- `dma_handle`：设备 DMA 引擎访问这块 buffer 用的总线地址/IOVA

---

## 3. 环境准备

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day29
source env/day29.env
```

如果你的本机路径和默认值不一致：

```bash
cp env/local.example.env env/local.<yourname>.env
vim env/local.<yourname>.env
source env/local.<yourname>.env
```

---

## 4. third_party 准备

如果还没有 `pciutils`：

```bash
mkdir -p third_party
git clone https://github.com/pciutils/pciutils.git third_party/pciutils
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
```

---

## 5. 补执行位

zip 解压后执行位可能会丢，先统一补：

```bash
chmod +x scripts/*.sh
chmod +x guest/init.day29
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
```

---

## 6. 推荐完整执行顺序

```bash
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

---

## 7. guest 内会自动做什么

`guest/init.day29` 会自动完成：

1. 挂载 `proc/sys/devtmpfs`
2. 打印 `lspci -nn` 和 `lspci -vv -nn`
3. `insmod day29_edu_dma.ko`（不再依赖 guest 运行时传 `dma_mask_bits`）
4. 确认 `/dev/day29_edu0` 存在
5. 打印工具 `info`
6. 跑一次 `verify <len> <seed>`
7. 导出 `result`
8. 导出 `dmesg`
9. 打出 `===DAY29:COMPLETE===`

所以 Day29 的 records 重点其实都来自完整串口日志切块。

---

## 8. 成功时你会看到什么

### 8.1 `tool-info.txt`

应该能看到类似：

- `vendor=0x1234`
- `device=0x11e8`
- `dma_handle=...`
- `dma_bytes=4096`
- `dma_mask_bits=32`

### 8.2 `dma-verify.txt`

应该能看到：

- `verify_ok=1`
- `mismatch_index=-1`
- `verify_len=<你的长度>`

### 8.3 `dmesg-driver.txt`

应该能看到：

- `probe enter`
- `dma mask set to 32 bits`
- `dma_alloc_coherent ok`
- `verify start`
- `verify ok`

---

## 8.4 当前 records/day29-local-001 的通过样本

当前包内自带的 `records/day29-local-001` 已经包含一轮通过样本，关键结果如下：

- `verify_ok=1`
- `verify_error=0`
- `mismatch_index=-1`
- `irq_delta=2`
- `guest flow complete: yes`
- `oops/dma-error/hung/panic found: no`

这意味着你本地复跑时，不只是“理论上可通过”，而是已经有一份完整证据链能对照。

## 9. 今天最容易踩错的点

### 9.1 把 `dma_virt` 当成设备地址

这是 Day29 最核心的禁忌。

设备 DMA 寄存器里应该写的是 `dma_handle`，不是 `dma_virt`。

### 9.2 长度越界

当前驱动的 verify 设计是：

- coherent buffer 总大小：4KB
- `src` 区从 `0` 开始
- `dst` 区从 `2048` 开始

所以 **单次 verify 的最大长度是 2048 字节**。

### 9.3 只看工具输出，不看 dmesg

如果 verify 失败，只看用户态工具通常不够。

一定要同时看：

```bash
sed -n '1,200p' records/${RUN_ID}/dmesg-driver.txt
```

---

## 10. Day29 结束前最少要收哪些证据

- `lspci-nn.txt`
- `lspci-vv-nn.txt`
- `tool-info.txt`
- `dma-verify.txt`
- `verify-result.txt`
- `dmesg-driver.txt`
- `run-summary.md`

这些材料足够支撑 day30 再往上做 `mmap`。


## 本轮已知修复

- 修复 guest rootfs 中 busybox applet 链接缺失导致的 `/init: line 8: mount: not found` 与 `Attempted to kill init!`。
- `run_qemu` 已加入 `-no-reboot` 与宿主侧超时 (`QEMU_TIMEOUT_SEC`)，避免异常时 QEMU 长时间挂住不退出。


> arm64 virt 关键点：QEMU EDU 默认仅支持 28-bit DMA；在本实验中默认通过 `-device edu,dma_mask=0xffffffff` 放宽到 32-bit，并在驱动默认值中收口为 32-bit，guest 自动化不再依赖运行时模块参数传值。
