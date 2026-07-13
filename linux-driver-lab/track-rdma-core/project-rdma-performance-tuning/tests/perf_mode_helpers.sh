#!/usr/bin/env bash

perf_mode_suffix() {
  local use_inline="${1:-0}"
  local signal_interval="${2:-1}"
  local poll_budget="${3:-16}"
  local suffix=""

  if [[ "${use_inline}" == "1" ]]; then
    suffix="${suffix}-inline"
  fi
  if [[ "${signal_interval}" != "1" ]]; then
    suffix="${suffix}-sig${signal_interval}"
  fi
  if [[ "${poll_budget}" != "16" ]]; then
    suffix="${suffix}-poll${poll_budget}"
  fi

  printf '%s' "${suffix}"
}

perf_sweep_csv_path() {
  local suffix

  suffix="$(perf_mode_suffix "${1:-0}" "${2:-1}" "${3:-16}")"
  if [[ -z "${suffix}" ]]; then
    printf '%s\n' "tests/perf-sweep.csv"
  else
    printf 'tests/perf-sweep%s.csv\n' "${suffix}"
  fi
}

perf_sweep_summary_path() {
  local suffix

  suffix="$(perf_mode_suffix "${1:-0}" "${2:-1}" "${3:-16}")"
  if [[ -z "${suffix}" ]]; then
    printf '%s\n' "tests/perf-sweep-summary.md"
  else
    printf 'tests/perf-sweep%s-summary.md\n' "${suffix}"
  fi
}

perf_sweep_dir_path() {
  local suffix

  suffix="$(perf_mode_suffix "${1:-0}" "${2:-1}" "${3:-16}")"
  if [[ -z "${suffix}" ]]; then
    printf '%s\n' "tests/sweep"
  else
    printf 'tests/sweep%s\n' "${suffix}"
  fi
}
