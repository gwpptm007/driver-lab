#ifndef DAY25_EDU_UAPI_H
#define DAY25_EDU_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define DAY25_IOC_MAGIC 'q'

struct day25_info {
    __u16 vendor_id;
    __u16 device_id;
    __u64 bar0_start;
    __u64 bar0_len;
    __u32 irq_vector;
    __u64 irq_count;
    __u32 last_irq_status;
    __u32 last_ack_value;
    __u32 liveness_value;
    __u32 liveness_inverted;
    __u32 msi_enabled;
};

struct day25_trigger {
    __u32 value;
};

struct day25_irq_count {
    __u64 count;
};

struct day25_irq_status {
    __u32 irq_status;
    __u32 ack_value;
};

#define DAY25_IOC_GET_INFO      _IOR(DAY25_IOC_MAGIC, 0x01, struct day25_info)
#define DAY25_IOC_TRIGGER_IRQ   _IOW(DAY25_IOC_MAGIC, 0x02, struct day25_trigger)
#define DAY25_IOC_GET_IRQ_COUNT _IOR(DAY25_IOC_MAGIC, 0x03, struct day25_irq_count)
#define DAY25_IOC_GET_IRQ_STATUS _IOR(DAY25_IOC_MAGIC, 0x04, struct day25_irq_status)

#endif
