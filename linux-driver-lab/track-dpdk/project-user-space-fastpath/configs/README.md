# configs

这里放项目脚本使用的环境变量样例。当前优先适配测试机：

- Ubuntu 22.04.5
- Linux 6.8.0-110-generic
- VMware Workstation guest
- 管理网卡：`ens33 / e1000 / 0000:02:01.0`
- DPDK 网卡：`ens192 / vmxnet3 / 0000:0b:00.0`
- 默认 driver：`uio_pci_generic`

使用方式：

```bash
source configs/fastpath-vmxnet3.env
sudo -E ./scripts/03_run_fastpath_single_port.sh
```
