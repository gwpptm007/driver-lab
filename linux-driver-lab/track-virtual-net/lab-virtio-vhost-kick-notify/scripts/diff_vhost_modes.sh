#!/usr/bin/env bash
set -euo pipefail
REC=${1:?usage: $0 <record-dir>}
OUT="$REC/vhost_mode_diff"
mkdir -p "$OUT"

for name in ip_br_link ip_s_link bridge_link bridge_fdb modules dev_nodes qemu_process; do
    OFF="$REC/off/state/${name}.txt"
    ON="$REC/on/state/${name}.txt"
    if [[ -f "$OFF" && -f "$ON" ]]; then
        diff -u "$OFF" "$ON" > "$OUT/${name}.diff" || true
    fi
done

cat > "$OUT/README.md" <<'EOF'
# vhost mode diff

这里保存 `vhost=off` 与 `vhost=on` 的 host state diff。

注意：
- 第一轮不要求 diff 一定非常明显。
- 关键是 qemu args、vhost_net 模块、/dev/vhost-net、连通性记录齐全。
EOF

echo "$OUT"
