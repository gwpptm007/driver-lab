# ACCEPTANCE

## 验收标准

### 功能验收

- [ ] `make build-userspace` 编译 send_stage02_frame 和 recv_stage02_frame
- [ ] `make build-module` 编译 netdev_stage02.ko
- [ ] `sudo make load` 加载模块，dmesg 显示 `loaded, ifname=nds2`
- [ ] `sudo ./tools/send_stage02_frame nds2 hello` 发送帧
- [ ] `sudo ./tools/recv_stage02_frame nds2` 能收到环回的帧
- [ ] `cat /sys/kernel/debug/netdev_stage02/stats` 显示完整统计

---

## 统计验收

- [ ] TX 帧被记录（tx_packets > 0）
- [ ] RX 帧被环回（rx_packets > 0）
- [ ] `netif_rx_success` 与 `loop_injected` 匹配
- [ ] `copy_built + clone_built = loop_injected`

---

## 模式验收

- [ ] 默认 `loop_mode=copy` 时，`copy_built` 增加
- [ ] `loop_mode=clone` 时，`clone_built` 增加
