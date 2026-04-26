# 03_TRACE_PLAN

## 原则

少而准，不追求一次抓太多点。

## 第一批推荐点

### 核心点
- 与 `napi poll` 直接相关的点
- 与 `netif_receive_skb` 或 RX 上送相关的点
- 与 queue/callback 最接近、又容易采到的点

### 当前建议
1. poll 相关证据
2. RX 收包计数
3. 简单前后差值
4. dmesg/tail 作为辅助

## 当前不建议
- 一上来启用很多 tracepoint
- 把 trace 配置搞得过重，反而影响运行
- 还没形成第一轮结论就不停改采集点
