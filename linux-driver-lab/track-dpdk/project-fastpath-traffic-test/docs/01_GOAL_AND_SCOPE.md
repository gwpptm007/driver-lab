# 01_GOAL_AND_SCOPE

## 目标

验证 `fastpath-lite` 能处理真实流量，而不只是启动成功。

## 范围内

- 复用 `project-user-space-fastpath/app/fastpath-lite`
- 真实 UDP 流量进入 DPDK RX
- 软件统计计数变化
- UDP-only 和 rewrite 测试记录
- 生成 review bundle

## 范围外

- 不新增复杂控制面
- 不重写 fastpath 主程序
- 不直接做完整媒体网关

## 与下一站关系

本项目通过后，才能进入：

```text
project-dpdk-media-gateway-lite
```
