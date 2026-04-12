# 04. 阶段任务拆解

## Stage00：bootstrap

### 目标
- 架构中立目录骨架
- 依赖检查
- 变量化 env/scripts
- 启动报告

### 产出
- `Makefile`
- `env/stage00.env`
- `scripts/discover_paths.sh`
- `scripts/check_host_tools.sh`
- `scripts/generate_stage00_report.sh`

---

## Stage01：netdev skeleton

### 任务
- 最小 `net_device` 驱动骨架
- `alloc_netdev_mqs` / `register_netdev`
- `ndo_open/stop/start_xmit`
- 最小 stats / debugfs

### 产出
- 驱动源码
- 最小用户态控制工具
- lifecycle 说明文档

---

## Stage02：skb path

### 任务
- 理解 `skb` 基本字段
- 软件 TX/RX 闭环
- `netif_rx` / `napi_gro_receive` 选择口径说明
- 内部环回 / 注入机制

### 产出
- `skb` 路径实验
- 数据流说明图
- 最小回归脚本

---

## Stage03：NAPI poll

### 任务
- 加入 NAPI
- poll + budget
- 中断抑制 / 恢复语义
- 统计项：irq / poll / budget hit / rx batches

### 产出
- NAPI 实验报告
- 纯中断 vs NAPI 对比
- 观测脚本

---

## Stage04：ring / DMA / RX replenishment

### 任务
- descriptor/ring 设计
- ownership 语义
- streaming DMA map/unmap
- RX buffer 预填充与 refill
- ring 枯竭行为说明

### 产出
- ring 设计文档
- DMA 路径代码
- refill 指标和错误注入

---

## Stage05：virtio-net + parameterization

### 任务
- 精读 `virtio-net` 主路径
- 对照自研 ring 与 vring
- 将 env/scripts 变为平台可配置
- 为 ARM64 迁移列出差异清单

### 产出
- 对照分析文档
- 参数化改造清单
- 路线评审结论

---

## Stage06：ARM64 migration

### 任务
- 迁移到 ARM64 QEMU
- 交叉编译与运行脚本
- 跨平台回归
- 性能/观测差异总结

### 产出
- ARM64 运行记录
- 差异报告
- 最终总报告
