# SUMMARY

## Lab

lab-virtio-user-vhost

## Result

PASS / FAIL / PASS_WITH_WARN

## Key Evidence

- ENV_CHECK.txt
- HUGEPAGE_SETUP.txt
- TESTPMD_COMMANDS.txt
- TESTPMD_BACKEND.log
- TESTPMD_FRONTEND.log
- VHOST_SOCKET.txt
- REVIEW_BUNDLE.md

## Notes

- RX/TX 为 0 是否正常：默认可接受，按 WARN 记录。
- 是否影响物理网卡：不应影响，本实验使用 `--no-pci`。
- 下一步：进入 `lab-dpdk-l2-forwarding`。
