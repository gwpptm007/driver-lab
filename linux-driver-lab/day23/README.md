# day23：pci_driver 骨架与 BAR 映射

## 1. 今日定位

- 周期：W4
- 后端设备：ivshmem-doorbell
- 当日目标：实现 `pci_driver` 的 probe/remove 基础骨架，打通 `enable_device/request_regions/pci_iomap`。

这一天不追求“大而全”，只追求把当天的关键链路做通，并且把证据沉淀下来。

---

## 2. 你今天真正要学会什么

- 掌握 `pci_device_id` 与 `pci_driver` 的配对流程
- 理解 BAR 是什么、为什么需要 `request_regions` 和 `pci_iomap`
- 理解 probe/remove 的对称释放顺序

---

## 3. 推荐推进顺序

1. 编写最小 `pci_driver` 骨架与 `MODULE_DEVICE_TABLE`。
2. 在 probe 中依次完成：`pci_enable_device()`、`pci_request_regions()`、`pci_set_master()`、`pci_iomap()`。
3. 打印 BAR 起止地址、长度、虚拟映射地址。
4. 在 remove 中对称释放资源并验证重复加载。

---

## 4. 当日验收

- 模块加载后 probe 成功
- `dmesg` 中打印 BAR 信息
- `rmmod` 无泄漏、无 oops、无 use-after-free

---

## 5. 当日交付物

- `records/<timestamp>/dmesg-probe.txt`
- `output/day23_probe_sequence.md`

---

## 6. 风险提醒

- BAR 编号理解错误
- 资源释放顺序不对导致二次加载失败

---

## 7. 目录说明

```text
day23/
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

- 前一天：day22 的输出作为今日输入
- 后一天：day24 基于今天的证据继续推进
