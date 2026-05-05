# COMMANDS


## 2026-05-05 18:10:47
```bash
./scripts/00_check_env.sh 
```

## 2026-05-05 18:11:17
```bash
./scripts/01_build_app.sh 
```

## 2026-05-05 18:11:29
```bash
sudo ./scripts/02_prepare_vmxnet3.sh 
```

## 2026-05-05 18:16:13
```bash
./scripts/01_build_app.sh 
```

## 2026-05-05 18:16:35
```bash
sudo ./scripts/02_prepare_vmxnet3.sh 
```

## 2026-05-05 18:16:48
```bash
sudo /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk/lab-dpdk-l2-forwarding/app/build/l2fwd-lite -l 0-1 -n 4 --file-prefix l2fwd_lite -a 0000:0b:00.0 -- --run-seconds 15 --stats-period 2 --burst-size 32 --promisc 1 
```

## 2026-05-05 18:17:22
```bash
./scripts/06_collect_stats.sh 
```

## 2026-05-05 18:17:31
```bash
./scripts/07_make_review_bundle.sh 
```

## 2026-05-05 18:21:35
```bash
./scripts/07_make_review_bundle.sh 
```

## 2026-05-05 18:25:41
```bash
./scripts/07_make_review_bundle.sh 
```
