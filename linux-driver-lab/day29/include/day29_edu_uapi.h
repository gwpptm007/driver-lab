/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DAY29_EDU_UAPI_H
#define DAY29_EDU_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define DAY29_TOOL_API_VERSION 1

struct day29_info {
    __u32 tool_api_version;
    __u16 vendor_id;
    __u16 device_id;
    __u32 irq_vector;
    __u64 irq_count;
    __u32 last_irq_status;
    __u32 last_ack_value;
    __u64 bar0_start;
    __u64 bar0_len;
    __u64 dma_handle;
    __u32 dma_bytes;
    __u32 dma_mask_bits;
    __u32 msi_enabled;
    __u32 last_verify_len;
    __u32 last_verify_seed;
    __u32 last_verify_ok;
    __s32 last_verify_error;
    __s32 last_mismatch_index;
    __u8  last_mismatch_expected;
    __u8  last_mismatch_actual;
    __u16 __pad0;
    __u32 last_irq_delta;
    __u32 last_dma_cmd;
};

struct day29_verify_req {
    __u32 len;
    __u32 pattern_seed;
};

struct day29_verify_result {
    __u32 verify_len;
    __u32 verify_seed;
    __u32 verify_ok;
    __s32 verify_error;
    __s32 mismatch_index;
    __u8  mismatch_expected;
    __u8  mismatch_actual;
    __u16 __pad0;
    __u32 irq_delta;
    __u32 last_dma_cmd;
};

#define DAY29_IOC_MAGIC 'L'
#define DAY29_IOC_GET_INFO          _IOR(DAY29_IOC_MAGIC, 0x01, struct day29_info)
#define DAY29_IOC_RUN_VERIFY        _IOW(DAY29_IOC_MAGIC, 0x02, struct day29_verify_req)
#define DAY29_IOC_GET_VERIFY_RESULT _IOR(DAY29_IOC_MAGIC, 0x03, struct day29_verify_result)
#define DAY29_IOC_RESET_STATS       _IO(DAY29_IOC_MAGIC,  0x04)

#endif
