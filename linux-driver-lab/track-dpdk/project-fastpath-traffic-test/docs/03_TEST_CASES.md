# 03_TEST_CASES

## Case 1: RX smoke

目标：程序启动，统计打印。

命令：

```bash
sudo ./scripts/03_run_fastpath_rx.sh
```

通过：

```text
port 0 started
enter fastpath loop
fastpath-lite software stats
```

## Case 2: UDP traffic

目标：外部发包后 `rx/ipv4/udp` 非 0。

建议外部发包：

```bash
python3 tools/scapy_udp_sender.py --iface <sender-iface> --dst-mac <port0-mac> --dst-ip 192.168.100.1 --count 1000
```

通过：

```text
rx > 0
ipv4 > 0
udp > 0
```

## Case 3: UDP-only filter

目标：打开 UDP-only 后，非 UDP 包被 drop 计数记录。

通过：

```text
non_udp > 0 或 drop_non_udp > 0
```

## Case 4: Rewrite demo

目标：打开 rewrite 后，确认配置加载，并在真实 UDP 流量中触发 rewrite。

通过：

```text
rewrite_enable=1
rewrite > 0
```

## Case 5: Forwarding

目标：双端口或虚拟端口下 `rx/tx` 都非 0。

通过：

```text
port0 rx > 0
port1 tx > 0
```
