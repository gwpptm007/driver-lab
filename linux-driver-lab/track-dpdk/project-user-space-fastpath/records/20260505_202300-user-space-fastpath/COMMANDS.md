# COMMANDS


## 2026-05-05 20:23:00
```bash
./scripts/00_check_env.sh 
```

## 2026-05-05 20:23:12
```bash
./scripts/01_build_app.sh 
```

## 2026-05-05 20:23:25
```bash
sudo ./scripts/02_prepare_vmxnet3.sh 
```

## 2026-05-05 20:23:38
```bash
sudo /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/project-user-space-fastpath/app/build/fastpath-lite -l 0-1 -n 4 --file-prefix fastpath_lite -a 0000:0b:00.0 -- --run-seconds 20 --stats-period 2 --burst-size 32 --promisc 1 --udp-only 1 --swap-mac 1 --rewrite 0 
```

## 2026-05-05 20:24:46
```bash
./scripts/07_collect_stats.sh 
```

## 2026-05-05 20:24:51
```bash
./scripts/08_make_review_bundle.sh 
```

## 2026-05-05 20:26:23
```bash
./scripts/08_make_review_bundle.sh 
```
