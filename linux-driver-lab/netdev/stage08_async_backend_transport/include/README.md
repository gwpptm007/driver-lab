# include

本目录放 stage08 的兼容头与后续可扩展头。

当前先保留：

- `netdev_stage08_compat.h`

后续如果 stage08 再拆：
- queue helper
- backend helper
- timeline helper

也建议优先在 `include/` 沉淀公共头，而不是直接把所有逻辑堆进单一 `.c`。
