#!/usr/bin/env bash
set -euo pipefail
OUT_FILE=${1:-patches/0001-virtio_net-ethtool-stats-mini-patch.patch}
mkdir -p "$(dirname "$OUT_FILE")"
cat > "$OUT_FILE" <<'EOF'
From 0000000000000000000000000000000000000000 Mon Sep 17 00:00:00 2001
From: Your Name <you@example.com>
Date: Thu, 1 Jan 1970 00:00:00 +0000
Subject: [PATCH] virtio_net: ethtool/stats mini patch placeholder

Why:
- Explain why this patch point was selected.
- Explain why it is low-risk and easy to validate.

What:
- Briefly describe the exact control-plane/stats change.

How to validate:
- before/after ethtool -S
- before/after workload records
- review note linkage to source-dive/runtime-observe
---
 drivers/net/virtio_net.c | 0
 1 file changed, 0 insertions(+), 0 deletions(-)

diff --git a/drivers/net/virtio_net.c b/drivers/net/virtio_net.c
index 0000000..1111111 100644
--- a/drivers/net/virtio_net.c
+++ b/drivers/net/virtio_net.c
@@ -1 +1 @@
-/* placeholder */
+/* replace with real patch */
--
2.43.0
EOF
echo "$OUT_FILE"
