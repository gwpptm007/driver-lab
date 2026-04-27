# COMMANDS

## 环境检查

```bash
./scripts/00_check_env.sh
```

## hugepage

```bash
sudo ./scripts/01_setup_hugepages.sh
```

## bind status

```bash
./scripts/02_bind_vmxnet3.sh status
```

## bind to vfio-pci

```bash
sudo DPDK_CONFIRM_BIND=YES ./scripts/02_bind_vmxnet3.sh bind
```

## run testpmd

```bash
sudo ./scripts/03_run_testpmd.sh
```

## collect

```bash
./scripts/04_collect_stats.sh
./scripts/05_make_review_bundle.sh
```
