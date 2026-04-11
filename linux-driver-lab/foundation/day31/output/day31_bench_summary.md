# day31 bench summary

## 建议填写方式

1. 先把 `records/<RUN_ID>/bench-all.txt` 中的 `csv,` 行整理到 CSV 模板里
2. 再把关键观察写在这里

## 建议至少回答的问题

- `ioctl / mmap / dma` 三条路径谁最轻、谁最重
- payload 从 64B 增长到 2048B 时，延迟和吞吐的趋势如何
- `cpu_user_pct / cpu_sys_pct` 是否揭示了零拷贝带来的价值
- `p99` 与 `avg` 的差距是否明显，是否存在抖动
