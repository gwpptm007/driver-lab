# 07_FEATURES_ETHTOOL_XDP

## 目标

把控制面与能力面补齐，不只盯 TX/RX 主路径。

## 这轮要回答的问题

1. feature negotiation 在哪里做？
2. 哪些 offload / capability 是通过 feature bits 打开的？
3. ethtool 相关接口在哪里接入？
4. XDP 在 `virtio_net` 里的入口在哪里？
5. 哪些能力是你当前 `stage14_xdp_basics` 还只是教学简化？

## 推荐拆法

### A. features / offload
- checksum
- GSO/GRO
- 其他与数据路径行为相关的 capability

### B. ethtool
- stats
- channels / queue 相关能力
- 驱动信息 / feature 展示

### C. XDP
- attach/detach 的入口
- RX 主路径里 XDP 与 skb path 的边界
- 真实驱动与教学驱动的语义差异

## 对照你自己的 stage

- `stage12_ethtool_control_plane`
- `stage13_offload_basics`
- `stage14_xdp_basics`

## 本篇交付建议

- 一份“features -> 行为变化”的表
- 一份 ethtool 入口索引
- 一份 XDP 入口与限制说明
