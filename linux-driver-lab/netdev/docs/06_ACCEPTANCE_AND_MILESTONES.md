# 06. 验收与里程碑

## M0：bootstrap ready

### 必须满足
- 路径发现脚本工作正常
- host tools 探测报告可读
- 架构不是写死的
- env 可参数化

---

## M1：netdev skeleton ready

### 必须满足
- 驱动能注册/注销 `net_device`
- `ip link` 可见接口
- `ndo_open/stop/start_xmit` 能被解释和观测
- 有最小统计项

---

## M2：skb path ready

### 必须满足
- 已能解释 `skb` 基本字段
- 有软件 TX/RX 闭环
- 能证明“先理解处理对象，再理解搬运机制”这条主线成立

---

## M3：NAPI ready

### 必须满足
- NAPI poll 真在跑
- 有 budget 相关统计
- 能对比纯中断与 NAPI 行为差异
- 观测项至少包含：irq、poll、rx packets、budget exhausted 次数

> 不建议用 `ping -f localhost` 这类与目标设备路径不一致的例子作为核心验收。
> 验收应围绕自研设备路径与自定义统计项完成。

---

## M4：ring / DMA ready

### 必须满足
- ring / descriptor 设计清楚
- ownership 切换可解释
- RX replenishment 独立可讲
- ring 枯竭时有可观测行为
- streaming DMA 的 map/unmap 生命周期可解释

---

## M5：virtio-net compare ready

### 必须满足
- 能把自研 ring 与 vring 对照讲清楚
- 明确哪些 northbound 资产被复用
- env/scripts 已基本参数化

---

## M6：ARM64 migrate ready

### 必须满足
- ARM64 跑通
- x86_64 / ARM64 差异有记录
- build/run/records 可复现
- 有最终阶段总结文档
