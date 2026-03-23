/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DAY32_EDU_UAPI_H
#define DAY32_EDU_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define DAY32_TOOL_API_VERSION 1

struct day32_info {
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

    __u32 map_bytes;
    __u32 src_off;
    __u32 dst_off;
    __u32 max_verify_len;

    __u64 total_run_calls;
    __u64 total_run_ok;
    __u64 total_run_fail;
    __u64 last_run_ns;

    __u32 last_run_len;
    __u32 last_run_seed;
    __u32 last_run_ok;
    __s32 last_run_error;
    __u32 last_irq_delta;
    __u32 last_dma_cmd;

    __u32 last_mmap_ok;
    __s32 last_mmap_error;
    __u32 last_mmap_len;
    __u32 last_mmap_pgoff;
};

struct day32_run_req {
    __u32 len;
    __u32 pattern_seed;
};

struct day32_run_result {
    __u64 total_run_calls;
    __u64 total_run_ok;
    __u64 total_run_fail;
    __u64 last_run_ns;

    __u32 run_len;
    __u32 run_seed;
    __u32 run_ok;
    __s32 run_error;
    __u32 irq_delta;
    __u32 last_dma_cmd;

    __u32 mmap_ok;
    __s32 mmap_error;
    __u32 mmap_len;
    __u32 mmap_pgoff;
};

#define DAY32_IOC_MAGIC 'B'
#define DAY32_IOC_GET_INFO        _IOR(DAY32_IOC_MAGIC, 0x01, struct day32_info)
#define DAY32_IOC_RUN_DMA         _IOW(DAY32_IOC_MAGIC, 0x02, struct day32_run_req)
#define DAY32_IOC_GET_RESULT      _IOR(DAY32_IOC_MAGIC, 0x03, struct day32_run_result)
#define DAY32_IOC_RESET_STATS     _IO(DAY32_IOC_MAGIC,  0x04)

#endif
