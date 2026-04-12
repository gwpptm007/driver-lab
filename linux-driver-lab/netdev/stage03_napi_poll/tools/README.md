# tools

## stage03 主要工具

- `send_stage03_frame`：支持 burst 发送的原始套接字发包工具
- `recv_stage03_frame`：支持 max_frames / timeout 的原始套接字收包工具

## 为什么 stage03 要支持 burst

因为 stage03 想看的是：
- pending queue
- NAPI poll
- budget

如果一次只发 1 帧，很难把这些现象观察出来。
