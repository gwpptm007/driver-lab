# app

## Build

```bash
make
```

Outputs:

```text
build/xdp_redirect_basics.bpf.o
build/xdp_loader
```

## Manual run

```bash
cd app/build
sudo ./xdp_loader run --ifname ens192 --mode skb --action pass --duration 10 --obj ./xdp_redirect_basics.bpf.o
```

Detach manually:

```bash
sudo ./xdp_loader detach --ifname ens192 --mode skb
```
