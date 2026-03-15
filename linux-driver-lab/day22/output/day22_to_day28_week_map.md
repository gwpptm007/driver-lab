# day22 到 day28 的承接关系

- day22：把 `ivshmem-doorbell` 带进 guest，并产出 `lspci` 证据
- day23：写 `pci_driver` 骨架，完成 `probe/remove` 资源框架
- day24：完成 BAR 映射与基本 MMIO 验证
- day25：完成 MSI / 中断计数验证
- day26：补用户态工具与清晰错误码
- day27：做 remove 对称释放与 200 次循环
- day28：整理 README、证据与复现说明

也就是说，day22 是 W4 的平台 bring-up 日。
