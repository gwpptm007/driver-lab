# 04_EXECUTION_FLOW

## 推荐流程

```text
00_check_env
    ↓
01_build_app
    ↓
02_prepare_kernel_netdev
    ↓
03_run_xdp_pass
    ↓
04_run_xdp_drop
    ↓
06_collect_stats
    ↓
07_make_review_bundle
```

## PASS 模式

用于确认 attach 和 stats：

```bash
sudo ./scripts/03_run_xdp_pass.sh
```

## DROP 模式

用于确认 action 控制，必须显式确认：

```bash
sudo AF_XDP_CONFIRM_DROP=YES ./scripts/04_run_xdp_drop.sh
```

## REDIRECT dry-run

用于确认 XSKMAP redirect 模型，必须显式确认：

```bash
sudo AF_XDP_CONFIRM_REDIRECT=YES ./scripts/05_run_xdp_redirect_dryrun.sh
```
