#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
rm -rf "${WORKDIR}"/*
find "${DAY34_ROOT}/driver" -name '*.o' -o -name '*.ko' -o -name '*.mod.c' -o -name '*.order' -o -name '*.symvers' | xargs -r rm -f
find "${DAY34_ROOT}/driver" -type d -name '.tmp_versions' -prune -exec rm -rf {} +
echo '[day34] clean complete'
