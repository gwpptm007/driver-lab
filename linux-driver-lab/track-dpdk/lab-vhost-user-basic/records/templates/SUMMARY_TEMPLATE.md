# SUMMARY

## Lab

lab-vhost-user-basic

## Result

PASS / FAIL / PASS_WITH_WARN

## Key Evidence

- ENV_CHECK.txt
- HUGEPAGE_SETUP.txt
- TESTPMD_COMMAND.txt
- TESTPMD_VHOST.log
- VHOST_SOCKET.txt
- REVIEW_BUNDLE.md

## Notes

- RX/TX 为 0 是否正常：正常，当前无 frontend。
- 是否影响物理网卡：不应影响，本实验使用 `--no-pci`。
