# SUMMARY

## 本轮目标
在 host 上建立 Linux bridge + TAP 设备，QEMU guest 通过 virtio-net-pci + tap 后端打通网络路径，验证 guest 能 ping 通 host bridge IP。

## 拓扑
- host: br-vnet0 = 192.168.100.1/24, tap-vnet0 attached to br-vnet0
- guest: eth0 = 192.168.100.2/24 (virtio-net-pci)
- QEMU: -netdev tap,id=net0,ifname=tap-vnet0,script=no,downscript=no -device virtio-net-pci,netdev=net0

## 已执行命令
```bash
# host 上手工执行
sudo ip link add name br-vnet0 type bridge
sudo ip addr add 192.168.100.1/24 dev br-vnet0
sudo ip link set br-vnet0 up

sudo ip tuntap add dev tap-vnet0 mode tap user wq7
sudo ip link set tap-vnet0 master br-vnet0
sudo ip link set tap-vnet0 up

# guest 内
ip addr add 192.168.100.2/24 dev eth0
ip link set eth0 up
ping 192.168.100.1
```

## 结果
- ping 测试: 5 packets transmitted, 5 received, 0% packet loss, RTT min/avg/max = 0.529/0.606/0.717 ms
- bridge fdb 学习到 guest MAC 52:54:00:12:34:56 → tap-vnet0
- tap-vnet0 state UP, master br-vnet0

## 当前结论
- guest virtio_net → QEMU tap 后端 → host tap-vnet0 → bridge → host IP栈 完整路径验证通过
- RTT < 1ms 表明路径延迟很低
- bridge FDB 学习机制工作正常，后续同目的 MAC 的帧按 FDB 命中直接转发到对应端口，无需 flooding；但 Linux bridge 是 host kernel 软件桥，转发仍然经过 host CPU

## 问题与下一步
- 收尾 SUMMARY/TOPOLOGY_NOTE
- 下一步: 运行 tcpdump 抓包记录，验证 ICMP frame 细节