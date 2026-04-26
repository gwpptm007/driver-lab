#!/usr/bin/env bash
set -euo pipefail
MODE=${1:?usage: $0 <off|on> <record-dir>}
REC=${2:?usage: $0 <off|on> <record-dir>}
DIR=$(cd -- "$(dirname -- "$0")" && pwd)

case "$MODE" in
    on|off) ;;
    *) echo "mode must be on or off" >&2; exit 1 ;;
esac

OUT="$REC/$MODE"
mkdir -p "$OUT"

"$DIR/generate_qemu_vhost_args.sh" "$MODE" > "$OUT/qemu_net_args.txt"
"$DIR/collect_vhost_state.sh" "$OUT/state" >/dev/null

cat > "$OUT/MODE_RESULT_HINT.md" <<EOF
# MODE RESULT HINT

## mode
vhost=$MODE

## guest-side commands to record manually

Inside guest:

\`\`\`bash
ip addr
ip link
ping -c 5 192.168.100.1
\`\`\`

Save ping output to:
- $OUT/guest_ping_host.txt
EOF

echo "$OUT"
