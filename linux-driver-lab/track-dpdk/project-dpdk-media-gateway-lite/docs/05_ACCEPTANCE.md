# 05_ACCEPTANCE

## PASS_BUILD

```text
app/build/media-gateway-lite 存在且可执行
```

## PASS_SMOKE

```text
media-gateway-lite starting
available/initialized ports
rules:
port X started
enter media gateway loop
media-gateway-lite software stats
bye
```

## PASS_RULE_CONFIG

```text
rule 0 name=...
rule 1 name=...
rewrite_dst_ip=...
rewrite_dst_port=...
```

## PASS_TRAFFIC

```text
rx > 0
ipv4 > 0
udp > 0
```

## PASS_FORWARDING

```text
tx > 0
rte_eth_stats opackets > 0
```

## PASS_REWRITE

```text
rewrite > 0
rule N rewrite > 0
```
