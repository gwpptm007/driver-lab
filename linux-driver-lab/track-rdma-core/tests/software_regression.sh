#!/usr/bin/env bash
set -euo pipefail

track_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
skip_clean="${SKIP_CLEAN:-0}"
extended="${EXTENDED_REGRESSION:-0}"
prepare_rxe="${PREPARE_RXE:-0}"

run_make_test() {
  local target="$1"

  echo "RDMA_REGRESSION_BEGIN target=${target}"
  if [[ "${skip_clean}" != "1" ]]; then
    make -C "${track_root}/${target}" clean
  fi
  make -C "${track_root}/${target}" test
  echo "RDMA_REGRESSION_PASS target=${target}"
}

"${track_root}/tests/check_fundamentals.sh"

if [[ "${prepare_rxe}" == "1" ]]; then
  "${track_root}/tests/prepare_rxe.sh"
fi

# 六个基础实验覆盖对象、MR、QP、RC、one-sided 和 UD/RoCEv2 语义。
for lab in \
  lab-rdma-verbs-object-lifecycle \
  lab-rdma-memory-region-deep-dive \
  lab-rdma-qp-state-machine \
  lab-rdma-rc-pingpong \
  lab-rdma-one-sided-read-write \
  lab-rdma-ud-rocev2-model; do
  run_make_test "${lab}"
done

# 扩展模式用于阶段收口，可能需要调用 sudo 配置 RXE，因此凭据只从环境传入。
if [[ "${extended}" == "1" ]]; then
  for project in \
    project-rdma-rc-client-server \
    project-rdma-performance-tuning \
    project-rdma-one-sided-kv; do
    run_make_test "${project}"
  done
fi

echo "RDMA_FUNDAMENTALS_AND_SOFTWARE_REGRESSION_PASS extended=${extended} prepare_rxe=${prepare_rxe}"

