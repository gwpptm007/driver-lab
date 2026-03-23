# day33 START_HERE

建议按下面顺序阅读和执行：

1. 先看 `README.md`，确认 day33 的目标是“采集并解释关键路径的 function_graph”
2. 再看 `docs/07_TEST_RESULT_ANALYSIS.md`，先理解当前包内这轮 records 为什么**还不算通过**
3. 看 `docs/06_TROUBLESHOOTING.md`，重点理解 tracefs 路径兼容问题
4. 执行 `make check` / `make run`
5. 查看新的 `records/day33-local-001/trace-window.txt`
6. 对照 `output/day33_ftrace_explain_template.md`，把关键函数与耗时说明补齐

## 今天最重要的一句话

**Day33 的重点不是跑更大的 workload，而是把一条已经跑通的路径解释清楚。**

## 今天不要做过头的点

- 不要一上来 trace 大规模 `bench-dma`
- 不要把所有函数都放进 `set_graph_function`
- 不要只看 trace，不回到 `run-result` / `mmap-verify` 校验实际行为

## 当前 records 的阅读顺序

先看：

- `records/day33-local-001/mmap-verify.txt`
- `records/day33-local-001/trace-config.txt`
- `records/day33-local-001/run-summary.md`

你会先看到：业务路径通过；再看到：trace 环境失败；最后在摘要里确认这轮不能算通过。
