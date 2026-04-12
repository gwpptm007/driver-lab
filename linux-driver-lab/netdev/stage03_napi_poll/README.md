# stage03_napi_poll

## 阶段定位
NAPI / poll / 中断抑制与可观测性。

## 这一阶段要解决什么问题
stage02 已经证明：
- `ndo_start_xmit()` 可以收到真实发送的帧
- 驱动可以把一帧重新送回协议栈 RX 路径
- 用户态 sender / receiver 可以形成教学型 software loopback

但 stage02 仍然是“**每来一帧就立刻处理一帧**”的思路。它能讲清 `skb`，却讲不清下面几个真正属于网络驱动的问题：

- 为什么高频收包不能每包都直接走中断 / 立刻注入
- 为什么 RX 路径需要一个“先缓存、后批处理”的模型
- `budget` 到底限制了什么
- poll 结束时为什么要重新开中断
- 同样是软件环回，`direct` 和 `napi` 两种模式到底差在哪

stage03 的目标就是把这层语义补上：

> **把 stage02 的“立刻注入 RX”改成“先进入 pending queue，再由 NAPI poll 批量处理”。**

## 当前阶段边界
这一阶段**仍然不引入真实 descriptor ring / DMA / RX replenishment**。

也就是说：
- 先用 `sk_buff_head pending_rxq` 代替真实硬件 ring
- 先用“软件触发 irq → napi_schedule → poll drain”代替真实设备中断
- 先把 NAPI 的批处理、budget、complete 语义讲明白
- stage04 再讨论 ring / DMA / ownership / refill

## 本阶段核心产出
- 一个教学型 NAPI 驱动：`driver/netdev_stage03.c`
- 两种 RX 模式：
  - `rx_mode=direct`：沿用 stage02 的“立即注入”路径
  - `rx_mode=napi`：进入 pending queue，再由 poll drain
- 两个用户态工具：
  - `send_stage03_frame`：支持 burst 发送
  - `recv_stage03_frame`：支持按帧数 / 超时接收
- build/load/smoke/report 脚本
- debugfs 统计项，能看到：
  - irq raised / masked / unmasked
  - napi schedule / poll / complete / budget exhausted
  - pending queue depth / peak / drained
  - direct vs napi 注入次数

## 推荐阅读顺序
1. `START_HERE.md`
2. `docs/01_STAGE_GOAL_AND_BOUNDARY.md`
3. `docs/02_NAPI_MOTIVATION_AND_MODEL.md`
4. `docs/03_PENDING_QUEUE_AND_POLL_PATH.md`
5. `docs/04_DIRECT_VS_NAPI_MODE.md`
6. `driver/netdev_stage03.c`
7. `docs/05_TEST_AND_ACCEPTANCE.md`

## 建议执行顺序
```bash
cd linux-driver-lab/netdev/stage03_napi_poll
make report
make build-userspace
make build-module
sudo make load
sudo make smoke
sudo make unload
```
