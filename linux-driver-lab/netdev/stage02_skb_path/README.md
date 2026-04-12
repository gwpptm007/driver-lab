# stage02_skb_path

## 阶段定位
`skb` 路径与软件 TX/RX 闭环。

## 这一阶段要解决什么问题
stage01 已经证明：
- 最小 `net_device` 能注册/注销
- `ndo_open/stop/start_xmit` 能被真实触发
- 用户态发一帧，驱动能在 `start_xmit()` 里观察到它

stage02 往前走一步，不再满足于“看到包到了 TX 入口”，而是要把下面这件事做成闭环：

> **把一帧从 TX 路径拿到手，再以软件教学方式重新注入 RX 路径，形成最小的软件环回。**

这样做的目的不是模拟真实网卡，而是先把 `skb` 作为网络驱动核心数据对象真正吃透。

## 当前阶段边界
这一阶段**先不引入 NAPI、ring、DMA、RX replenishment**。

也就是说：
- 先理解 `skb` 是什么
- 先理解 TX / RX 在驱动里的入口出口
- 先理解 `netif_rx()` 把包重新交还给协议栈意味着什么
- 再到 stage03/stage04 讨论批处理、descriptor、DMA 搬运

## 本阶段核心产出
- 一个教学型 `net_device` 模块：`driver/netdev_stage02.c`
- 软件环回 TX/RX 闭环
- 两个用户态工具：
  - `send_stage02_frame`：发送自定义二层帧
  - `recv_stage02_frame`：接收软件回环进来的帧
- build/load/smoke/report 脚本
- 解释 `skb clone/copy` 的文档与调试统计

## 推荐阅读顺序
1. `START_HERE.md`
2. `docs/01_STAGE_GOAL_AND_BOUNDARY.md`
3. `docs/02_SKB_LIFECYCLE_AND_DESIGN.md`
4. `docs/03_SOFTWARE_LOOPBACK_PATH.md`
5. `driver/netdev_stage02.c`
6. `docs/04_TEST_AND_ACCEPTANCE.md`

## 建议执行顺序
```bash
cd linux-driver-lab/netdev/stage02_skb_path
make report
make build-userspace
make build-module
sudo make load
sudo make smoke
sudo make unload
```
