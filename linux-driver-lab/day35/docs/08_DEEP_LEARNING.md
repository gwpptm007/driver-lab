# Day35 阶段报告收口深度指南 - W4/W5 终点

## 一、Day35 是什么？

Day35 是 W5 的最后一天，也是整个 Linux Driver Lab（day01-35）的**终点**。

**核心目标**：把前面 35 天的学习成果（功能、性能、稳定性、可观测性）整理成一份**可交付、可复盘、可汇报**的阶段报告。

Day35 不再新增驱动功能。这一天的重点是：
1. 汇总证据（day29-34 的 records 解析）
2. 统一指标口径（metrics summary CSV）
3. 形成阶段结论（final report）
4. 列出风险与回滚建议（risk register）

---

## 二、为什么需要 Day35？

### 2.1 从"做出来"到"说清楚"

```
前面 34 天：证明"我做出来了"
Day35：证明"我不仅做出来了，还能说清楚、总结好"

面试/汇报时，面试官不会问"你怎么实现 mmap"，
他们会问："你的性能提升了多少？稳定性怎么验证的？风险是什么？"
```

### 2.2 Day35 的价值

```
1. 证据组织能力：
   - 把分散的 records 整理成索引
   - 让面试官能快速定位证据

2. 指标量化能力：
   - 把"快了很多"变成"从 280us 降到 1us，提升 280 倍"
   - 把"测试通过"变成"1000 次模块循环，0 次失败"

3. 风险意识：
   - 诚实列出开放问题
   - 给出回滚建议
   - 展示系统思维
```

---

## 三、Day35 的输入：day29-34 证据链

### 3.1 各天的核心产出

```
Day29：coherent DMA round-trip
  → records：mmap-verify 输出、dmesg 日志
  → 关键指标：verify_ok、irq_delta

Day30：mmap 零拷贝主链路
  → records：mmap-verify、run-result
  → 关键指标：mmap_ok、verify_ok、run_ok

Day31：三条 bench 路径基准
  → records：bench-ioctl、bench-mmap、bench-dma
  → 关键指标：avg_us、p99_us、throughput_mbps

Day32：mmap 优化前后对比
  → records：bench-mmap baseline vs optimized
  → 关键指标：latency_gain_pct、throughput_gain_pct

Day33：ftrace function_graph 关键路径
  → records：trace-window.txt
  → 关键指标：函数调用树、关键路径耗时

Day34：稳定性与错误注入
  → records：concurrent-stress、module-loop、fault-*
  → 关键指标：completed_loops、failed_loops、fault_errno
```

### 3.2 证据收集原则

```
Day35 不复制原始日志全文。

只做两件事：
  1. 记录"证据文件在哪里"
  2. 记录"这个文件证明了什么"

这样做的好处：
  - 报告不会变成日志堆砌
  - 面试官能快速定位原始证据
  - 后续可以增量扩展
```

---

## 四、报告结构：五段式设计

### 4.1 五段式结构

```
1. 背景与目标
   → 这阶段要解决什么问题？
   → 为什么选择 PCIe EDU 作为学习载体？

2. 阶段结果概览
   → 哪些天通过，哪些天未通过
   → 用一句话总结每个关键里程碑

3. 指标与证据
   → 量化数据（延迟、吞吐量、成功率）
   → 证据索引（指向具体 records 文件）

4. 风险与限制
   → Day33 trace 覆盖仍有优化空间（开放风险）
   → Day34 稳定性回归通过（已验证）

5. 结论与建议
   → W4/W5 主线已收住
   → 后续可以继续优化的方向
```

### 4.2 为什么这样设计？

```
便于面试/汇报时快速展开：
  - 面试官问"这个项目做什么的"→ 回答背景
  - 面试官问"做到了什么程度"→ 回答结果概览
  - 面试官问"性能数据呢"→ 回答指标与证据
  - 面试官问"有什么风险"→ 回答风险与限制
  - 面试官问"后续怎么改进"→ 回答结论与建议

不会把报告写成流水账：
  - 每个 section 都有明确目的
  - 不是"第一天做了什么，第二天做了什么"
  - 而是"我们解决了什么问题，达到了什么指标"
```

---

## 五、指标体系

### 5.1 关键指标来源

| Day | 指标 | 含义 |
|-----|------|------|
| Day29 | `verify_ok` | DMA 往返数据完整性验证 |
| Day29 | `irq_delta` | DMA 往返触发的 IRQ 次数（应为 2） |
| Day30 | `mmap_ok` | mmap 是否成功 |
| Day30 | `verify_ok` | mmap 零拷贝路径数据完整性 |
| Day31 | `avg_us` | 平均延迟（微秒） |
| Day31 | `p99_us` | P99 延迟（微秒） |
| Day31 | `throughput_mbps` | 吞吐量（MB/s） |
| Day32 | `latency_gain_pct` | 延迟提升百分比 |
| Day32 | `throughput_gain_pct` | 吞吐量提升百分比 |
| Day34 | `completed_loops` | 完成的模块循环次数 |
| Day34 | `failed_loops` | 失败的模块循环次数 |
| Day34 | `fault_errno` | 错误注入返回的 errno |

### 5.2 指标汇总 CSV

```csv
day,metric,value,unit,evidence
day29,verify_ok,1,bool,records/day29-local-001/mmap-verify.txt
day29,irq_delta,2,count,records/day29-local-001/run-result.txt
day30,mmap_ok,1,bool,records/day30-local-001/mmap-verify.txt
day30,verify_ok,1,bool,records/day30-local-001/mmap-verify.txt
day31,bench-ioctl-avg_us,16.5,us,records/day31-local-001/bench-ioctl.txt
day31,bench-mmap-avg_us,0.52,us,records/day31-local-001/bench-mmap.txt
day31,bench-dma-avg_us,198000,us,records/day31-local-001/bench-dma.txt
day32,latency_gain_pct,99.65,%,records/day32-local-001/compare-*.md
day32,throughput_gain_pct,24826,%,records/day32-local-001/compare-*.md
day34,completed_loops,1000,count,records/day34-local-001/module-loop.txt
day34,failed_loops,0,count,records/day34-local-001/module-loop.txt
```

**CSV 的价值**：
- 后续阶段对比（增量实验）
- 面试时快速引用具体数字
- 自动化报告生成的基础

---

## 六、风险登记

### 6.1 风险登记的目的

```
不是把所有问题列出来就完了，
而是：
  1. 诚实列出开放问题（不是所有问题都能解决）
  2. 给出缓解措施（风险不能消除就缓解）
  3. 给出回滚建议（如果出了问题怎么退）
```

### 6.2 Day35 识别的风险

```
【开放风险】Day33 trace 覆盖仍有优化空间
  描述：function_graph 已成功开启并采到关键路径窗口，
        但对目标函数的覆盖仍可继续优化。
  影响：无法完整展示 DMA 调用的耗时分布
  缓解：已验证核心函数（ioctl → do_run_dma → program_dma → wait_dma_idle）调用链
  状态：开放

【已验证】Day34 稳定性回归通过
  描述：并发压测、1000 次模块循环、错误注入全部通过
  影响：无
  状态：已关闭
```

### 6.3 回滚建议

```
如果后续修改导致回归：
  1. 回滚到 Day34 基线（稳定性验证最完整）
  2. 重新运行 mmap-verify 确认主链路
  3. 重新运行 module-loop 确认资源释放
  4. 重新运行 concurrent-stress 确认并发安全
```

---

## 七、提交检查单

### 7.1 必须满足

```
- [ ] output/day35_final_report.md 已生成
- [ ] output/day35_evidence_index.md 已生成
- [ ] output/day35_metrics_summary.csv 已生成
- [ ] output/day35_risk_register.md 已生成
- [ ] 报告中明确标出 Day33 为开放风险
- [ ] 报告中明确标出 Day34 为稳定性回归通过点
```

### 7.2 报告质量自查

```
1. 目标明确：
   - 开头是否说清楚"这阶段要解决什么问题"
   - 结尾是否回答了"做到了什么程度"

2. 指标量化：
   - 是否有具体数字（延迟、吞吐量、成功率）
   - 是否有对比数据（优化前 vs 优化后）

3. 证据可查：
   - 每个结论是否有对应的 records 文件
   - evidence_index 是否能快速定位

4. 风险诚实：
   - 是否列出了开放问题（不是所有问题都能解决）
   - 是否有回滚建议
```

---

## 八、W4/W5 演进路线回顾

### 8.1 PCIe + DMA 学习路径

```
W4：PCIe 基础（day22-28）
  day22-23：PCIe 枚举与 BAR/MMIO
  day24-25：MSI 中断
  day26-28：ivshmem-doorbell 设备

W5：DMA + 性能分析（day29-35）
  day29：coherent DMA round-trip
  day30：mmap 零拷贝
  day31：benchmarking 三条路径
  day32：perf + ftrace 优化分析
  day33：ftrace function_graph 关键路径
  day34：稳定性测试
  day35：阶段报告收口
```

### 8.2 关键学习点

```
1. DMA 核心概念：
   - dma_alloc_coherent() 返回 cpu_virt 和 dma_handle
   - 两段 DMA：RAM→EDU，EDU→RAM
   - 中断触发：DONE 位 + IRQ

2. mmap 零拷贝：
   - dma_mmap_coherent() 将 DMA buffer 映射到用户态
   - 用户态直接访问，不需要 copy_to_user/copy_from_user

3. 性能分析工具：
   - perf stat/record/report：热点分析
   - ftrace function_graph：调用路径分析

4. 稳定性验证：
   - 并发压测：flock() 用户态协调
   - 模块循环：request_irq() vs devm_request_irq()
   - 错误注入：边界检查和拒绝
```

---

## 九、与 Day34 的关系

### 9.1 继承关系

```
Day34：验证了稳定性（并发、模块循环、错误注入）
Day35：总结稳定性验证的结论，并作为风险登记的依据
```

### 9.2 核心区别

```
Day34：做稳定性测试（过程）
Day35：写稳定性结论（结果）

Day34 的输出：raw records（日志、计数、错误输出）
Day35 的输出：organized evidence（索引、报告、风险登记）
```

---

## 十、面试要会讲的五句话

1. **"Day35 是 W5 的收口日，目标是把我从 day29 到 day34 做的东西整理成一份可交付的阶段报告"**
   → 证明你有"做完之后收口"的习惯

2. **"报告中最重要的不是过程堆砌，而是目标、指标、结果、风险、回滚建议这五段式结构"**
   → 证明你有结构化表达能力

3. **"W4/W5 的主线是 PCIe 枚举→DMA→mmap→性能分析→稳定性验证，Day35 把这条线串起来"**
   → 证明你对整个学习路径有全局视角

4. **"指标汇总 CSV 不是为了好看，而是为了后续增量实验和面试时快速引用"**
   → 证明你有数据管理意识

5. **"风险登记不是把所有问题列出来，而是诚实列出开放问题并给出缓解措施和回滚建议"**
   → 证明你有风险意识和系统思维

---

## 十一、附录：Day35 脚本执行

### 11.1 推荐执行顺序

```bash
cd day35
chmod +x scripts/*.sh
source env/day35.env
bash scripts/04_run_all.sh
```

### 11.2 生成的文件

```
output/day35_final_report.md       # 最终阶段报告
output/day35_evidence_index.md     # 证据索引
output/day35_metrics_summary.csv   # 指标汇总
output/day35_risk_register.md     # 风险登记
output/day35_submission_checklist.md  # 提交检查单
```

### 11.3 查看顺序

```bash
cat output/day35_final_report.md       # 首先看报告
cat output/day35_risk_register.md      # 其次看风险
cat output/day35_evidence_index.md      # 再看证据索引
cat output/day35_metrics_summary.csv   # 最后看指标
```
