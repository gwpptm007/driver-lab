#!/usr/bin/env bash
set -euo pipefail
OUT=${1:?usage: $0 <record-dir>}
mkdir -p "$OUT"

cat > "$OUT/FINAL_TOPOLOGY.md" <<'EOF'
# FINAL TOPOLOGY

```text
             host
+--------------------------------+
|              br-vnet0          |
|        192.168.100.1/24        |
+----------+-------------+-------+
           |             |
     tap-vnet-a     tap-vnet-b
           |             |
        QEMU A        QEMU B
           |             |
     guest A eth0   guest B eth0
     192.168.100.2  192.168.100.3
```

## vhost mode
- vhost=off:
- vhost=on:
EOF

echo "$OUT/FINAL_TOPOLOGY.md"
