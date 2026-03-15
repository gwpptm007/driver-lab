# day25：消息中断向量与中断统计

## 1. 今日定位

- 周期：W4
- 后端设备：ivshmem-doorbell
- 当日目标：使用 `pci_alloc_irq_vectors()` 与 `request_irq()` 打通消息中断路径，验证 `/proc/interrupts` 计数增长。

这一天不追求“大而全”，只追求把当天的关键链路做通，并且把证据沉淀下来。

---

## 2. 你今天真正要学会什么

- 理解 `pci_alloc_irq_vectors()` 的统一入口意义
- 知道为什么在 ivshmem-doorbell 场景中更常见的是 MSI-X / doorbell 路线
- 掌握驱动内部计数与 `/proc/interrupts` 双证据验证

---

## 3. 推荐推进顺序

1. 调用 `pci_alloc_irq_vectors()`，flags 允许 `PCI_IRQ_MSIX | PCI_IRQ_MSI | PCI_IRQ_INTX`。
2. 使用 `pci_irq_vector()` 获取向量并 `request_irq()`。
3. 设计一个最小触发路径，让用户态或对端动作能触发中断。
4. 在 ISR 中更新原子计数并导出状态。

---

## 4. 当日验收

- 中断成功注册
- `/proc/interrupts` 计数增长
- 驱动内部计数与系统计数方向一致

---

## 5. 当日交付物

- `records/<timestamp>/interrupts-before.txt`
- `records/<timestamp>/interrupts-after.txt`
- `output/day25_irq_analysis.md`

---

## 6. 风险提醒

- 把“MSI 学习目标”和 ivshmem 实际 MSIX 行为混为一谈
- 中断已触发但 ack/清状态逻辑不完整

---

## 7. 目录说明

```text
day25/
├── README.md
├── START_HERE.md
├── docs/
├── output/
└── records/
```

- `docs/`：当天的详细理解、拆解与清单
- `output/`：当天最终整理出的说明、模板、阶段结论
- `records/`：运行证据、日志、截图、命令输出

---

## 8. 和前后天的关系

- 前一天：day24 的输出作为今日输入
- 后一天：day26 基于今天的证据继续推进
