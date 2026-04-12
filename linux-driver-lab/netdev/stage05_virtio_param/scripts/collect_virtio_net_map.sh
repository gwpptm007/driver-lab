#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT_FILE="$ROOT_DIR/output/virtio_net_map.md"
mkdir -p "$ROOT_DIR/output"
source "$ROOT_DIR/scripts/lib_stage05.sh"

VIRTIO_PATH=""
if ! VIRTIO_PATH=$(stage05_find_virtio_net_source 2>/dev/null); then
cat > "$OUT_FILE" <<'EOF'
# virtio-net 阅读地图

## 当前状态

- virtio_net.c：未发现

## 可以怎么继续

1. 设置 `VIRTIO_NET_SOURCE=/path/to/drivers/net/virtio_net.c`
2. 或设置 `KERNEL_SOURCE_ROOT=/path/to/linux-src`
3. 重新执行 `make virtio-map`
EOF
    echo "[stage05] virtio_net.c not found -> $OUT_FILE"
    exit 0
fi
KROOT=$(cd "$(dirname "$VIRTIO_PATH")/../.." && pwd)
RING_PATH="$KROOT/virtio/virtio_ring.c"
UAPI_PATH="$(cd "$(dirname "$VIRTIO_PATH")/../../.." && pwd)/include/uapi/linux/virtio_net.h"
map_hit() {
    local label=$1 regex=$2 hit
    hit=$(grep -nE "$regex" "$VIRTIO_PATH" | head -n 3 || true)
    if [[ -n "$hit" ]]; then
        printf '### %s

```text
%s
```

' "$label" "$hit"
    else
        printf '### %s

- 当前版本未直接匹配到。

' "$label"
    fi
}
{
    echo '# virtio-net 阅读地图'
    echo
    printf -- '- virtio_net.c: %s
' "$VIRTIO_PATH"
    printf -- '- virtio_ring.c: %s (%s)
' "$RING_PATH" "$( [[ -f "$RING_PATH" ]] && echo yes || echo no )"
    printf -- '- include/uapi/linux/virtio_net.h: %s (%s)
' "$UAPI_PATH" "$( [[ -f "$UAPI_PATH" ]] && echo yes || echo no )"
    echo
    echo '## 关键入口'
    echo
    map_hit 'probe' 'virtnet_probe\s*\('
    map_hit 'open' 'virtnet_open\s*\('
    map_hit 'close' 'virtnet_close\s*\('
    map_hit 'xmit' 'virtnet_xmit\s*\('
    map_hit 'poll' 'virtnet_poll\s*\('
    map_hit 'refill' 'try_fill_recv\s*\('
} > "$OUT_FILE"
echo "[stage05] generated virtio map -> $OUT_FILE"
