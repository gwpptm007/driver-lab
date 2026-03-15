# day27：remove/卸载与 200 次循环

## 1. 今日定位

- 周期：W4
- 后端设备：ivshmem-doorbell
- 当日目标：验证资源释放对称性，通过 200 次 `insmod/rmmod` 循环，无崩溃、无残留状态。

这一天不追求“大而全”，只追求把当天的关键链路做通，并且把证据沉淀下来。

---

## 2. 你今天真正要学会什么

- 理解 PCI 驱动 remove 的清理顺序
- 学会用循环回归暴露资源泄漏/竞态问题
- 形成最小压力脚本与失败样本收集方法

---

## 3. 推荐推进顺序

1. 梳理 probe 中申请的所有资源及其清理顺序。
2. 编写 `insmod -> smoke -> rmmod` 循环脚本。
3. 每轮记录是否成功、失败点、dmesg 摘要。
4. 遇到失败时归档失败轮次的完整日志。

---

## 4. 当日验收

- 200 次循环通过
- 无 oops、无 hung task、无明显内存泄漏迹象
- `records/` 中有循环摘要

---

## 5. 当日交付物

- `records/<timestamp>/loop_summary.txt`
- `output/day27_remove_checklist.md`

---

## 6. 风险提醒

- IRQ/vector、iomap、chrdev、kthread 清理遗漏
- 模块退出时仍有打开文件或等待队列

---

## 7. 目录说明

```text
day27/
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

- 前一天：day26 的输出作为今日输入
- 后一天：day28 基于今天的证据继续推进
