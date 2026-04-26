# 02_OBSERVE_CHAIN

## 本轮要观察的核心链条

建议你先把本轮实验理解成下面两条链，而不是很多散点。

### 链 1：RX 事件推进链
```text
queue event/callback
  -> napi schedule
  -> poll(budget)
  -> packet processing
  -> refill/recycle
```

### 链 2：运行节奏链
```text
idle
  -> ping
  -> iperf3
```

随着负载变化，观察：
- poll 是否变得更频繁
- RX 计数变化是否更明显
- 事件推进是否更容易在 trace/log 中被看到

## 本轮最重要的判断点

1. poll 在有负载时是否明显活跃
2. idle 与 ping 的差异是否足够形成 baseline 对照
3. ping 与 iperf3 的差异是否足够支持后续更深观测
4. 是否已经能把 `source-dive` 里的 Round2/Round3 说法转成运行期证据
