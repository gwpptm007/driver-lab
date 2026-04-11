# day22 run summary

- run-id: day22-local-001
- record-dir: /home/wq7/workspace/driver-lab/linux-driver-lab/day22/records/day22-local-001

- ivshmem 设备可见：否（未在 lspci -nn 中发现 1af4:1110）
- lspci -vv 归档：否
- dmesg PCI 归档：否
- sysfs PCI 设备归档：否
- config 空间样本归档：否

建议下一步：
1. 打开 lspci-vv-nn.txt，看 BAR 信息是否已经可见。
2. day23 基于该 BDF 和设备 ID 开始写 pci_driver 骨架。
