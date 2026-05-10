# 01_GOAL_AND_SCOPE

## 目标

把前面三个 AF_XDP lab 的能力组合成一个项目型 mini forwarder。

前置能力：

```text
lab-xdp-redirect-basics      -> XDP attach / action / redirect model
lab-af-xdp-socket-rings      -> UMEM / XSK socket / rings
lab-af-xdp-zero-copy-vs-copy -> copy / zero-copy mode 边界
```

本项目输出：

```text
XDP redirect -> AF_XDP RX ring -> userspace policy -> recycle/drop/reflect -> stats
```

## 范围

第一版只做：

- 单队列 AF_XDP socket；
- 单网卡 queue 0；
- `drop` 与 `reflect` 两种策略；
- `skb + copy` 默认路径；
- `native/zero-copy` 参数保留；
- stats/review bundle。

后续可扩展：

- 多队列；
- 双网卡 forward；
- UDP-only 过滤；
- checksum rewrite；
- busy poll / need_wakeup 对比；
- zero-copy 平台验证。
