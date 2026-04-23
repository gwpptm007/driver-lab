# 27_PATCH_TRACING_PRIORITY_PLAN

> 这是一份基于当前 `lab-virtio-net-source-dive` 状态整理出来的后续优先级计划。  
> 原则：先选 **低风险、易验证、能体现理解** 的点，再逐渐进入更重的主路径实验。

## Priority 1：ethtool / stats 相关小改动

### 为什么排第一
- 与 `stage12_ethtool_control_plane` 连续性最强
- 风险低
- 结果容易验证
- 很适合作为“第一次真实驱动 patch”

### 可以做的类型
- 补/增强一个统计项理解与输出
- 梳理并增强某个 capability 的展示
- 做一版小型对照说明文档

### 验证方式
- ethtool 查询
- 驱动日志/统计变化
- 文档对照

---

## Priority 2：tracing / 运行期观测增强

### 为什么排第二
- 不直接动主路径语义
- 可以验证 Round2 / Round3 的阅读结论
- 很适合把“纸面理解”变成“运行期证据”

### 可以做的类型
- 增加 `trace_virtio_net_basic.sh` 的观测点
- 做 queue / poll / callback 的最小 trace 组合
- 输出一版 trace 与源码映射记录

### 验证方式
- trace 日志
- 路径图和运行期证据的一致性
- 评审时可展示

---

## Priority 3：queue / poll 观测型小实验

### 为什么排第三
- 和当前专题主线连续性强
- 比直接改 TX/RX 语义更稳
- 能进一步验证事件推进模型

### 可以做的类型
- 补一个 queue / poll 相关的轻量观察点
- 做一版 callback -> napi schedule -> poll 的观测记录
- 形成“事件推进图 + 运行期证据”组合材料

---

## 当前不建议优先做的方向

### 1. 一开始就改重的主路径语义
原因：
- 风险高
- 验证复杂
- 容易把当前专题从“理解/映射/观测”拖进过重的实现细节

### 2. 一开始就把 virtio front-end/back-end/QEMU/vhost 全链条一起拉进来
原因：
- 范围过大
- 会冲淡当前专题闭环

### 3. 并行读多个复杂真实 NIC
原因：
- 当前阶段更需要“把一个专题做扎实”
- 不是同时铺很多入口

---

## 建议的执行顺序

1. **先做 P1：ethtool / stats 小 patch**
2. **再做 P2：trace / 观测增强**
3. **最后做 P3：queue / poll 观测型实验**

这条顺序最符合当前仓库的实际成熟度。
