# day30 排障手册

## 1. `mmap()` 返回 `EINVAL`

### 现象
用户态工具在 `mmap()` 阶段失败。

### 优先看什么
- `mmap` 传入长度是不是 4096
- `offset` 是否为 0
- 驱动 `day30_mmap()` 是否只允许 full-page mapping

### 当前设计约束
day30 明确只允许：

- `offset == 0`
- `length == PAGE_ALIGN(dma_bytes)`，当前就是 4096

所以：
- 传 offset=1 page，应该失败
- 非法长度测试不要用 2048；在 4KB 页大小下它会被内核扩成 4096，反而可能变成合法整页映射
- 更稳的非法长度样例是 4097 或 map_bytes+1

---

## 2. `/dev/day30_edu0` 不存在

### 现象
guest 里工具无法打开设备节点。

### 排查
- 看 `lspci -nn` 是否有 `1234:11e8`
- 看 dmesg 是否有 `probe success`
- 看 `/sys/class/day30_edu/day30_edu0/dev` 是否存在
- 看 guest `/init` 是否执行了 `mknod`

---

## 3. `run_ok=1` 但 `verify_ok=0`

### 含义
说明：
- 驱动侧两段 DMA 已完成
- 但用户态在映射区比对 src/dst 失败

### 排查方向
- 用户态是否在 ioctl 前正确填了 src、清了 dst
- `len` 是否超过 `max_verify_len`
- `src_off/dst_off` 是否用错
- mismatch 字节是什么

---

## 4. 非法映射测试“居然成功”

### 现象
`mmap-invalid-len` 或 `mmap-invalid-offset` 没被拒绝。

### 先别急着下结论
- `invalid-offset` 如果成功，通常就是驱动边界真的没收住；
- `invalid-len` 则还要先确认测试样例本身是不是有效。

例如：在 4KB 页系统里传 `2048`，VMA 最终长度仍然会是 `4096`，这和 day30
当前允许的整页映射完全一致，因此“unexpected success”更可能是**用例设计问题**，
而不是驱动没有检查 `len != map_bytes`。

### 推荐复测
把非法长度改成 `4097` 或 `map_bytes+1`，再看驱动是否打印：
- `mmap rejected: len=... expected=4096`

---

## 5. guest 没有正常结束

### 常见原因
- `/init` 用到的 busybox applet 没有链接
- `insmod` 失败
- 工具执行失败后 shell 提前退出

### 排查
看：
- `serial.log`
- `qemu.stderr.log`

重点搜：
- `mount: not found`
- `insmod: can't insert`
- `Kernel panic`
- `===DAY30:COMPLETE===`

---

## 6. 如何判断你现在卡在哪一层

### 构建层
- `make check`
- `make build-tools`
- `make module`

### rootfs 层
- `sudo -E make rootfs`

### QEMU 运行层
- `sudo -E make run`

### guest 功能层
- `records/<RUN_ID>/mmap-verify.txt`
- `records/<RUN_ID>/run-result.txt`
- `records/<RUN_ID>/dmesg-driver.txt`
