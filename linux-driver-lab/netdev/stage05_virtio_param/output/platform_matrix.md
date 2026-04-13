# stage05 平台矩阵

| TARGET_ARCH | RUN_MODE | 典型用途 | 主要工具链 | 关键依赖 | 当前建议 |
|---|---|---|---|---|---|
| host | host | 文档/脚本/分析输出 | gcc=gcc | 无额外 QEMU 依赖 | 当前首选 |
| x86_64 | qemu-x86_64 | 未来可选 x86 QEMU 路线 | gcc=gcc | qemu-system-x86_64=yes | 可选 |
| arm64 | qemu-arm64 | stage06 ARM64 迁移主路线 | aarch64-linux-gnu-gcc=yes | qemu-system-aarch64=yes | 下一阶段重点 |
