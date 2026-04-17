# 05. stage06 收口与 stage07 执行计划

## stage06 收口与 stage07 方向

### 文档定位

这份文档用于承接当前 netdev 主线的"下一步"。

在当前完整项目基线下，最合理的路线不是：
- 回到 `foundation` 继续加 day
- 立刻跳去 DPDK/XDP
- 直接做多队列/大杂烩

而是：

1. 先把 `stage06_arm64_migration` 真正收口
2. 再新开 `stage07`，把教学型 netdev 推到更接近真实队列驱动的模型

---

## 一、为什么下一步先做 stage06 收口

因为当前第二阶段里：

- `stage03` 已把 NAPI 语义讲清
- `stage04` 已把 ring / DMA / refill 做成代码高点
- `stage05` 已完成 `virtio-net` 对照与平台参数化准备
- `stage06` 已形成迁移框架和 ARM64 smoke 结果

但 stage06 仍然更像：

> 迁移方法已经建立，但还没有完全沉淀成"别人拿去就能复现"的阶段包。

所以先做 stage06 收口，回报最高、风险最低。

---

## 二、stage06 收口要完成什么

### 1. 清理个人路径依赖
把脚本中所有固定作者路径去掉。

### 2. 统一 env / build / run / smoke 契约
让 stage06 不再依赖人工记忆命令。

### 3. 把迁移逻辑讲透
明确：
- 什么保持不变
- 什么是平台差异
- 什么由兼容层承担

### 4. 补齐验收闭环
让 host / qemu-x86_64 / qemu-arm64 都能被正式验收。

---

## 三、为什么 stage07 才是下一阶段功能高点

因为真正能把项目层次再拉高的一步，是：

> 把现在的教学型 ring / DMA / NAPI 模型，继续推进成更接近真实 NIC / virtio 的 queue / completion / notify 组织方式。

这一步不该塞进 stage06，因为那会混淆阶段边界。

---

## 四、stage07 建议定位

推荐名称：

- `stage07_real_queue_model`

也可以接受：

- `stage07_backend_integration`

但更推荐第一个，因为当前最重要的不是"接什么后端"，而是把驱动模型做得更像真实队列驱动。

---

## 五、stage07 最应该解决什么

### 1. 显式 producer / consumer index
建立：
- TX submit index
- TX complete index
- RX post index
- RX consume index

### 2. 建立 notify / completion 语义
把：
- 提交
- doorbell
- 完成
- irq
- napi
- refill

关系做清楚。

### 3. 明确 queue 生命周期
拆清：
- submit
- complete
- post
- consume
- refill

### 4. 与 virtio-net 做结构映射
不是重写 virtio-net，而是把：
- virtqueue 思想
- used/avail 语义
- kick/interrupt 思想

映射到自己的教学驱动。

---

## 六、下一步推荐顺序

### Phase A
先完成 `stage06` 收口。

### Phase B
开 `stage07_real_queue_model`。

### Phase C
补 netdev 统计与观测固定资产：
- stats
- trace
- compare
- 报告

---

# stage07  execution plan

## 文档定位

这份文档把 `stage07_real_queue_model` 的方向，进一步落成可执行计划。

它不只是说"下一步做 stage07"，而是把：

- 为什么 stage07 值得做
- 目录怎么拆
- 数据结构怎么想
- 核心职责怎么划
- 里程碑怎么定
- 验收怎么判

都固定下来。

---

## 一、为什么 stage07 是当前最值的下一步

因为当前 netdev 主线已经完成：

- `stage03`：NAPI 语义
- `stage04`：ring / DMA / RX refill
- `stage05`：`virtio-net` 对照与平台参数化
- `stage06`：跨平台迁移收口框架

继续往前最该做的，不是再横向扩题，而是：

> 把现有教学型驱动模型推进成更接近真实队列驱动的组织方式。

---

## 二、推荐阶段名

推荐固定为：

- `stage07_real_queue_model`

因为当前最核心的是 queue model，而不是后端本身。

---

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

---

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

---

## 五、建议推进顺序

### R1：目录和数据结构定型
### R2：TX/RX 生命周期 first pass
### R3：notify/irq/napi 稳定
### R4：stats/trace/report 完整

---

## 六、最终通过标准

stage07 完成的标志不是"能发一个包"，而是：

- 生命周期正确
- 统计可见
- 行为可证明
- 与 stage04 差异可解释
- 与 `virtio-net` 映射站得住

---

## 七、一句话总结

> stage07 是 netdev 主线从"教学型实现"迈向"准真实驱动模型"的关键一跳。
