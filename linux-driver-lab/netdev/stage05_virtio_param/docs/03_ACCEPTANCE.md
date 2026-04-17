# ACCEPTANCE

## 验收标准

### 验收 1：环境就绪

- [ ] `make report` 成功执行
- [ ] gcc 可用
- [ ] qemu-system-x86_64 / qemu-system-aarch64 可用（如适用）
- [ ] aarch64-linux-gnu-gcc 可用（如适用）
- [ ] `VIRTIO_NET_SOURCE` 指向有效的 virtio_net.c 路径

### 验收 2：virtio-net 阅读地图

- [ ] `make virtio-map` 成功生成 `output/virtio_net_map.md`
- [ ] 包含 6 个关键函数入口：probe / open / close / xmit / poll / try_fill_recv
- [ ] 每个函数有正确的行号

### 验收 3：stage04 ↔ virtio-net 对照报告

- [ ] `make compare` 成功生成 `output/stage04_vs_virtio_report.md`
- [ ] TX / RX / NAPI / DMA / ownership 五维对照完整
- [ ] 有清晰的"教学概念 → 真实实现"映射

### 验收 4：平台矩阵

- [ ] `make platform-matrix` 成功生成 `output/platform_matrix.md`
- [ ] 包含 host / qemu-x86_64 / qemu-arm64 三平台
- [ ] 每平台有 ARCH / RUN_MODE / CROSS_COMPILE / QEMU_BIN / KDIR 等参数

### 验收 5：一键 smoke

- [ ] `make smoke` 全部步骤 PASS
- [ ] 最终报告生成到 `output/stage05_report.md`

---

## 核心产物清单

| 产物文件 | 说明 |
|---------|------|
| `output/virtio_net_map.md` | virtio-net 源码阅读地图 |
| `output/stage04_vs_virtio_report.md` | stage04 ↔ virtio-net 对照报告 |
| `output/platform_matrix.md` | 三平台参数矩阵 |
| `output/resolved_*.env` | 各平台解析后的具体 env 值 |
| `output/stage05_report.md` | 阶段总报告 |

---

## 通过标准

> stage05 不编译新驱动，验证的是"环境就绪度"和"认知升级完成度"。

只要以上 5 项全部满足，即可判定 stage05 完成。
