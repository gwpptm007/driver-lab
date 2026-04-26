# next_step_after_vhost

## 当前 Lab 完成后

下一步建议进入：

```text
lab-two-guest-bridge-flow/
```

## 为什么

完成 vhost 后，你已经具备：

- 单 guest tap/bridge 基础路径
- userspace backend vs vhost backend 对照

下一步最自然就是扩展到：

```text
guest A -> tap A -> bridge -> tap B -> guest B
```

也就是双 guest L2 flow。
