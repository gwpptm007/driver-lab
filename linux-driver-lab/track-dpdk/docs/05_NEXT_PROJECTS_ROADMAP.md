# 05_NEXT_PROJECTS_ROADMAP

## 目标

把当前 `project-user-space-fastpath` 的 `PASS_SMOKE` 结果继续推进为可以面试展示的 `PASS_PROJECT`。

## 当前卡点

`project-user-space-fastpath` 已经能证明：

- `fastpath-lite` 可以编译
- EAL / hugepage / mempool / ethdev 初始化正常
- vmxnet3 PMD 可以 probe
- port 0 可以启动
- 软件 stats 可以打印

但还不能证明：

- 真实 UDP 包进入 DPDK RX 队列
- `ipv4/udp/non_udp` 分类计数被真实包触发
- `udp_only` 能过滤非 UDP
- rewrite 规则能命中
- 双端口或虚拟端口能形成转发闭环

## 后续阶段

### 1. project-fastpath-traffic-test

定位：测试工程，不是新数据面。

目标：用外部发包源或 vhost/virtio-user 拓扑，证明已有 `fastpath-lite` 能处理真实流量。

验收等级：

```text
PASS_TRAFFIC:
  rx > 0
  ipv4/udp/non_udp 至少一类非 0
  records 可复盘

PASS_REWRITE:
  rewrite_enable=1
  rewrite counter 非 0 或日志能证明规则被加载并等待触发

PASS_FORWARDING:
  有双端口/虚拟端口
  rx/tx 均非 0
```

### 2. project-dpdk-media-gateway-lite

定位：简化版用户态媒体网关。

原前置条件是 `project-fastpath-traffic-test` 至少达到 `PASS_TRAFFIC`，但当前选择先推进 v17 legacy review；media gateway 真实流量后续补。

功能方向：

- 多方向 rule table
- UDP-only media path
- MAC/IP/UDP port rewrite
- ARP 基础处理策略
- 按方向、按规则、按 drop reason 统计
- 配置文件驱动
- records + interview notes

### 3. project-dpdk-v17-legacy-review - CURRENT

定位：把既有 DPDK v17 项目经验和当前现代 DPDK track 对齐。

输出：

- v17 Makefile/KNI/uio 模型说明
- 21.11 meson/vfio-or-uio/vhost/virtio-user 模型说明
- 媒体面收包、重写、转发逻辑迁移表
- 面试讲法

## 一句话总结

先证明流量，再项目化；先做 `traffic-test`，再做 `media-gateway-lite`。
