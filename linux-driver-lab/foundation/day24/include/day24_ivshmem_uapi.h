/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DAY24_IVSHMEM_UAPI_H
#define DAY24_IVSHMEM_UAPI_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
typedef uint32_t __u32;
typedef uint64_t __u64;
#endif

#define DAY24_IOC_MAGIC 'q'

#define DAY24_IVSHMEM_VENDOR_ID 0x1af4
#define DAY24_IVSHMEM_DEVICE_ID 0x1110

#define DAY24_PROTO_MAGIC        0x44593234U /* "DY24" */
#define DAY24_PROTO_VERSION      1U
#define DAY24_PROTO_OFF_MAGIC    0x00U
#define DAY24_PROTO_OFF_VERSION  0x04U
#define DAY24_PROTO_OFF_SEQ      0x08U
#define DAY24_PROTO_OFF_STATE    0x0cU
#define DAY24_PROTO_OFF_LEN      0x10U
#define DAY24_PROTO_PAYLOAD_OFF  0x20U
#define DAY24_PROTO_MAX_PAYLOAD  256U

#define DAY24_STATE_EMPTY        0U
#define DAY24_STATE_READY        1U
#define DAY24_STATE_USER_WRITTEN 2U
#define DAY24_STATE_USER_TOUCHED 3U

struct day24_bar_info_uapi {
    __u32 index;
    __u32 reserved;
    __u64 start;
    __u64 end;
    __u64 len;
    __u64 flags;
};

struct day24_info_uapi {
    __u32 vendor;
    __u32 device;
    __u32 bar0_first_dword;
    __u32 proto_magic;
    __u32 proto_version;
    __u32 proto_seq;
    __u32 proto_state;
    __u32 proto_payload_len;
    struct day24_bar_info_uapi bar0;
    struct day24_bar_info_uapi bar2;
};

struct day24_mmio32_uapi {
    __u32 offset;
    __u32 value;
};

#define DAY24_IOC_GET_INFO      _IOR(DAY24_IOC_MAGIC, 0x01, struct day24_info_uapi)
#define DAY24_IOC_MMIO_READ32   _IOWR(DAY24_IOC_MAGIC, 0x02, struct day24_mmio32_uapi)
#define DAY24_IOC_MMIO_WRITE32  _IOW(DAY24_IOC_MAGIC, 0x03, struct day24_mmio32_uapi)
#define DAY24_IOC_CLEAR_PAYLOAD _IO(DAY24_IOC_MAGIC, 0x04)

#endif /* DAY24_IVSHMEM_UAPI_H */
