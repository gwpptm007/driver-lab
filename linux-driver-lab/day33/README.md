# day33：ftrace function_graph 关键路径解释

## 1. 今日定位

- 周期：W5
- 后端设备：QEMU EDU（DMA-capable）
- 当日目标：采集关键路径的 function_graph 窗口，解释主要耗时函数并沉淀截图/文本证据。

这一天不追求“大而全”，只追求把当天的关键链路做通，并且把证据沉淀下来。

---

## 2. 你今天真正要学会什么

- 理解 function_graph 适合看“路径”和“嵌套耗时”
- 知道如何缩小 trace 窗口避免噪音
- 学会把 trace 和 bench/perf 结果互相印证

---

## 3. 推荐推进顺序

1. 确定关键路径，例如 ioctl -> DMA 提交 -> 中断完成。
2. 设置 function_graph 的 filter 与 trace 窗口。
3. 导出 trace 文本或截图。
4. 解释主要耗时点与调用关系。

---

## 4. 当日验收

- 成功采集关键路径
- 能解释主要耗时函数
- trace 与 workload 对应得上

---

## 5. 当日交付物

- `output/day33_ftrace_explain_template.md`
- `records/<timestamp>/trace.txt`

---

## 6. 风险提醒

- trace 窗口太大导致信息淹没
- 只贴 trace 不解释

---

## 7. 目录说明

```text
day33/
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

- 前一天：day32 的输出作为今日输入
- 后一天：day34 基于今天的证据继续推进
