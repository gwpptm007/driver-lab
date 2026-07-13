# DPDK Flow Pipeline Tuning Matrix

| case | burst | cache | rules | samples | p50 cycles | p99 cycles | p99 ns | vs baseline |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| baseline | 16 | 250 | 3 | 4096 | 75 | 125 | 50 | 0.0% |
| burst_1 | 1 | 250 | 3 | 4096 | 75 | 100 | 40 | -20.0% |
| burst_32 | 32 | 250 | 3 | 4096 | 75 | 100 | 40 | -20.0% |
| burst_64 | 64 | 250 | 3 | 4096 | 75 | 125 | 50 | 0.0% |
| cache_0 | 16 | 0 | 3 | 4096 | 75 | 125 | 50 | 0.0% |
| rules_512 | 16 | 250 | 512 | 4096 | 75 | 100 | 40 | -20.0% |
| rules_64 | 16 | 250 | 64 | 4096 | 75 | 100 | 40 | -20.0% |

> p99 只覆盖 parse + software hash lookup + decision。
