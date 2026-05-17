# AF_XDP Project Portfolio

## 项目定位

AF_XDP track 是 Linux 原生用户态数据面作品线，目标是证明自己不仅理解内核 `net_device/NAPI/XDP` 和 DPDK PMD，也理解 AF_XDP 如何通过 XDP redirect、XSKMAP、UMEM、rings 建立用户态收发路径。

## 作品组成

### 1. lab-xdp-redirect-basics

- XDP BPF 程序；
- 用户态 loader；
- XDP attach/detach；
- PASS/DROP/REDIRECT 动作模型；
- per-action stats map。

### 2. lab-af-xdp-socket-rings

- XDP redirect 到 XSKMAP；
- AF_XDP socket 创建；
- UMEM 创建与注册；
- FILL/RX/TX/COMPLETION rings 初始化；
- 用户态 poll loop。

### 3. lab-af-xdp-zero-copy-vs-copy

- skb/copy baseline；
- native/copy probe；
- native/zero-copy probe；
- zero-copy 不支持时的 fallback 说明。

### 4. project-af-xdp-mini-forwarder

- drop 模式：验证 RX + frame recycle；
- reflect 模式：验证 RX→TX + completion；
- forwarder 统计解析；
- review bundle。

## 和简历项目的关系

可以把它和 DPDK track 放在一起，形成完整数据面能力：

```text
内核 netdev/XDP
DPDK 用户态 PMD fast path
AF_XDP Linux 原生用户态 fast path
```
