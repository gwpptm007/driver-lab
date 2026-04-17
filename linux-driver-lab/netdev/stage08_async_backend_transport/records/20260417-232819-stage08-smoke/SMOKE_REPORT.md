# stage08 smoke report

- ifname: nds8
- ethertype: 0x88B8
- count: 32
- timeout_sec: 5

本次 smoke 的重点不是极限性能，而是确认：

1. send -> submit
2. submit -> doorbell
3. doorbell -> backend worker
4. backend worker -> irq
5. irq -> napi poll
6. poll -> complete / consume / refill

请结合：
- debugfs_stats_after.txt
- debugfs_timeline_after.txt
- debugfs_queues_after.txt
- recv.txt
综合判断。
