# 13_ROUND3_FEATURE_XDP_CHECKLIST

> 目标：把 feature、offload、ethtool、XDP 入口读清楚，并完成和 `stage12~stage14` 的映射。

## 本轮要回答的问题

1. `virtio_net` 的 feature negotiation 是在哪些阶段完成的？
2. 哪些 offload 能力通过 feature bits 暴露？
3. ethtool 相关能力在驱动里如何挂接？
4. XDP 在 `virtio_net` 里挂在哪条边界上？
5. `stage12_ethtool_control_plane`、`stage13_offload_basics`、`stage14_xdp_basics` 对应到真实驱动里的哪些部分？

## 建议阅读顺序

1. feature 协商与 capability 入口
2. ethtool ops
3. RX/TX 路径中的 offload 处理点
4. XDP attach / fast path 入口
5. 回填 `08_STAGE_TO_VIRTIO_NET_MAPPING.md`

## 本轮产出物

建议目录：
```text
records/<date>-round3-feature-xdp/
```

至少包含：
- `feature_notes.md`
- `ethtool_notes.md`
- `xdp_notes.md`
- `SUMMARY.md`

## 本轮通过标准

- 能说明 feature / offload / ethtool / XDP 的分层关系
- 能完成 `stage12~stage14` 到真实驱动的映射
- 能为下一步 `small patch` 或 tracing 找到切入点
