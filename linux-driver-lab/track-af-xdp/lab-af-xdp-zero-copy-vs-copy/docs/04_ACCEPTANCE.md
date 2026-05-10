# 04_ACCEPTANCE

## PASS_BUILD

`BUILD.log` 中出现：

```text
BUILD_RESULT=PASS
```

## PASS_COPY_BASELINE

`COPY_BASELINE.log` 中出现：

```text
af-xdp-rings config
bind=copy
UMEM_READY
XSK_SOCKET_READY
FILL_RING_READY
```

如果由于测试机队列/权限导致失败，也必须在 REVIEW_BUNDLE 中明确记录为 `COPY_BASELINE_FAIL`。

## ZERO_COPY_PROBED

只要执行了 zero-copy 探测并生成 `ZERO_COPY_PROBE.log`，就可以认为完成探测。

## PASS_ZERO_COPY

`ZERO_COPY_PROBE.log` 中出现：

```text
bind=zero-copy
XSK_SOCKET_READY
AF_XDP_FINAL_STATS
bye
```

## ACCEPTABLE_ZERO_COPY_FAIL

如果 `ZERO_COPY_PROBE.log` 出现 `xsk_socket__create`、`Operation not supported`、`Invalid argument` 等错误，也可以判定为合理失败，前提是 `COMPARE_MODES.txt` 说明 fallback 到 `copy`。
