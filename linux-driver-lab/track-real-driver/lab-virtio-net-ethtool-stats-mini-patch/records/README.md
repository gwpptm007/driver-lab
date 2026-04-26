# records

这里存放 ethtool stats mini patch 实验记录。

## 2026-04-25 测试结果

### 测试环境

| 项目 | 值 |
|------|-----|
| 测试方式 | QEMU + virtio-net-pci |
| Guest IP | 10.0.2.15 |
| 测试工具 | busybox ping |
| 记录时间 | 20260425_190551 |

### 测试结果

**Before (idle baseline)**
- RX packets: 1
- TX packets: 7

**After (10 ping + idle)**
- RX packets: 12 (delta +11)
- TX packets: 18 (delta +11)

**Ping 结果**
- 10 transmitted, 10 received, 0% loss
- RTT avg: 1.267ms

### 测试过程

```bash
# 1. 启动 QEMU (virtio-net 环境)
# 2. Guest 内配置网络
busybox ifconfig eth0 up
busybox ifconfig eth0 10.0.2.15 netmask 255.255.255.0

# 3. 记录 before
cat /sys/class/net/eth0/statistics/rx_packets > records/.../rx_before.txt
cat /sys/class/net/eth0/statistics/tx_packets > records/.../tx_before.txt

# 4. 执行 ping workload
busybox ping -c 10 10.0.2.2

# 5. 记录 after
cat /sys/class/net/eth0/statistics/rx_packets > records/.../rx_after.txt
cat /sys/class/net/eth0/statistics/tx_packets > records/.../tx_after.txt
```

### 记录目录

```
records/20260425_190551-ethtool-mini-patch/
├── BEFORE_AFTER.md        ← before/after 对照
├── SUMMARY.md             ← 测试总结
├── PATCH_POINT_NOTE.md    ← patch 点说明
├── PATCH_REVIEW_NOTE.md   ← review 备注
├── rx_before.txt          ← RX 初始值
├── rx_after.txt           ← RX 最终值
├── tx_before.txt          ← TX 初始值
└── tx_after.txt           ← TX 最终值
```

### 限制

- minimal rootfs 无 ethtool 命令
- 通过 `/sys/class/net/eth0/statistics/` 获取统计数据
- 可作为 runtime-observe 的 before/after 对照参考