# PACKAGE_NOTES

本包新增 `lab-af-xdp-zero-copy-vs-copy`。

注意：

- `zero-copy` 探测脚本允许失败，并会记录 `rc`；
- 对 VMware/vmxnet3 来说，zero-copy 不支持是合理结果；
- 本实验的目的不是性能压测，而是建立 AF_XDP 模式选择和 fallback 判断能力。
