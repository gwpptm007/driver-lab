# RX path sample

> 这是示范写法，不是源码逐行解析。

建议最终 RX 路径图至少包含：

1. RX buffer 准备/refill
2. callback / 中断触发
3. napi poll
4. 包提取/构建
5. GRO/checksum 处理点
6. XDP 边界
7. 上送协议栈或回收路径

最后一定要回答：
- `virtnet_poll` 在这条图里的位置是什么
- 自己的 `stage11_page_pool_rx` 和这里相比少了哪些真实复杂度
