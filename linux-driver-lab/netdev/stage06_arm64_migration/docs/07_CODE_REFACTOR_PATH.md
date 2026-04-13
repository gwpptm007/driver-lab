# 07_CODE_REFACTOR_PATH

## 为什么 stage06 也需要代码交付

因为 stage06 不应只有文档和脚本。

本阶段虽然不新增大数据面功能，但需要准备：
- 兼容层头文件
- 平台参数默认值
- 为 stage04 / 后续驱动留出可复用入口

## 当前代码交付

### `include/netdev_kcompat.h`
提供：
- NAPI 兼容包装
- `u64_stats` 兼容包装
- 常见内核版本判断

### `include/netdev_stage_port_profile.h`
提供：
- host / x86_64 / arm64 的推荐默认参数
- ring / napi_weight / rx_buf_size 的平台建议

## 后续如何接到 stage04

可以把 stage04 当前内联的兼容宏，逐步收敛为：

```c
#include "../../stage06_arm64_migration/include/netdev_kcompat.h"
#include "../../stage06_arm64_migration/include/netdev_stage_port_profile.h"
```

然后把：
- `STAGE04_NETIF_NAPI_ADD`
- `STAGE04_U64_*`
- 平台默认值

逐步替换为 stage06 里的统一定义。
