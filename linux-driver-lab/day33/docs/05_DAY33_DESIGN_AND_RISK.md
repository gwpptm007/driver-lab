# Day33 设计与风险

## 1. 设计原则

- 不新增复杂功能
- 复用 day32 已通过的 `mmap + dma` 基线
- 把精力放在“采集证据 + 解释证据”上

## 2. 主要风险

### 风险 A：内核没开 function_graph
解决：`make check` 先检查 `.config`

### 风险 B：trace 窗口太大
解决：默认 workload 只跑一次 `mmap-verify`

### 风险 C：trace 命中了，但没人能解释
解决：同时产出 `trace-config.txt`、`run-result.txt`、`dmesg-driver.txt`
