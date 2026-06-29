#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
LOG="${RECORD_DIR}/L3_FORWARD.log"
REPORT="${RECORD_DIR}/SUMMARY.md"
[[ -f "$LOG" ]] || { echo "[ERR] missing $LOG" >&2; exit 1; }

result_line=$(grep '^RESULT ' "$LOG" | tail -1)
route_line=$(grep '^ROUTE_STATS\[0\]' "$LOG" | tail -1)
acl_line=$(grep '^ACL_STATS\[0\]' "$LOG" | tail -1)
route_cfg=$(grep '^ROUTE\[0\]' "$LOG" | tail -1 || true)
acl_cfg=$(grep '^ACL\[0\]' "$LOG" | tail -1 || true)

get_num() {
  local key="$1" line="$2"
  echo "$line" | sed -n "s/.*${key}=\\([0-9][0-9]*\\).*/\\1/p"
}

rx=$(get_num rx_packets "$result_line")
fwd=$(get_num forwarded_packets "$result_line")
acl=$(get_num acl_drops "$result_line")
miss=$(get_num route_miss_drops "$result_line")
txfail=$(get_num tx_failed "$result_line")
route_hits=$(get_num hits "$route_line")
acl_drops=$(get_num drops "$acl_line")

pass_build=$(grep -q 'dpdk-l3-forwarder-lite' "${RECORD_DIR}/BUILD.log" && echo PASS || echo FAIL)
pass_route=$([[ -n "$route_cfg" && "$route_hits" -gt 0 ]] && echo PASS || echo FAIL)
pass_fwd=$([[ "$fwd" -gt 0 && "$txfail" -eq 0 ]] && echo PASS || echo FAIL)
pass_acl=$([[ -n "$acl_cfg" && "$acl" -gt 0 && "$acl_drops" -eq "$acl" ]] && echo PASS || echo FAIL)
pass_stats=$([[ "$rx" -gt 0 && "$route_hits" -eq "$fwd" ]] && echo PASS || echo FAIL)
pass_pcap=$([[ -s "$PCAP_FILE" && "$rx" -gt 0 ]] && echo PASS || echo FAIL)

{
  echo "# L3 Forwarder Lite Summary"
  echo
  echo "| Item | Result |"
  echo "|------|--------|"
  echo "| PASS_BUILD | ${pass_build} |"
  echo "| PASS_ROUTE_CONFIG | ${pass_route} |"
  echo "| PASS_L3_FORWARD | ${pass_fwd} |"
  echo "| PASS_ACL_DROP | ${pass_acl} |"
  echo "| PASS_PER_RULE_STATS | ${pass_stats} |"
  echo "| PASS_PCAP_EVIDENCE | ${pass_pcap} |"
  echo
  echo "## Raw stats"
  echo
  echo "- ${result_line}"
  echo "- ${route_line}"
  echo "- ${acl_line}"
  echo "- route_miss_drops=${miss}"
} > "$REPORT"
cat "$REPORT"
echo "[OK] summary saved: $REPORT"

