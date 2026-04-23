# 11_ROUND1_ARCH_CHECKLIST

> 目标：先把 `virtio_net` 的“骨架”和“入口”看清，不急着深挖所有 helper。

## 本轮只回答 5 个问题

1. `virtio_net` 如何作为 `virtio_driver` 被注册？
2. `probe()` 的总体阶段顺序是什么？
3. 驱动私有结构体挂在哪里？
4. RX/TX queue 与 `napi_struct` 在哪里被组织起来？
5. `net_device_ops` / ethtool ops / feature 协商入口在哪里？

## 本轮执行步骤

### Step 1：生成函数清单
运行：
```bash
./scripts/collect_virtio_net_symbols.sh
./scripts/build_function_index.sh
./scripts/build_grouped_function_index.sh
```

### Step 2：抽 probe/remove 关键片段
运行：
```bash
./scripts/extract_probe_path.sh
```

### Step 3：人工阅读并填记录
使用：
- `records/templates/ROUND_SUMMARY_TEMPLATE.md`
- `records/templates/FUNCTION_NOTE_TEMPLATE.md`

## 本轮产出物

建议目录：
```text
records/<date>-round1-arch/
```

最少包含：
- `virtio_net_symbols.txt`
- `virtio_net_function_index.md`
- `virtio_net_grouped_index.md`
- `probe_path_snippets.txt`
- `SUMMARY.md`

## 本轮通过标准

- 你能不看源码，口头讲出 `probe -> alloc netdev -> init queues -> init napi -> register_netdev` 这类主链路
- 你能说明 `virtnet_info`、queue、napi、netdev 的关系
- 你能把“设备模型层 / netdev 层 / 数据路径层”三层分开讲
