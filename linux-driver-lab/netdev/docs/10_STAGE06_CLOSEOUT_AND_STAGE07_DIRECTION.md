# 10_STAGE06_CLOSEOUT_AND_STAGE07_DIRECTION

## 文档定位

这份文档用于承接当前 netdev 主线的“下一步”。

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

> 迁移方法已经建立，但还没有完全沉淀成“别人拿去就能复现”的阶段包。

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

但更推荐第一个，因为当前最重要的不是“接什么后端”，而是把驱动模型做得更像真实队列驱动。

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

## 七、一句话总结

当前这条线最值的下一步是：

> 先把 stage06 做成真正可复用的平台迁移阶段，再开 stage07，把 stage04 的教学型 netdev 推进成更接近真实驱动模型的版本。
