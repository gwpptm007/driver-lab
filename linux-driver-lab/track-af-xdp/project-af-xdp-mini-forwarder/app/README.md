# app

## Build

```bash
make clean
make all
```

## Run example

```bash
sudo ./build/af_xdp_forwarder   --ifname ens192   --queue 0   --mode skb   --copy   --forward drop   --duration 10   --interval 1   --obj ./build/af_xdp_forwarder_kern.bpf.o
```
