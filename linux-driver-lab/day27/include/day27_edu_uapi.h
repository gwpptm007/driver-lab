/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DAY27_EDU_UAPI_H
#define DAY27_EDU_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define DAY27_TOOL_API_VERSION 1

struct day27_info {
    __u32 tool_api_version;
    __u16 vendor_id;
    __u16 device_id;
    __u32 irq_vector;
    __u64 irq_count;
    __u32 last_irq_status;
    __u32 last_ack_value;
    __u64 bar0_start;
    __u64 bar0_len;
    __u32 msi_enabled;
};

struct day27_irq_count {
    __u64 count;
};

#define DAY27_IOC_MAGIC 'K'
#define DAY27_IOC_GET_INFO      _IOR(DAY27_IOC_MAGIC, 0x01, struct day27_info)
#define DAY27_IOC_GET_IRQ_COUNT _IOR(DAY27_IOC_MAGIC, 0x02, struct day27_irq_count)
#define DAY27_IOC_RESET_STATS   _IO(DAY27_IOC_MAGIC,  0x03)

#endif
