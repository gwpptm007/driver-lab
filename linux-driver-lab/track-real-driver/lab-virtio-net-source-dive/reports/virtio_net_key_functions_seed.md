# virtio_net key functions seed

> 这不是最终答案，而是一份“第一次阅读时你应该主动去找的函数/入口类型”。

## 第一类：注册与设备模型
- driver registration
- probe
- remove
- feature negotiation 入口

## 第二类：netdev 生命周期
- netdev alloc/init/register
- netdev ops
- ethtool ops

## 第三类：队列与 NAPI
- queue pair 初始化
- NAPI 初始化
- callback / poll

## 第四类：TX
- `ndo_start_xmit`
- kick/notify
- reclaim/completion

## 第五类：RX
- refill
- poll
- skb build
- GRO/checksum/XDP 边界

## 第六类：高级能力
- stats
- channels / queue config
- feature/offload
- XDP
