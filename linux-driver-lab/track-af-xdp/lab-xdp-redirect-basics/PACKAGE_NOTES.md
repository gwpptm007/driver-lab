# PACKAGE_NOTES

本包在 `track-af-xdp` 下新增第一站可执行实验：`lab-xdp-redirect-basics`。

## 注意

- 这不是完整 AF_XDP socket 实验；
- 当前阶段只验证 XDP attach/action/stats/XSKMAP 模型；
- 真正的 UMEM/rings/socket 会在下一站 `lab-af-xdp-socket-rings` 实现；
- 如果测试网卡当前被 DPDK 绑定到 `uio_pci_generic`，要先恢复到内核驱动。
