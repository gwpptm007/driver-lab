# day24：MMIO 读写与共享内存协议

## 1. 今日定位

- 周期：W4
- 后端设备：ivshmem-doorbell
- 当日目标：完成 `readl/writel` 练习，并定义最小共享内存/寄存器协议，支持用户态可验证读写。

这一天不追求“大而全”，只追求把当天的关键链路做通，并且把证据沉淀下来。

---

## 2. 你今天真正要学会什么

- 掌握 MMIO 与普通内存访问的差异
- 学会设计“可验证”的最小协议而不是只做裸读写
- 理解设备寄存器区与共享内存区的职责划分

---

## 3. 推荐推进顺序

1. 梳理 ivshmem 的 BAR 结构与共享区访问边界。
2. 封装 `readl/writel` 或 `ioread32/iowrite32` 访问函数。
3. 设计一组最小字段：magic、version、state、producer/consumer、payload_len。
4. 提供内核态自测与用户态校验路径。

---

## 4. 当日验收

- 用户态可写入一段数据并在另一端读回验证
- `dmesg` 有明确协议状态日志
- 边界检查与错误码清晰

---

## 5. 当日交付物

- `output/day24_protocol_design.md`
- `records/<timestamp>/mmio_rw_verify.txt`

---

## 6. 风险提醒

- 把共享内存当普通缓冲随意访问
- 没有做 offset/size 校验导致越界

---

## 7. 目录说明

```text
day24/
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

- 前一天：day23 的输出作为今日输入
- 后一天：day25 基于今天的证据继续推进
