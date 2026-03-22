# day28 W4 最终阶段总结

## 1. 结论

基于当前上传仓库中的真实 records，W4 已完成：设备可见、驱动接住设备、MMIO 读写、MSI 中断、用户态工具、200 次循环稳定性这条完整学习链。

> **W4 通过。**

## 2. 各天结果

### day22

- 结论：核心通过
- 说明：旧版 `run-summary.md` 存在误判，应以 `serial.log` 原始 marker 为准
- 证据：serial.log 中出现 ivshmem 设备 ID 1af4:1110
- 证据：serial.log 中出现 lspci -vv marker
- 证据：serial.log 中出现 dmesg PCI marker
- 证据：serial.log 中出现 COMPLETE marker

### day23

- RUN_ID: day23-local-001
- insmod 成功：yes
- probe 成功：yes
- BAR0 信息：yes
- BAR2 信息：yes
- rmmod 成功：yes
- guest 流程完成：yes
- serial.log
- dmesg-probe.txt
- dmesg-remove.txt
- lspci-vv-nn.txt

### day24

- RUN_ID: day24-local-001
- insmod 成功：yes
- probe 成功：yes
- mmio info：yes
- mmio write：yes
- mmio read after：yes
- shm write：yes
- shm read：yes
- rmmod 成功：yes
- guest 流程完成：yes
- serial.log
- dmesg-driver.txt
- mmio-info.txt
- shm-read.txt
- lspci-vv-nn.txt

### day25

- edu device visible: yes
- probe success: yes
- BAR0 logged: yes
- driver irq_count grows: yes
- /proc/interrupts entry exists: yes
- /proc/interrupts count grows: yes
- guest flow complete: yes

### day26

- edu device visible: yes
- probe success: yes
- ioctl info works: yes
- read state works: yes
- driver irq_count grows: yes
- /proc/interrupts entry exists: yes
- /proc/interrupts count grows: yes
- invalid trigger error clear: yes
- guest flow complete: yes

### day27

- run id: day27-local-001
- edu device visible: yes
- probe logged: yes
- remove logged: yes
- irq handler logged: yes
- loop target met (200): yes
- guest flow complete: yes
- oops/hung/panic found: no
- probe success count: 394
- remove leave count: 395
- irq handler count: 394
- loop pass: 200
- loop fail: 0

## 3. W5 输入

- 当前已经有稳定的 PCIe/QEMU 复现实验环境
- 当前已经有 driver + tool + guest + records 的固定目录范式
- 下一步可以自然进入 DMA / mmap / bench / perf / ftrace
