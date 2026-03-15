# day29：DMA coherent 跑通

## 1. 今日定位

- 周期：W5
- 后端设备：QEMU EDU（DMA-capable）
- 当日目标：切换到 DMA-capable 后端，完成 `dma_alloc_coherent`、DMA 地址下发、读写校验和无崩溃验证。

这一天不追求“大而全”，只追求把当天的关键链路做通，并且把证据沉淀下来。

---

## 2. 你今天真正要学会什么

- 理解 coherent DMA buffer 与普通 kmalloc 的差别
- 知道 `dma_handle`/总线地址/CPU 虚拟地址三者分别是什么
- 掌握 DMA 前后数据一致性校验

---

## 3. 推荐推进顺序

1. 准备 DMA-capable 设备后端与 QEMU 参数。
2. 在驱动中调用 `dma_set_mask_and_coherent()` 与 `dma_alloc_coherent()`。
3. 设计最小 DMA 事务并完成数据一致性校验。
4. 记录失败路径和清理顺序。

---

## 4. 当日验收

- DMA buffer 分配成功
- 数据一致性校验通过
- 无 crash、无 DMA mapping error

---

## 5. 当日交付物

- `records/<timestamp>/dma_verify.txt`
- `output/day29_dma_notes.md`

---

## 6. 风险提醒

- 没区分 coherent buffer 与 streaming DMA
- 错误地把虚拟地址直接写给设备

---

## 7. 目录说明

```text
day29/
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

- 前一天：无，作为 W4/W5 起点
- 后一天：day30 进入 mmap 阶段
