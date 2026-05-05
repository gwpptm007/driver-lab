# tools

可选辅助工具。当前不会被默认验收脚本调用。

## scapy_udp_sender.py

用于在另一台 VM/宿主机上向 DPDK 口发 UDP 包，帮助把 `PASS_SMOKE` 升级到 `PASS_FORWARDING`。

示例：

```bash
sudo python3 tools/scapy_udp_sender.py --iface eth0 --dst-mac 00:0c:29:f8:f6:82 --dst-ip 192.168.100.20 --dport 6000 --count 100
```
