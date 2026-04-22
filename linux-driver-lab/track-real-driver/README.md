# track-real-driver

> stage14 之后的第一条正式 Track：真实 Linux 驱动源码与补丁线。

## 定位

在 `netdev/stage00~stage14` 中，你已经完成了教学型 soft NIC 主线：

- `net_device`
- `sk_buff`
- NAPI
- ring / descriptor
- multi-queue
- MSI-X / per-queue IRQ
- page_pool
- ethtool / control plane
- offload
- XDP 入口

从这里继续往后推进，如果还沿用 `stage15 stage16 ...` 命名，会逐渐把“课程式推进”和“真实工业驱动研究”混在一起。

因此从 stage14 之后，正式切换为：

- `track`
- `lab`
- `project`

## 当前建议的第一个 Lab

- `lab-virtio-net-source-dive/`

它的职责，是把你已经写过的教学驱动与 Linux 内核中的真实 `virtio_net` 驱动建立一一映射。
