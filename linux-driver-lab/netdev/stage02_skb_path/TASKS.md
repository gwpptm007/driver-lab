# stage02_skb_path / TASKS

## 目标
围绕 `skb` 建立软件 TX/RX 闭环，形成最小的软件教学环回。

## 必做项
- [x] 落地 stage02 README / docs / env / scripts 骨架
- [x] 实现教学型 `net_device` 模块：`netdev_stage02.c`
- [x] 在 `ndo_start_xmit()` 中完成：TX 统计 → 构造 RX skb → `netif_rx()` 注入
- [x] 支持 `loop_mode=copy|clone`
- [x] 提供 sender / receiver 用户态工具
- [x] 提供最小 smoke 脚本
- [x] 提供 debugfs 统计导出

## 本阶段不做
- [ ] NAPI / poll / budget
- [ ] ring / descriptor
- [ ] DMA map/unmap
- [ ] RX replenishment
- [ ] virtio-net 对照

## 阶段验收
- `ip link` 能看到 TX/RX 统计增长
- `recv_stage02_frame` 能收到软件环回的帧
- debugfs 能看到 copy/clone 模式、环回注入计数、最近一次 proto/len
- 能清楚解释：为什么这一阶段先不谈 NAPI/ring/DMA
