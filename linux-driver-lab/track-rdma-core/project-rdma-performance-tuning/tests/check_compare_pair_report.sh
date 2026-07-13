#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

LEFT_CSV="${LEFT_CSV:?missing LEFT_CSV}"
RIGHT_CSV="${RIGHT_CSV:?missing RIGHT_CSV}"
LEFT_SUMMARY="${LEFT_SUMMARY:?missing LEFT_SUMMARY}"
RIGHT_SUMMARY="${RIGHT_SUMMARY:?missing RIGHT_SUMMARY}"
COMPARE_SUMMARY="${COMPARE_SUMMARY:?missing COMPARE_SUMMARY}"
LEFT_LABEL="${LEFT_LABEL:?missing LEFT_LABEL}"
RIGHT_LABEL="${RIGHT_LABEL:?missing RIGHT_LABEL}"

for path in \
  "${LEFT_CSV}" \
  "${RIGHT_CSV}" \
  "${LEFT_SUMMARY}" \
  "${RIGHT_SUMMARY}" \
  "${COMPARE_SUMMARY}"; do
  if [[ ! -f "${path}" ]]; then
    echo "missing_compare_input path=${path}" >&2
    exit 1
  fi
done

for heading in \
  "## 1. Best Throughput" \
  "## 2. Best Latency" \
  "## 3. Best Speedup" \
  "## 4. Notes"; do
  if ! grep -q "^${heading}$" "${COMPARE_SUMMARY}"; then
    echo "missing_compare_heading heading=${heading}" >&2
    exit 1
  fi
done

left_rows="$(grep -c "^| ${LEFT_LABEL} |" "${COMPARE_SUMMARY}")"
right_rows="$(grep -c "^| ${RIGHT_LABEL} |" "${COMPARE_SUMMARY}")"
if [[ "${left_rows}" -lt 3 || "${right_rows}" -lt 3 ]]; then
  echo "invalid_compare_rows left_rows=${left_rows} right_rows=${right_rows}" >&2
  exit 1
fi

echo "compare_report_check=pass path=${COMPARE_SUMMARY} left=${LEFT_LABEL} right=${RIGHT_LABEL} left_rows=${left_rows} right_rows=${right_rows}"
