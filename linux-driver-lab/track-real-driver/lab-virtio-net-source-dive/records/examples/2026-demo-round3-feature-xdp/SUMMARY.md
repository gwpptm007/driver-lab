# SUMMARY

## 本轮目标
把 feature / offload / ethtool / XDP 入口从“知道有”推进到“知道挂在哪、为什么挂在这里”。

## 当前建议的拆法

### 1. feature negotiation
先把能力协商看作“驱动和设备的契约建立”。

### 2. offload
不要背完所有 capability，先抓住：
- checksum
- GSO/GRO
- queue/channels 相关控制能力

### 3. ethtool
重点看：
- stats
- channels
- capability 查询
- 控制面接口

### 4. XDP
重点看：
- attach 点
- RX fast path 边界
- 与常规 skb path 的关系

## 当前和自己项目的映射重点
- `stage12_ethtool_control_plane`
- `stage13_offload_basics`
- `stage14_xdp_basics`
