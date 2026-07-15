#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

"${root}/tests/check_fundamentals.sh"
python3 -m py_compile "${root}/tests/check_fundamentals.py"

script_count=0
while IFS= read -r -d '' script; do
  # 这里只做语法解析，不执行可能修改网络、tracefs 或内核源码的实验脚本。
  bash -n "${script}"
  script_count=$((script_count + 1))
done < <(find "${root}" -type f -name '*.sh' -print0)

echo "REAL_DRIVER_SHELL_SYNTAX_PASS scripts=${script_count}"
echo "REAL_DRIVER_SOFTWARE_REGRESSION_PASS"
