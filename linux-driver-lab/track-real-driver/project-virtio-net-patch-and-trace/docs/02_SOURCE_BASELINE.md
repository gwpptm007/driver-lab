# 02_SOURCE_BASELINE

## 这个项目依赖哪些前置 Lab

### 1. `lab-virtio-net-source-dive`
提供：
- `virtio_net` 分层理解
- TX/RX 主路径理解
- queue/NAPI/IRQ / feature/offload/XDP 边界理解

### 2. `lab-virtio-net-runtime-observe`
提供：
- 运行期 baseline
- idle / ping / iperf 的最初观测方式
- stats / log / trace 的目录组织方式

### 3. `lab-virtio-net-queue-poll-observe`
提供：
- queue / callback / napi schedule / poll 事件推进证据
- 适合后续 trace 收口的运行期观察入口

## 如何使用这些前置成果

不是把前面的内容复制进来，而是：

1. 用它们帮助选 patch 点
2. 用它们提供 before baseline
3. 用它们帮助解释 patch 后的运行期现象
