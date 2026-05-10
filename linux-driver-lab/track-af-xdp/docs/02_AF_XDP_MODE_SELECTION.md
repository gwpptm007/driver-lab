# 02_AF_XDP_MODE_SELECTION

AF_XDP 需要同时区分：

```text
XDP attach mode: skb / native
AF_XDP bind mode: copy / zero-copy
```

本 track 的第三站 `lab-af-xdp-zero-copy-vs-copy` 专门验证当前测试机 `ens192/vmxnet3` 对这些组合的支持边界。

结论不要预设为 zero-copy 一定成功。虚拟网卡或驱动不支持时，正确结果是：

```text
记录错误 -> 判定不支持 -> fallback copy mode -> 继续后续 mini forwarder
```
