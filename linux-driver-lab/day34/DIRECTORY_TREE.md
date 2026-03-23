# day34 目录说明

```text
day34/
├── README.md
├── START_HERE.md
├── DIRECTORY_TREE.md
├── Makefile
├── docs/
├── driver/
├── env/
├── guest/
├── include/
├── output/
├── records/
├── scripts/
├── third_party/
├── tools/
└── workdir/
```

- `driver/`：day34 独立字符设备驱动，能力基线沿用 day33，但名字与注释已收成稳定性主题
- `tools/`：guest 侧稳定性工具，提供并发 worker、错误注入和状态读取命令
- `guest/`：自动化入口 `/init`
- `scripts/`：宿主机构建 / 运行 / 提取记录 / 生成总结
- `records/`：每轮运行的原始留证
- `output/`：稳定性模板与自动生成摘要
