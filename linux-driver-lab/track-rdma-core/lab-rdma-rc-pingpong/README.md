# RDMA RC Ping-Pong

两个本地 RC QP 进入 RTS 后，执行 `left -> right` ping 和 `right -> left` pong，验证 WR、SGE、CQE 与 payload。

```bash
make clean && make
build/rdma-rc-pingpong --device rxe0 --port 1 --gid-index 1
make test
```

重点阅读 `src/main.c` 中 `post_recv()`、`post_send()`、`poll_two()`、`transfer()`，再读 `docs/RC_DATA_PATH.md` 和完整测试记录。
