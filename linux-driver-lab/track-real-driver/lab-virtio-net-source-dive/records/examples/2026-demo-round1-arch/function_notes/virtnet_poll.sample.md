# FUNCTION NOTE

## 基本信息
- 函数名：`virtnet_poll`（示范名）
- 所属分组：RX / NAPI 组
- 关注轮次：Round1 / Round2

## 这段函数负责什么
承接 RX 方向的 poll 处理，是理解 `virtio_net` 与 `stage03_napi_poll` 对应关系的关键入口。

## 它的上游是谁
- callback / napi schedule / 中断路径

## 它的下游是谁
- RX 包处理
- 预算控制
- 可能的 completion / refill 协同

## 和自己项目的哪个 stage 最像
- `stage03_napi_poll`
- `stage11_page_pool_rx`

## 这段代码中最容易忽略的点
NAPI 并不是单独存在的，它必须和 queue / callback / refill 一起看。

## 下一步还要追哪些函数
- RX buffer refill
- skb 构建相关函数
- GRO/checksum/XDP 边界函数
