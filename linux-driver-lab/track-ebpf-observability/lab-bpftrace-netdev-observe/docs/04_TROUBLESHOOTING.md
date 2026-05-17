# 04_TROUBLESHOOTING

## BEGIN_trigger 报错

现象：

```text
ERROR: Could not resolve symbol: /proc/self/exe:BEGIN_trigger
```

处理：本版 `.bt` 脚本已移除 `BEGIN/END` block。重新使用：

```bash
sudo ./scripts/03_run_tracepoint_rx.sh
```

## kprobe / BTF 报错

现象：

```text
BTF: failed to find BTF data
```

处理：本 lab 不再强依赖 kprobe，使用 tracepoint 作为主路径。kprobe 只通过 `06_run_optional_kprobe.sh` 记录可用性。

## ens192 上有旧 XDP 程序

查看：

```bash
ip -d link show ens192
```

清理：

```bash
sudo EBPF_CONFIRM_XDP_OFF=YES ./scripts/02_clean_xdp_if_attached.sh
```

## 统计一直是 0

常见原因：

```text
目标网卡没有流量
XDP/AF_XDP/DPDK 改变了路径
观测时间太短
观察了 ens192，但流量实际在 ens33
```

可先用管理口 smoke：

```bash
BPFTRACE_IFACE=ens33 sudo ./scripts/03_run_tracepoint_rx.sh
BPFTRACE_IFACE=ens33 sudo ./scripts/04_run_tracepoint_tx.sh
```
