# tools

## stage02 用户态工具

### send_stage02_frame
向指定接口发送一帧 ETHERTYPE 可配置的原始二层帧，用于触发 `ndo_start_xmit()`。

### recv_stage02_frame
绑定到指定接口与 ETHERTYPE，等待接收一帧软件环回注入回来的包。

## 推荐配合
先起 receiver，再运行 sender，最后查看：
- `ip -s link`
- debugfs stats
- receiver 输出
