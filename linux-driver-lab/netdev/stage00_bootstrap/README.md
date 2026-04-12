# stage00_bootstrap

本阶段不是开始写网络驱动功能，而是先把整个 `netdev/` 主线的启动骨架搭起来。

## 目标

- 架构中立
- 平台可参数化
- 路径探测与依赖检查清楚
- 可以为后续阶段复用

## 当前原则

- 默认不写死 ARM64
- 优先支持 `TARGET_ARCH=host`
- 后续再扩展 `x86_64` / `arm64` 路线

## 主要入口

```bash
make discover-paths
make check-host
make report
make all
```
