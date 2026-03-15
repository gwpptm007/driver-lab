# day26：用户态工具与错误码

## 1. 今日定位

- 周期：W4
- 后端设备：ivshmem-doorbell
- 当日目标：完成用户态工具，支持状态查询、共享区读写、中断触发或观察，错误码明确可定位。

这一天不追求“大而全”，只追求把当天的关键链路做通，并且把证据沉淀下来。

---

## 2. 你今天真正要学会什么

- 知道什么时候选 ioctl，什么时候选 read/write
- 理解“工具可用”不等于“排障友好”，错误码与日志设计同样重要
- 学会给工具设计 smoke case 与 help 文本

---

## 3. 推荐推进顺序

1. 确定字符设备接口与 ioctl 编号。
2. 实现基础命令：get_info、read_state、write_payload、clear_stats、trigger_or_wait_irq。
3. 工具打印 errno、返回码、关键上下文。
4. 形成一套最小 smoke 用例。

---

## 4. 当日验收

- 工具可正常跑通核心路径
- 错误输入有清晰报错
- README 中有命令示例

---

## 5. 当日交付物

- `output/day26_tool_usage.md`
- `records/<timestamp>/tool_smoke.txt`

---

## 6. 风险提醒

- 接口过多导致当日收不住
- 没有定义结构体版本号与大小检查

---

## 7. 目录说明

```text
day26/
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

- 前一天：day25 的输出作为今日输入
- 后一天：day27 基于今天的证据继续推进
