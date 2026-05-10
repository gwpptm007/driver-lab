# lab-af-xdp-zero-copy-vs-copy REVIEW_BUNDLE

## Environment

```text
LAB=af-xdp-zero-copy-vs-copy
DATE=2026-05-10T02:32:30+08:00
HOST=wq7-virtual-machine
KERNEL=6.8.0-111-generic
AF_XDP_IFACE=ens192
AF_XDP_MANAGEMENT_IFACE=ens33
AF_XDP_PCI=0000:0b:00.0
AF_XDP_DRIVER=vmxnet3
AF_XDP_QUEUE=0
AF_XDP_DURATION=8
AF_XDP_INTERVAL=1
```

## Evidence files

| File | Status |
|---|---|
| ENV_CHECK.txt | MISSING |
| BUILD.log | DONE |
| PREPARE_KERNEL_NETDEV.txt | DONE |
| COPY_BASELINE.log | DONE |
| NATIVE_COPY_PROBE.log | DONE |
| ZERO_COPY_PROBE.log | DONE |
| COMPARE_MODES.txt | DONE |
| COLLECT_STATS.txt | DONE |

## Mode comparison

```text
LAB=af-xdp-zero-copy-vs-copy
DATE=2026-05-10T02:32:30+08:00
HOST=wq7-virtual-machine
KERNEL=6.8.0-111-generic
AF_XDP_IFACE=ens192
AF_XDP_MANAGEMENT_IFACE=ens33
AF_XDP_PCI=0000:0b:00.0
AF_XDP_DRIVER=vmxnet3
AF_XDP_QUEUE=0
AF_XDP_DURATION=8
AF_XDP_INTERVAL=1

RECORD_DIR=/home/wq7/workspace/driver-lab/linux-driver-lab/track-af-xdp/lab-af-xdp-zero-copy-vs-copy/records/20260510-022436-af-xdp-zero-copy-vs-copy

COPY_BASELINE_RC=0
COPY_BASELINE_STATUS=PASS
COPY_BASELINE_FIRST_ERROR=libbpf: Error in bpf_create_map_xattr(xsks_map):ERROR: strerror_r(-524)=22(-524). Retrying without BTF.

NATIVE_COPY_RC=0
NATIVE_COPY_STATUS=PASS
NATIVE_COPY_FIRST_ERROR=libbpf: Error in bpf_create_map_xattr(xsks_map):ERROR: strerror_r(-524)=22(-524). Retrying without BTF.

ZERO_COPY_RC=1
ZERO_COPY_STATUS=UNSUPPORTED_OR_ATTACH_FAIL
ZERO_COPY_FIRST_ERROR=xsk_socket__create: Operation not supported

PASS_COPY_BASELINE=YES
ZERO_COPY_PROBED=YES
PASS_ZERO_COPY=NO
ZERO_COPY_RESULT=NOT_SUPPORTED_OR_FAILED_ON_THIS_ENV
FALLBACK=USE_SKB_COPY_OR_NATIVE_COPY_IF_AVAILABLE

== Interpretation ==
- skb/copy 是兼容性基线，最容易成功。
- native/copy 通过说明驱动支持 native XDP attach。
- native/zero-copy 成功才说明真正支持 AF_XDP ZC。
- vmxnet3 上 zero-copy 失败是可接受的，只要记录清楚 fallback 策略。
```

## Verdict guide

- `PASS_COPY_BASELINE=YES`：skb+copy 基线测试通过（XSK_SOCKET_READY 出现）。
- `ZERO_COPY_PROBED=YES`：zero-copy 探测已实际执行。
- `PASS_ZERO_COPY=YES`：native+zero-copy 成功，XSK socket 创建成功。
- vmxnet3 上零拷贝失败是可接受的，只要 fallback 策略已记录。
