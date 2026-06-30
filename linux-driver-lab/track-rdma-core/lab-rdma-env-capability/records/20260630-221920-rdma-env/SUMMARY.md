# RDMA Env Capability Summary

- Record: `20260630-221920-rdma-env`
- Generated: `2026-06-30T22:20:18+08:00`

## Status

- BLOCKED_RDMA_TOOLS_MISSING: one or more rdma-core tools are missing.
- BLOCKED_NO_RDMA_DEVICE: no RDMA device was reported by current checks.
- PASS_SOFT_ROCE_AVAILABLE: `rdma_rxe` module metadata is available.

## Evidence Files

- `ENV_CHECK.log`
- `RDMA_CAPABILITY.log`
- `SOFT_ROCE_BOUNDARY.log`

## Next Step

No real RDMA device was detected. Consider explicit Soft-RoCE setup with `ENABLE_RXE_SETUP=1 RXE_NETDEV=<netdev>`.
