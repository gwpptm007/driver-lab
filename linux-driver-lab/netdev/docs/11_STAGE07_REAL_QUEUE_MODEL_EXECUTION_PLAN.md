# 11_STAGE07_REAL_QUEUE_MODEL_EXECUTION_PLAN

## 文档定位

这份文档把 `stage07_real_queue_model` 的方向，进一步落成可执行计划。

它不只是说“下一步做 stage07”，而是把：

- 为什么 stage07 值得做
- 目录怎么拆
- 数据结构怎么想
- 核心职责怎么划
- 里程碑怎么定
- 验收怎么判

都固定下来。

## 一、为什么 stage07 是当前最值的下一步

因为当前 netdev 主线已经完成：

- `stage03`：NAPI 语义
- `stage04`：ring / DMA / RX refill
- `stage05`：`virtio-net` 对照与平台参数化
- `stage06`：跨平台迁移收口框架

继续往前最该做的，不是再横向扩题，而是：

> 把现有教学型驱动模型推进成更接近真实队列驱动的组织方式。

## 二、推荐阶段名

推荐固定为：

- `stage07_real_queue_model`

因为当前最核心的是 queue model，而不是后端本身。

## 三、目录与职责

建议目录：

- `docs/`：设计、映射、验收、学习文档
- `driver/`：主驱动代码与 ring/queue 框架
- `include/`：兼容层或共享定义
- `scripts/`：build/run/smoke/stats/trace
- `tools/`：最小测试工具
- `output/`：当前输出
- `records/`：证据归档
- `reports/`：评审结果

## 四、最关键的四件事

### 1. 显式 index 模型
必须建立：
- TX submit index
- TX complete index
- RX post index
- RX consume index

### 2. 明确 queue helper 职责
必须拆清：
- submit
- complete
- post
- consume
- refill

### 3. 明确 notify / irq / napi 分工
必须做到：
- irq 只负责触发
- poll 负责批处理
- helper 负责状态推进

### 4. 建立 `virtio-net` 映射
要能解释：
- avail/used 思想
- kick/interrupt 思想
- buffer posting 思想

## 五、建议推进顺序

### R1：目录和数据结构定型
### R2：TX/RX 生命周期 first pass
### R3：notify/irq/napi 稳定
### R4：stats/trace/report 完整

## 六、最终通过标准

stage07 完成的标志不是“能发一个包”，而是：

- 生命周期正确
- 统计可见
- 行为可证明
- 与 stage04 差异可解释
- 与 `virtio-net` 映射站得住

## 七、一句话总结

> stage07 是 netdev 主线从“教学型实现”迈向“准真实驱动模型”的关键一跳。
