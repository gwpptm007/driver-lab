# PACKAGE_NOTES

本包新增 `project-af-xdp-mini-forwarder`，用于从 lab 型 AF_XDP 练习过渡到项目型用户态 fast path。

## 不做的事

- 不强依赖测试机上立即产生真实流量；
- 不承诺 zero-copy 一定成功；
- 不做完整 L3/L4 路由，仅做 AF_XDP socket 层 mini forwarder；
- 不替代后续 `project-af-xdp-traffic-test`。

## 做的事

- XDP BPF redirect；
- XSKMAP 注册；
- UMEM/rings 初始化；
- drop/reflect 两种 smoke 模式；
- stats 与 review bundle。
