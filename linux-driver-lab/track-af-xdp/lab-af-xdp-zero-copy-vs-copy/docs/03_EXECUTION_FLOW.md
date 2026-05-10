# 03_EXECUTION_FLOW

```text
check env
  ↓
build BPF + user app
  ↓
ensure ens192 is vmxnet3 kernel driver
  ↓
run skb/copy baseline
  ↓
probe native/copy
  ↓
probe native/zero-copy
  ↓
compare logs and classify status
  ↓
make review bundle
```

本实验允许部分模式失败。失败本身是结果，重点是记录：

- 参数；
- attach mode；
- bind mode；
- 返回码；
- 典型错误；
- fallback 建议。
