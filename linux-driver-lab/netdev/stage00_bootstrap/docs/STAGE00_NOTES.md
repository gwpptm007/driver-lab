# Stage00 说明

## 为什么这里不默认 ARM64
因为本阶段目的是先搭骨架，不是开始做平台迁移。

## 后续如何扩展
只要 env/scripts 使用统一变量，后面增加 `TARGET_ARCH=arm64` 即可。
