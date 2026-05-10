# 02_MODE_MODEL

AF_XDP 至少要分清两组概念。

## XDP attach mode

```text
skb/native
```

- `skb`：generic XDP，兼容性好，但性能不是最终目标；
- `native`：driver XDP，依赖网卡驱动支持。

## AF_XDP bind mode

```text
copy/zero-copy
```

- `copy`：包数据需要拷贝，兼容性更强；
- `zero-copy`：UMEM frame 直接与驱动交互，依赖驱动实现。

## 组合判断

```text
skb + copy             基线
native + copy          native XDP 支持探测
native + zero-copy     ZC 支持探测
skb + zero-copy        多数情况下不成立，用作边界验证
```
