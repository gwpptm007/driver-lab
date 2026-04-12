# stage02_skb_path / START HERE

## 一句话理解 stage02
stage01 解决的是“最小网卡骨架有没有”；stage02 解决的是“网络包对象 `skb` 怎么经过 TX，再被软件方式送回 RX”。

## 先看什么
- `docs/01_STAGE_GOAL_AND_BOUNDARY.md`：阶段目标与边界
- `docs/02_SKB_LIFECYCLE_AND_DESIGN.md`：`skb` 的关键字段、clone/copy 差异
- `docs/03_SOFTWARE_LOOPBACK_PATH.md`：这套教学闭环的主路径

## 再看代码
- `driver/netdev_stage02.c`
- `tools/send_stage02_frame.c`
- `tools/recv_stage02_frame.c`
- `scripts/smoke.sh`

## 本阶段最该理解的 4 件事
1. `skb` 是网络驱动真正处理的核心对象
2. `ndo_start_xmit()` 拿到的是“已经进入驱动发送路径的 skb”
3. 软件环回不是“真实网卡收包”，而是**教学方式重新注入 RX**
4. NAPI / ring / DMA 还没上场，这一阶段先把对象语义吃透
