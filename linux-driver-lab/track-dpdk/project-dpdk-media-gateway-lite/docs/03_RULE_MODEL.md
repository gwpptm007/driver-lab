# 03_RULE_MODEL

## 规则字段

每条规则包含：

```text
direction: in_port -> out_port
match:     src_ip / dst_ip / src_udp / dst_udp 可选
rewrite:   src_mac / dst_mac / src_ip / dst_ip / src_udp / dst_udp 可选
stats:     hit / bytes / rewrite
```

## 命令行示例

```bash
--rule0 0:1 \
--rule0-name access_to_core \
--rule0-dst-port 9000 \
--rule0-rewrite-dst-ip 10.10.20.20 \
--rule0-rewrite-dst-port 10000
```

## 默认规则

如果程序发现两个 DPDK 端口，但没有显式规则，会自动安装：

```text
port0 -> port1
port1 -> port0
```

如果只有一个端口，不安装默认转发规则，只做分类和 no-route drop 统计。
