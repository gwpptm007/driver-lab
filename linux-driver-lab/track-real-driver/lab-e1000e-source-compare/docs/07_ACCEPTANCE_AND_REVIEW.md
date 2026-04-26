# 07_ACCEPTANCE_AND_REVIEW

## 最低通过标准

1. 能说明 `e1000/e1000e` 的设备模型和驱动骨架
2. 能给出 TX / RX / IRQ / NAPI / stats 的阅读骨架
3. 能和 `virtio_net` 做一版对照表
4. 能和自己 `netdev/stage00~stage13` 做一版映射表

## 标准通过

在最低通过基础上，再满足：
- 有一版函数索引
- 有关键片段导出
- 有最小运行环境验证记录
- 有一份结构化对照报告

## 优秀通过

- 能清楚说出为什么 `e1000/e1000e` 适合作为 `virtio_net` 之后的第二真实驱动
- 能自然引出后续 `track-virtual-net/`
