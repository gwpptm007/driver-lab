# 12_ROUND2_TXRX_CHECKLIST

> 目标：把 `virtio_net` 的 TX/RX 主路径读通，并形成自己的“路径图”。

## TX 本轮必须回答

1. `ndo_start_xmit` 从哪里进入？
2. skb 在哪里转换成 virtqueue 所需的数据结构？
3. 什么时机触发 notify / kick？
4. completion / reclaim 从哪里回来？
5. 真实驱动中的 TX 与你 `stage04/stage09/stage10` 的差异是什么？

## RX 本轮必须回答

1. receive buffer 是什么时候准备的？
2. 中断、callback、poll 三者如何衔接？
3. RX 包在什么位置构建成 skb？
4. GRO / checksum 在路径中的位置是什么？
5. 真实驱动中的 RX recycle 与你 `stage11_page_pool_rx` 的差异是什么？

## 推荐脚本

```bash
./scripts/extract_tx_path.sh
./scripts/extract_rx_path.sh
./scripts/trace_virtio_net_basic.sh
```

## 本轮产出物

建议目录：
```text
records/<date>-round2-txrx/
```

至少包含：
- `tx_path_snippets.txt`
- `rx_path_snippets.txt`
- `trace_sample.txt`
- `SUMMARY.md`

## 本轮通过标准

- 你能画出 TX 路径
- 你能画出 RX 路径
- 你能指出“教学驱动的简化点”和“真实 virtio_net 的复杂点”
