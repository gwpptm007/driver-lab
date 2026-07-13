#ifndef FLOW_CAPABILITY_H
#define FLOW_CAPABILITY_H

#include <stdint.h>

/* 只探测并打印 PMD 能力，不创建硬件规则，也不改变端口运行状态。 */
void flow_capability_probe(uint16_t port_id);

#endif
