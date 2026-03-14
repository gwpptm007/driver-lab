# Day16 APPLY_ROUND1_04

## 1. round1 的目标

先去掉最明确、最安全的一批无关大块：

- 网络驱动族
- 声音栈
- 部分 USB host / USB storage / USB HID

同时不破坏：

- boot 链
- demo 链
- tracing / function_graph / perf 内核侧链

---

## 2. 执行位置

全部在 **宿主机** 执行。

先进入：

```bash
cd ~/workspace/driver-lab/kernel-src/linux-5.15.10
```

并定义：

```bash
export KERNEL_DIR=~/workspace/driver-lab/kernel-src/linux-5.15.10
export KERNEL_SRC=$KERNEL_DIR/src
export KERNEL_OUT=$KERNEL_DIR/build/arm64
export KERNEL_IMG=$KERNEL_DIR/output/arm64/Image
export DAY16_DIR=~/workspace/driver-lab/linux-driver-lab/day16
export CROSS_COMPILE=aarch64-linux-gnu-
```

---

## 3. round1 已实际关闭的项

```text
# CONFIG_THUNDER_NIC_PF is not set
# CONFIG_THUNDER_NIC_BGX is not set
# CONFIG_HNS3 is not set
# CONFIG_E1000 is not set
# CONFIG_E1000E is not set
# CONFIG_IGB is not set
# CONFIG_IGBVF is not set
# CONFIG_SKY2 is not set
# CONFIG_SOUND is not set
# CONFIG_USB_HID is not set
# CONFIG_USB_EHCI_HCD is not set
# CONFIG_USB_OHCI_HCD is not set
# CONFIG_USB_STORAGE is not set
```

---

## 4. round1 主链检查

round1 后仍保留：

- `CONFIG_DEBUG_FS=y`
- `CONFIG_TRACEPOINTS=y`
- `CONFIG_TRACING=y`
- `CONFIG_FTRACE=y`
- `CONFIG_FUNCTION_TRACER=y`
- `CONFIG_FUNCTION_GRAPH_TRACER=y`
- `CONFIG_PERF_EVENTS=y`

---

## 5. round1 已通过运行时验证

在 **guest shell** 验证得到：

```text
boot_ok=yes
tracing_ok=yes
function_graph_ok=yes
trace_smoke_ok=yes
insmod_ok=yes
snapshot_ok=yes
trigger_ok=yes
dmesg_warn=no
```
