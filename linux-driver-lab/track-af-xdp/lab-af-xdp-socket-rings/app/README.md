# app: AF_XDP socket rings

This app contains two parts:

- `af_xdp_kern.bpf.c`: XDP program, redirects packets into `xsks_map[rx_queue_index]`.
- `af_xdp_rings.c`: user-space AF_XDP app, creates UMEM + FILL/COMPLETION/RX/TX rings and receives packets.

Build:

```bash
make
```

Run example:

```bash
sudo ./build/af_xdp_rings --ifname ens192 --queue 0 --mode skb --duration 15 --obj ./build/af_xdp_kern.bpf.o
```

Expected smoke markers:

```text
UMEM_READY
XSK_SOCKET_READY
FILL_RING_READY
XDP_ATTACHED
XSKMAP_REGISTERED
AF_XDP_RINGS_READY
AF_XDP_FINAL_STATS
bye
```
