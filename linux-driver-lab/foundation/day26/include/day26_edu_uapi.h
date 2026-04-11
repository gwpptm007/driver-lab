#ifndef DAY26_EDU_UAPI_H
#define DAY26_EDU_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define DAY26_IOC_MAGIC 'r'
#define DAY26_TOOL_API_VERSION 1

struct day26_info {
    __u32 tool_api_version;
    __u16 vendor_id;
    __u16 device_id;
    __u64 bar0_start;
    __u64 bar0_len;
    __u32 irq_vector;
    __u64 irq_count;
    __u32 last_irq_status;
    __u32 last_ack_value;
    __u32 identity_value;
    __u32 liveness_value;
    __u32 liveness_inverted;
    __u32 msi_enabled;
};

struct day26_irq_count {
    __u64 count;
};

struct day26_irq_status {
    __u32 irq_status;
    __u32 ack_value;
};

#define DAY26_IOC_GET_INFO       _IOR(DAY26_IOC_MAGIC, 0x01, struct day26_info)
#define DAY26_IOC_GET_IRQ_COUNT  _IOR(DAY26_IOC_MAGIC, 0x02, struct day26_irq_count)
#define DAY26_IOC_GET_IRQ_STATUS _IOR(DAY26_IOC_MAGIC, 0x03, struct day26_irq_status)
#define DAY26_IOC_RESET_STATS    _IO(DAY26_IOC_MAGIC,  0x04)

#endif
