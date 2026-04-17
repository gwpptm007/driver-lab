# 04. 里程碑与风险

## 验收与里程碑

### M0：bootstrap ready

必须满足：
- 路径发现脚本工作正常
- host tools 探测报告可读
- 架构不是写死的
- env 可参数化

---

### M1：netdev skeleton ready

必须满足：
- 驱动能注册/注销 `net_device`
- `ip link` 可见接口
- `ndo_open/stop/start_xmit` 能被解释和观测
- 有最小统计项

---

### M2：skb path ready

必须满足：
- 已能解释 `skb` 基本字段
- 有软件 TX/RX 闭环
- 能证明"先理解处理对象，再理解搬运机制"这条主线成立

---

### M3：NAPI ready

必须满足：
- NAPI poll 真在跑
- 有 budget 相关统计
- 能对比纯中断与 NAPI 行为差异
- 观测项至少包含：irq、poll、rx packets、budget exhausted 次数

> 不建议用 `ping -f localhost` 这类与目标设备路径不一致的例子作为核心验收。
> 验收应围绕自研设备路径与自定义统计项完成。

---

### M4：ring / DMA ready

必须满足：
- ring / descriptor 设计清楚
- ownership 切换可解释
- RX replenishment 独立可讲
- ring 枯竭时有可观测行为
- streaming DMA 的 map/unmap 生命周期可解释

---

### M5：virtio-net compare ready

必须满足：
- 能把自研 ring 与 vring 对照讲清楚
- 明确哪些 northbound 资产被复用
- env/scripts 已基本参数化

---

### M6：ARM64 migrate ready

必须满足：
- ARM64 跑通
- x86_64 / ARM64 差异有记录
- build/run/records 可复现
- 有最终阶段总结文档

---

# 可观测性与调试

## 一、贯穿所有阶段的基本观测项

- netdev 注册/注销日志
- open/stop/start_xmit 计数
- RX/TX packet/byte 计数
- 错误计数
- debugfs 导出

---

## 二、Stage03 重点观测项

- irq count
- napi poll count
- budget exhausted count
- 每轮 poll 批处理包数
- 中断关闭/打开次数

---

## 三、Stage04 重点观测项

- ring head/tail
- avail/used 数量
- refill success/fail
- map/unmap 次数
- ring empty / full / stall 次数

---

## 四、建议工具

- `ip link`
- `ethtool -S`
- `cat /proc/interrupts`
- `cat /proc/softirqs`
- `perf top/record/report`
- `trace-cmd` / ftrace
- debugfs

---

## 五、注意事项

### 1. 不要让观测先于语义
先确定你要证明什么，再决定看哪个统计项。

### 2. 不要只看 host 工具输出
驱动内部自己的 stats/debugfs 同样重要。

### 3. 中断与 softirq 要分开看
网络驱动里，很多关键路径并不只在硬中断上下文内完成。

---

# 风险与阶段 Gate

## 主要风险

### 风险1：过早引入平台复杂度
表现：一开始就深陷交叉编译、QEMU 启动、rootfs 细节，导致 netdev 主线推进缓慢。

### 风险2：把 ring/DMA 放在 skb 之前
表现：理解顺序颠倒，后期对 RX/TX 语义理解不牢。

### 风险3：把 NAPI 当成普通 API 学
表现：只会写 `napi_schedule()`，却讲不清中断风暴、budget 与 re-enable 语义。

### 风险4：低估 RX replenishment
表现：会"收包"，但说不清 buffer 生命周期和 refill 跟不上时会怎样。

### 风险5：Stage05 混入太多工作
表现：`virtio-net` 精读、平台参数化、ARM64 迁移同时发生，节奏失控。

---

## Gate 建议

### Gate A：Stage02 结束后停下来复盘
如果此时还讲不清 `skb` 与 TX/RX 软件闭环，就不要进入 NAPI。

### Gate B：Stage03 结束后做中期检查
如果 NAPI 观测还不稳定，不要急着推进到 ring/DMA。

### Gate C：Stage04 结束后做阶段评审
只有在 ring、refill、DMA 生命周期都能讲清楚时，再进入 `virtio-net`。
