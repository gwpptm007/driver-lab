# 学习进度（按当前 day35 基线更新）

## 1. 当前阶段结论

当前 `linux-driver-lab` 已经完成：

- **W1：字符设备基础闭环**
- **W2：platform / DT / IRQ / regmap / ftrace**
- **W3：baseline / 裁剪 / perf / 回归收口**
- **W4：PCIe 基本功作品线**
- **W5：DMA / mmap / bench / perf / function_graph / stability**

因此，当前仓库的状态已经从“驱动入门练习”演进为：

> **一套以 day01 ~ day35 为主线、具有阶段交付物和证据归档的实验型驱动学习项目。**

---

## 2. 已完成阶段

### W1：day01 ~ day07
已完成内容：

- Day01：miscdevice 驱动骨架与生命周期
- Day02：ioctl SET/GET 与用户态测试
- Day03：sysfs 属性接口
- Day04：debugfs 状态导出与日志级别控制
- Day05：waitqueue / workqueue / 上下文理解
- Day06：回归脚本与压力测试
- Day07：W1 收口与文档整理

阶段结论：

- 字符设备主线已完成最小闭环
- 已形成基本的接口层次、可观测性和回归意识

---

### W2：day08 ~ day14
已完成内容：

- Day08：platform_driver + probe/remove + devm
- Day09：Device Tree 匹配、reg/irq 解析
- Day10：IRQ 注册与 `/proc/interrupts` 验证
- Day11：workqueue bottom-half 与延迟统计
- Day12：regmap 封装寄存器 + debugfs 快照
- Day13：function_graph 跟踪 IRQ 路径
- Day14：bring-up checklist 文档化

阶段结论：

- 已完成从字符设备到平台驱动套路的迁移
- arm64 / QEMU virt / DT / ftrace 路径已经建立

---

### W3：day15 ~ day21
已完成内容：

- baseline 冻结
- 配置裁剪
- tracing / perf 能力保留
- profile 与结果对比
- 自动回归套件
- 最终提交版总结材料

阶段结论：

- 已形成“可裁剪、可对比、可回归、可回滚”的实验平台雏形
- `day21/FINAL_SUBMISSION.md` 可以作为 W3 的正式总结入口

---

### W4：day22 ~ day28
已完成内容：

- PCI 设备枚举
- `lspci -vv` 与证据归档
- `pci_driver` 骨架
- BAR / MMIO / 共享内存协议
- MSI 中断
- 用户态工具闭环
- remove / 200 次循环装卸
- W4 最终证据收口

阶段结论：

- W4 已完成从“设备可见”到“驱动闭环 + 稳定性”的整条 PCIe 基本功链
- `day28/README.md` 可以作为 W4 的正式总结入口

---

### W5：day29 ~ day35
已完成内容：

- coherent DMA round-trip
- `mmap` 零拷贝主链路
- bench 三条路径
- perf 热点分析
- function_graph 路径采集
- 稳定性与错误注入
- 阶段性能与风险报告

阶段结论：

- W5 的功能、性能、稳定性和基础可观测性主线已经收住
- 当前开放项已经更多转成 trace 覆盖完整性与后续扩展方向，而不是“主链路是否成立”
- `day35/README.md` 可以作为 W5 的正式总结入口

---

## 3. 当前最值得看的阶段入口

### 看整体
- `START_HERE_CURRENT.md`
- `docs/CURRENT_PROJECT_REVIEW.md`

### 看阶段收口
- `day21/FINAL_SUBMISSION.md`
- `day28/README.md`
- `day35/README.md`

---

## 4. 当前项目的主要优点

1. **day 之间是连续演进关系**，不是散乱 demo。
2. **records 和输出物保留较完整**，适合复盘与评审。
3. **W3 以后工程化明显增强**，开始有 baseline、回归、提交物意识。
4. **W4 / W5 已经具备作品线特征**，不再只是 API 练习。

---

## 5. 当前开放项

当前并不是“还没做完基础功能”，而是进入下面这些开放点：

1. **总入口文档统一性**
   - 本次已做一轮整理，但后续新增阶段时仍要同步维护。

2. **真实子系统级迁移**
   - 当前已强在 char/platform/PCI/DMA。
   - 后续若继续增强，需要迁移到更真实的子系统，例如网络驱动、块设备、runtime PM。

3. **trace 深化**
   - 当前已经有 perf / function_graph。
   - 后续提升点主要是覆盖更完整的关键路径，而不是“有没有 trace”。

---

## 6. 当前建议

在你正式评审“后续学什么”之前，我建议把当前仓库先按“阶段性完整作品”来审视：

- 先确认 W1 ~ W5 的收口质量
- 再决定下一站到底是网络驱动、块设备，还是别的路线

也就是说：

> 当前最重要的事情不是立刻继续加新 day，而是先把现有 day01 ~ day35 的阶段价值看清楚。
