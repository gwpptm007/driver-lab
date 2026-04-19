# Linux Driver Lab 后续学习与项目推进计划（基于当前仅完成到 stage09）

## 1. 当前阶段位置说明

当前项目主线已经推进到：

- `foundation/day01 ~ day35`：第一阶段基础驱动与实验平台能力
- `netdev/stage00 ~ stage09`：第二阶段网络驱动主线

其中，`netdev` 当前已经完成到：

- `stage07`：单队列 queue lifecycle 落地
- `stage08`：异步 backend transport 模型落地
- `stage09`：多队列与队列分发模型起步

这意味着当前项目已经不再处在“基础 API 学习期”，而进入了：

> **从教学型 netdev 驱动，向真实 Linux NIC 驱动关键机制靠拢的阶段**

因此，后续规划不能再只是“继续补几个 demo”，而应当按**主线深化 + 中长期转 Track**的方式推进。

---

## 2. 总体规划原则

后续规划遵循以下原则：

### 2.1 短期继续沿 netdev 主线深化
原因：
- 当前 `stage07 ~ stage09` 正好处在最值钱的位置
- 已经建立了 queue / NAPI / async backend / multi-queue 模型
- 此时停掉 netdev 主线，会导致投入产出不连续

### 2.2 中期不再无限 stage 化
`stage` 适合“课程式线性推进”，但不适合永远承载真实工业驱动研究。

因此建议：
- `stage10 ~ stage14` 仍保留为 netdev 主线深化
- `stage14` 之后，转为 `track + lab + project` 的组织方式

### 2.3 长期不只做 netdev
网络驱动主线完成后，应逐步展开其他模块线，包括：
- 块层 / 存储驱动
- PCIe / 总线 / MSI-X 深化
- 虚拟化网络与宿主机协同
- driver core / PM / 设备模型

---

## 3. 当前到 stage14 的 netdev 主线规划

## 3.1 stage10：MSI-X 与 per-queue IRQ affinity

### 目标
在 `stage09` 多队列模型基础上，引入更接近真实 NIC 驱动的中断组织方式：

- 每队列独立中断向量
- 每队列独立 NAPI context
- IRQ 与 CPU 绑定关系可观测
- 建立 queue / irq / cpu / napi 的对应模型

### 重点内容
- `pci_alloc_irq_vectors()` / MSI-X 机制
- 每 queue 对应一个 vector
- `request_irq()` 与 queue context 绑定
- `/proc/interrupts` 观测
- `smp_affinity` / IRQ 亲和性调节
- queue 到 CPU 的局部性分析

### 主要产出
- `stage10_msix_per_queue_irq/`
- 驱动代码
- queue/irq 映射文档
- affinity 调优说明
- 验收记录与中断分布报告

### 验收标准
- 至少 2 queue 独立中断
- `/proc/interrupts` 中可观察到每 queue 的 IRQ
- NAPI poll 与队列上下文对应关系明确
- affinity 调整后可看到中断落核变化

---

## 3.2 stage11：page_pool 与 RX page recycling

### 目标
把当前“教学型 RX buffer 生命周期”推进到更接近真实高性能 NIC 驱动的 RX 模型。

### 重点内容
- `page_pool`
- RX page 分配与回收
- page recycling
- `build_skb()` / `napi_build_skb()` 理解
- DMA sync 与 page 生命周期
- 为什么真实驱动不倾向于每包都 `alloc_skb`

### 主要产出
- `stage11_page_pool_rx/`
- page_pool 版 RX path
- RX 生命周期对比文档
- refill / recycle / drop 统计
- page_pool 观测脚本

### 验收标准
- RX path 引入 page_pool
- 正常完成收包、recycle、补充
- page reuse / recycle 统计稳定
- 无明显 page 泄漏、double free、残留 ready buffer

---

## 3.3 stage12：ethtool 与 control plane

### 目标
让驱动从“能收发包”提升为“可配置、可查询、可操作”的真实设备形态。

### 重点内容
- `ethtool_ops`
- stats 导出
- ringparam
- channels
- private flags
- feature bits 暴露
- queue / channel / ring 大小配置接口

### 主要产出
- `stage12_ethtool_control_plane/`
- ethtool 支持代码
- stats string / value 导出
- queue/channel 配置说明
- 控制面文档

### 验收标准
- `ethtool -S` 能正常获取统计
- 可读取 ring/channel 相关参数
- 至少实现 1~2 项控制配置能力
- 控制操作后行为与统计可观测

---

## 3.4 stage13：offload 基础（checksum / GRO / GSO 概念与最小实现）

### 目标
进入 Linux 驱动与网络栈协同的下一层，理解“驱动能力声明”和“协议栈配合”。

### 重点内容
- checksum offload 基础
- GRO / GSO / TSO 的概念与影响
- skb feature bits
- 驱动如何向协议栈暴露能力
- 为什么 offload 会改变数据路径和性能形态

### 主要产出
- `stage13_offload_basics/`
- offload 能力说明文档
- 最小可观测实验
- skb feature 相关观测报告

### 验收标准
- 能解释 checksum/GRO/GSO 的边界
- 至少实现一个最小能力暴露或最小实验
- 有清晰的“driver vs protocol stack”映射说明
- 有对比报告证明路径变化

---

## 3.5 stage14：XDP 入口与 fast path 模型

### 目标
在已经具备多队列、MSI-X、page_pool、offload 基础后，进入 XDP 入口理解。

### 重点内容
- XDP hook 点
- XDP_PASS / DROP / TX / REDIRECT
- page_pool 与 XDP 的关系
- fast path 与传统 skb path 的差异
- 驱动如何承接 XDP 程序执行

### 主要产出
- `stage14_xdp_entry/`
- XDP 入口版驱动实验
- XDP path 对比文档
- page_pool / XDP 关系说明
- 观测与验证脚本

### 验收标准
- 能清楚解释 XDP 在驱动中的位置
- 至少完成最小 XDP path 验证
- 与普通 RX skb path 做出对照
- 有明确的 fast path / normal path mapping 文档

---

## 4. stage10 ~ stage14 完成后的含义

当 `stage10 ~ stage14` 完成后，意味着 netdev 主线已经覆盖了 Linux NIC 驱动的关键机制链：

- multi-queue
- MSI-X
- per-queue NAPI
- page_pool
- ethtool/control plane
- offload
- XDP entry

到这一步，项目将从“教学驱动工程”进入：

> **具备真实 Linux NIC 驱动关键机制理解的完整作品线**

这时，不建议继续无限新增 `stage15 stage16 stage17...`，而应切换为：

- Track（长期方向）
- Lab（专题实验）
- Project（对外成果）

---

## 5. stage14 之后的组织方式：由 stage 转向 track / lab / project

## 5.1 总体组织调整建议

### 现阶段
继续保留：
- `netdev/stage00 ~ stage14`

### stage14 之后
不再继续线性 stage 化，而是进入：

- `track-real-driver/`
- `track-virtual-net/`
- `track-perf-debug/`
- `track-storage-block/`
- `track-driver-core-pm/`

每个 Track 下再分多个 `lab-*` 与最终 `project-*`。

---

## 6. stage14 之后的 Track 规划

## 6.1 Track A：真实 Linux NIC 驱动源码与补丁线

### 定位
从“自己写教学驱动”转向“阅读和修改真实工业驱动”。

### 适合的主题
- `virtio_net`
- `igc`
- `e1000e`
- `stmmac`
- 局部阅读 `mlx5`

### 建议 Lab
- `lab-virtio-net-source-dive`
- `lab-igc-msix-and-napi`
- `lab-stmmac-page-pool-analysis`
- `lab-real-driver-ethtool-stats`
- `lab-real-driver-small-patch`

### 最终目标
- 能带着问题读真实驱动
- 能做小 patch
- 能解释真实驱动和自己阶段项目的 mapping

### 对外成果
- `project-linux-nic-driver-deep-dive`
- `project-real-driver-patch-lab`

---

## 6.2 Track B：虚拟化网络与宿主机协同线

### 定位
把驱动和真实宿主机/虚拟化网络系统连接起来。

### 适合的主题
- virtio / vhost
- tap / tuntap
- veth
- bridge
- tc / qdisc
- host / guest 数据路径协同

### 建议 Lab
- `lab-virtio-vhost-kick-notify`
- `lab-tap-bridge-veth-basic`
- `lab-qdisc-and-driver-boundary`
- `lab-vhost-data-path-observation`

### 最终目标
- 理解 guest netdev 与 host 网络系统的连接方式
- 理解虚拟化网络中的 front/back 边界

### 对外成果
- `project-virtual-net-stack-bridge`

---

## 6.3 Track C：性能与故障定位线

### 定位
从“能写驱动”升级到“能分析真实问题”。

### 适合的主题
- queue 不均衡
- IRQ 落核不合理
- softirq 打满
- NAPI budget 不足
- RX drop 根因分析
- page_pool recycle 异常
- XDP 路径问题

### 建议 Lab
- `lab-softnet-stat-analysis`
- `lab-irq-affinity-balance`
- `lab-rx-drop-root-cause`
- `lab-page-pool-leak-debug`
- `lab-napi-budget-exhaustion`

### 最终目标
- 建立性能与故障分析方法论
- 形成一套系统级观测手段

### 对外成果
- `project-netdriver-performance-and-failure-analysis`

---

## 6.4 Track D：块层 / 存储驱动线

### 定位
作为 netdev 之外最值得展开的第二主线。

### 为什么推荐
块层与网络驱动一样，都是 Linux 内核里最典型的高性能 I/O 子系统。  
学习块层后，可以显著提升：

- queue/completion 理解
- 高性能 I/O 统一视角
- 驱动抽象能力

### 适合的主题
- block layer
- bio / request
- blk-mq
- virtio-blk
- NVMe 驱动阅读
- completion / timeout / tagset

### 建议 Lab
- `lab-blk-mq-basic-model`
- `lab-virtio-blk-source-dive`
- `lab-nvme-queue-and-completion`
- `lab-block-timeout-recovery`

### 最终目标
- 构建第二条高性能驱动主线
- 形成 netdev 与 block 两类 I/O 子系统的对照理解

### 对外成果
- `project-linux-block-driver-study`

---

## 6.5 Track E：PCIe / 总线 / MSI-X 深化线

### 定位
补足“真实设备驱动工程能力”的总线与中断管理层。

### 适合的主题
- MSI-X 深入
- 多 vector 分配
- bus / device / driver model
- AER / reset / error recovery
- DMA mask / IOMMU
- SR-IOV 概念理解
- suspend / resume 与设备恢复

### 建议 Lab
- `lab-pcie-msix-vectors`
- `lab-pcie-error-recovery`
- `lab-dma-mask-and-iommu`
- `lab-bus-device-driver-model`

### 最终目标
- 从“会用 PCIe”走向“能分析真实设备异常与恢复路径”

### 对外成果
- `project-pcie-driver-advanced`

---

## 6.6 Track F：driver core / PM / 设备模型线

### 定位
提升通用驱动工程底盘能力。

### 适合的主题
- kobject / kset
- sysfs deeper
- driver core
- device / bus / class 模型
- runtime PM
- suspend / resume
- devm 体系
- firmware / DT / ACPI 差异

### 建议 Lab
- `lab-driver-core-object-model`
- `lab-runtime-pm-basic`
- `lab-suspend-resume-driver-path`
- `lab-devm-and-lifecycle`

### 最终目标
- 建立“驱动不仅是数据路径代码”的完整工程视角

### 对外成果
- `project-driver-core-and-power-management`

---

## 7. 模块优先级建议

如果后续不能同时展开所有 Track，建议按这个优先级推进。

### 第一优先级：继续完成 netdev 主线到 stage14
原因：
- 当前积累最深
- 进展最连续
- 作品线最完整

### 第二优先级：开启块层 / 存储线
原因：
- 作为第二高价值主线，与 netdev 构成高性能 I/O 双主线

### 第三优先级：真实 NIC 驱动源码与 patch 线
原因：
- 把教学项目能力迁移到真实工业驱动

### 第四优先级：虚拟化网络 / 宿主机协同线
原因：
- 与云环境、虚拟化网络高度相关

### 第五优先级：PCIe/总线/MSI-X 深化与 driver core/PM
原因：
- 更偏底层通用能力，可并行长期补

---

## 8. 推荐的总体推进顺序

### 短期（当前 ~ stage14）
- 完成 `stage10 ~ stage14`
- 形成完整 netdev 主线闭环

### 中期（stage14 之后）
- 转入 `track-real-driver`
- 同时开启 `track-storage-block`

### 长期
- 并行推进：
  - `track-virtual-net`
  - `track-perf-debug`
  - `track-pcie-advanced`
  - `track-driver-core-pm`

---

## 9. 预期最终形成的能力结构

后续计划完成后，期望形成的能力结构是：

### 9.1 网络驱动能力
- netdev / NAPI / multi-queue / MSI-X / page_pool / XDP
- ethtool / offload / IRQ affinity
- real NIC driver source dive / patch

### 9.2 高性能 I/O 驱动能力
- network I/O
- block I/O
- queue/completion 统一理解
- DMA / MSI-X / 多队列 / 生命周期管理

### 9.3 系统协同能力
- 虚拟化网络
- 宿主机网络路径
- qdisc / tc / bridge / veth / tap
- front-end / back-end 模型

### 9.4 调试与问题定位能力
- trace / perf / ftrace / tcpdump
- interrupts / softnet_stat / ethtool -S
- queue imbalance / drop root cause / budget exhaustion

### 9.5 通用驱动工程能力
- driver core
- PM / suspend / resume
- device / bus / class
- devm / sysfs / kobject

---

## 10. 结论

当前只完成到 `stage09`，后续规划建议明确分成两段：

### 第一段：继续完成 netdev 主线到 stage14
目标：
- 把 Linux NIC 驱动关键机制系统吃透

### 第二段：从 stage 体系切换到 track / lab / project 体系
目标：
- 不再只做教学驱动
- 开始进入真实驱动源码、真实补丁、真实性能与故障定位、以及其他模块线

最终不应长期停留在“只有 netdev 一条线”，而应逐步形成：

- 一条完整的 netdev/NIC 主线
- 一条块层/存储第二主线
- 若干虚拟化、性能、PCIe、driver core 的专题 Track

这才是后续最合理、最专业、也最适合作为长期驱动成长路线的规划。
