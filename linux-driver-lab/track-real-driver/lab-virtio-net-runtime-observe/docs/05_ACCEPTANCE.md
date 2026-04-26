# 05_ACCEPTANCE

## 最低通过标准

1. 能产生至少一轮 idle baseline 记录
2. 能产生至少一轮 ping 记录
3. 能产生至少一轮 iperf3 记录
4. 至少拿到一组 TX/RX/NAPI 相关 trace 痕迹
5. 能把一条 `source-dive` 中的路径图和运行期记录对上

## 标准通过

在最低通过基础上，再做到：

- `ethtool -S` / `ip -s link` 有 before/after
- trace 记录能区分不同 workload
- `records/<ts>/SUMMARY.md` 能解释假设是否成立
- `reports/runtime_observe_report.md` 有结构化总结

## 优秀通过

- 能明确指出：哪些点适合后续 ethtool/stats 小 patch
- 能明确指出：哪些 trace 点值得继续增强
- 能自然导向下一个 Lab：`lab-virtio-net-ethtool-stats-mini-patch`
