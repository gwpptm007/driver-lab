#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

TF=$(tracefs_dir)

cat > "$TF/set_ftrace_filter" <<'EOF'
virtnet_poll
virtnet_*xmit*
*virtnet*complete*
*virtqueue*
napi_schedule*
EOF

echo "prepared trace filter in: $TF/set_ftrace_filter"
