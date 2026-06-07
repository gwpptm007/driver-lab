#!/usr/bin/env bash
cat <<'EOF'
# Traffic hint for lab-libbpf-net-observer

编译后运行:
  sudo build/skb_observer -v -d 10

在另一个窗口制造流量:
  ping -i 0.1 <网关或对端 IP>

示例:
  ping -i 0.1 192.168.65.2

确认有流量后 observer 的 ringbuf 会输出事件。
EOF
