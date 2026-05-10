# PACKAGE_NOTES

本包从 `track-af-xdp/lab-xdp-redirect-basics` 继续推进到第二站：`lab-af-xdp-socket-rings`。

## 已知边界

- 第一站 `lab-xdp-redirect-basics` 目前只达到 `PASS_BASIC_ATTACH`，DROP/REDIRECT 动作留到后续补测。
- 当前第二站优先使用 `skb + copy`，对 VMware/vmxnet3 更稳。
- `zero-copy` 需要 NIC/驱动支持，后续 `lab-af-xdp-zero-copy-vs-copy` 再系统比较。
