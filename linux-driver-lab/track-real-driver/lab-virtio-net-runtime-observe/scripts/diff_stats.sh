#!/usr/bin/env bash
set -euo pipefail
BEFORE=${1:-}
AFTER=${2:-}
if [[ -z "$BEFORE" || -z "$AFTER" ]]; then
    echo "usage: $0 <before-file> <after-file>" >&2
    exit 1
fi

echo "== before =="
cat "$BEFORE"
echo
echo "== after =="
cat "$AFTER"
