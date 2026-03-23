# day30 本地运行手册

## 1. 环境准备

进入 `day30/`：

```bash
cd ~/workspace/driver-lab/linux-driver-lab/day30
source env/day30.env
# 如本机路径与默认值不同，再额外 source 你自己的 local.xxx.env
```

如需要 guest 侧 `lspci`，先准备 `pciutils`：

```bash
mkdir -p third_party
git clone https://github.com/pciutils/pciutils.git third_party/pciutils
chmod +x third_party/pciutils/lib/configure
chmod +x third_party/pciutils/configure 2>/dev/null || true
```

---

## 2. 推荐执行顺序

```bash
make check
make kernel-module-tree
make build-tools
make module
sudo -E make rootfs
make backend
sudo -E make run
```

不建议一开始就只盯着 `make run`。  
先让 `check / build-tools / module` 过掉，能把问题更早暴露在构建层。

---

## 3. 本地排查优先顺序

### 第一层：环境是否齐
看：

```bash
make check
```

### 第二层：工具和模块是否真的重建
看：

```bash
make build-tools
make module
ls -l workdir/tools/aarch64/day30_edu_mmap_tool
ls -l driver/day30_edu_mmap.ko
```

### 第三层：rootfs 里是否带进了正确产物
看：

```bash
sudo -E make rootfs
ls -l workdir/rootfs
```

### 第四层：QEMU 是否跑完
看：

```bash
tail -n 120 workdir/runs/${RUN_ID}/serial.log
cat workdir/runs/${RUN_ID}/qemu.stderr.log
```

---

## 4. 运行后优先看哪些文件

```bash
cat records/${RUN_ID}/run-summary.md
cat records/${RUN_ID}/mmap-verify.txt
cat records/${RUN_ID}/run-result.txt
cat records/${RUN_ID}/invalid-mmap-len.txt
cat records/${RUN_ID}/invalid-mmap-offset.txt
cat records/${RUN_ID}/dmesg-driver.txt
```

---

## 5. day30 成功时最应该看到的关键词

### mmap verify
- `verify_ok=1`
- `mismatch_index=-1`

### driver result
- `run_ok=1`
- `run_error=0`

### invalid path
- `expected failure: invalid length rejected`
- `expected failure: invalid offset rejected`

### serial complete
- `===DAY30:COMPLETE===`

---

## 6. 常见失败场景

### `mmap()` 返回 `EINVAL`
先看：
- 传入长度是不是整页
- offset 是否为 0
- 驱动 `mmap` 是否只允许 full-page mapping

### `/dev/day30_edu0` 不存在
先看：
- 驱动 probe 是否成功
- `sys/class/day30_edu/day30_edu0/dev` 是否存在
- `mknod` 是否执行成功

### `run_ok=1` 但 `verify_ok=0`
说明：
- DMA 两段可能已经完成
- 但用户态 `src/dst` 比较失败
- 优先看 `mismatch_index / expected / actual`

### guest 直接 panic
优先检查：
- `/init` 里用到的 busybox applet 是否都链接好了
- `insmod` 是否成功
- 是否走到了 `poweroff -f`


## 补充说明：为什么非法长度建议用 4097

因为 mmap 的 length 会按页向上取整。在 4KB 页环境里，2048 最终会被扩成 4096，可能误变成合法整页映射。为了稳定命中 day30 的非法长度拒绝路径，建议使用 4097 或 `map_bytes + 1`。
