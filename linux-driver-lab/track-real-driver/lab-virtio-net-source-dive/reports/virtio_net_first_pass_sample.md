# virtio_net first pass sample

> 这是一份“第一遍读完后的样例总结结构”。

## 我已经搞清楚的
- `virtio_net` 不只是一个 `net_device` 驱动，它还带着 virtio 设备模型和 feature negotiation 背景
- queue / NAPI / callback / poll 必须连着看
- TX/RX 必须做路径图，否则容易把代码看散

## 我还没有完全搞清楚的
- 某些 helper 的边界
- 某些 feature bit 对路径的精确影响
- 某些 TX reclaim / RX refill 细节

## 我和自己项目之间最重要的映射
- `stage03` 对应 NAPI/poll 思想
- `stage09~stage10` 对应 queue / interrupt 组织
- `stage11` 对应 RX 资源生命周期
- `stage12~stage14` 对应控制面 / offload / XDP 边界

## 下一步最值得做的
- 追 2~3 个关键函数到更深一层
- 做一次最小 trace
- 选一个小点作为后续 patch / tracing 候选点
