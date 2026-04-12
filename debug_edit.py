#!/usr/bin/env python3
import sys

filepath = 'e:/02_Learning/2026/gitcode/driver-lab/linux-driver-lab/netdev/stage04_ring_dma/driver/netdev_stage04.c'

with open(filepath, 'r') as f:
    content = f.read()

old = '''		rxd->data_len = copy_len;
		rxd->owner = STAGE04_OWNER_CPU;
		rxd->state = STAGE04_DESC_DONE;'''

new = '''		rxd->data_len = copy_len;
		pr_info("[stage04] TX set data_len=%u state=DONE\\n", copy_len);
		rxd->owner = STAGE04_OWNER_CPU;
		rxd->state = STAGE04_DESC_DONE;'''

if old in content:
    content = content.replace(old, new, 1)
    with open(filepath, 'w') as f:
        f.write(content)
    print("Edit successful")
else:
    print("Pattern not found")
    # Debug: show first few chars around "rxd->data_len"
    idx = content.find('rxd->data_len = copy_len;')
    if idx >= 0:
        print(f"Found at idx {idx}")
        print(repr(content[idx-50:idx+100]))