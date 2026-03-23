// SPDX-License-Identifier: GPL-2.0
/*
 * Day30 guest user tool
 *
 * 这版工具与 day29 最大的区别是：它真正去 mmap 驱动暴露出来的 DMA buffer，
 * 然后在用户态直接写 src、读 dst、做 compare。也就是说，零拷贝验证的主角
 * 已经从内核转移到了用户态。
 */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "day30_edu_uapi.h"

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s <dev> info\n"
		"  %s <dev> read-state\n"
		"  %s <dev> mmap-verify <len> <seed>\n"
		"  %s <dev> mmap-invalid-len <len>\n"
		"  %s <dev> mmap-invalid-offset <page_off>\n"
		"  %s <dev> result\n"
		"  %s <dev> reset-stats\n",
		prog, prog, prog, prog, prog, prog, prog);
}

static int get_info(int fd, struct day30_info *info)
{
	if (ioctl(fd, DAY30_IOC_GET_INFO, info) != 0) {
		perror("ioctl(GET_INFO)");
		return -1;
	}
	return 0;
}

static void fill_pattern(uint8_t *buf, uint32_t len, uint32_t seed)
{
	uint32_t i;

	for (i = 0; i < len; ++i)
		buf[i] = (uint8_t)((seed + i) & 0xff);
}

static int do_info(int fd)
{
	struct day30_info info;

	if (get_info(fd, &info) != 0)
		return 1;

	printf("tool_api_version=%u\n", info.tool_api_version);
	printf("vendor=0x%04x device=0x%04x\n", info.vendor_id, info.device_id);
	printf("irq_vector=%u irq_count=%" PRIu64 " msi_enabled=%u\n",
	       info.irq_vector, (uint64_t)info.irq_count, info.msi_enabled);
	printf("last_irq_status=0x%08x last_ack_value=0x%08x\n",
	       info.last_irq_status, info.last_ack_value);
	printf("bar0_start=0x%" PRIx64 " bar0_len=0x%" PRIx64 "\n",
	       (uint64_t)info.bar0_start, (uint64_t)info.bar0_len);
	printf("dma_handle=0x%" PRIx64 " dma_bytes=%u dma_mask_bits=%u\n",
	       (uint64_t)info.dma_handle, info.dma_bytes, info.dma_mask_bits);
	printf("map_bytes=%u src_off=%u dst_off=%u max_verify_len=%u\n",
	       info.map_bytes, info.src_off, info.dst_off, info.max_verify_len);
	printf("run_len=%u run_seed=0x%x run_ok=%u run_error=%d\n",
	       info.last_run_len, info.last_run_seed,
	       info.last_run_ok, info.last_run_error);
	printf("last_irq_delta=%u last_dma_cmd=0x%08x\n",
	       info.last_irq_delta, info.last_dma_cmd);
	printf("mmap_ok=%u mmap_error=%d mmap_len=%u mmap_pgoff=%u\n",
	       info.last_mmap_ok, info.last_mmap_error,
	       info.last_mmap_len, info.last_mmap_pgoff);
	return 0;
}

static int do_read_state(int fd)
{
	char buf[640];
	ssize_t n = pread(fd, buf, sizeof(buf) - 1, 0);

	if (n < 0) {
		perror("read-state");
		return 1;
	}
	buf[n] = '\0';
	fputs(buf, stdout);
	return 0;
}

static int do_result(int fd)
{
	struct day30_run_result res;

	if (ioctl(fd, DAY30_IOC_GET_RESULT, &res) != 0) {
		perror("ioctl(GET_RESULT)");
		return 1;
	}

	printf("run_len=%u\n", res.run_len);
	printf("run_seed=0x%x\n", res.run_seed);
	printf("run_ok=%u\n", res.run_ok);
	printf("run_error=%d\n", res.run_error);
	printf("irq_delta=%u\n", res.irq_delta);
	printf("last_dma_cmd=0x%08x\n", res.last_dma_cmd);
	printf("mmap_ok=%u\n", res.mmap_ok);
	printf("mmap_error=%d\n", res.mmap_error);
	printf("mmap_len=%u\n", res.mmap_len);
	printf("mmap_pgoff=%u\n", res.mmap_pgoff);
	return res.run_ok ? 0 : 1;
}

static int do_reset_stats(int fd)
{
	if (ioctl(fd, DAY30_IOC_RESET_STATS) != 0) {
		perror("ioctl(RESET_STATS)");
		return 1;
	}
	puts("reset-stats ok");
	return 0;
}

/*
 * 非法长度测试有一个容易踩坑的点：mmap() 的 length 会按页向上取整。
 * 例如在 4KB 页大小下，请求 2048 字节，内核最终建立的 VMA 长度仍然是 4096，
 * 这会和当前驱动允许的 map_bytes 恰好相等，从而让“本想测失败”的用例误变成成功。
 *
 * 因此 day30 自动化里要使用 4097 这类“跨页但又不等于 map_bytes”的长度，
 * 这样驱动看到的 len 会变成 8192，才能稳定验证 len != map_bytes 的拒绝路径。
 */
static int do_mmap_invalid_len(int fd, const char *len_arg)
{
	size_t bad_len;
	void *map;

	bad_len = (size_t)strtoul(len_arg, NULL, 0);
	map = mmap(NULL, bad_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map != MAP_FAILED) {
		munmap(map, bad_len);
		fprintf(stderr,
			"unexpected success: invalid length %zu should be rejected\n",
			bad_len);
		return 1;
	}

	printf("expected failure: invalid length rejected, len=%zu errno=%d(%s)\n",
	       bad_len, errno, strerror(errno));
	return 0;
}

static int do_mmap_invalid_offset(int fd, const char *off_arg)
{
	struct day30_info info;
	unsigned long page_off;
	long page_size;
	void *map;

	if (get_info(fd, &info) != 0)
		return 1;

	page_off = strtoul(off_arg, NULL, 0);
	page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0) {
		fprintf(stderr, "sysconf(_SC_PAGESIZE) failed\n");
		return 1;
	}

	map = mmap(NULL, info.map_bytes, PROT_READ | PROT_WRITE,
		   MAP_SHARED, fd, (off_t)page_off * (off_t)page_size);
	if (map != MAP_FAILED) {
		munmap(map, info.map_bytes);
		fprintf(stderr,
			"unexpected success: invalid offset %lu pages should be rejected\n",
			page_off);
		return 1;
	}

	printf("expected failure: invalid offset rejected, page_off=%lu errno=%d(%s)\n",
	       page_off, errno, strerror(errno));
	return 0;
}

/*
 * mmap-verify 是 Day30 最核心的动作：
 * 1. 读驱动信息，拿到 map_bytes/src_off/dst_off/max_verify_len；
 * 2. mmap 整个 coherent DMA buffer；
 * 3. 用户态直接写 src、清 dst；
 * 4. ioctl 触发两段 DMA；
 * 5. 回到用户态直接比较 src/dst。
 *
 * 这里没有再让内核帮忙 compare，这就是 day30 的“零拷贝主角切到用户态”。
 */
static int do_mmap_verify(int fd, const char *len_arg, const char *seed_arg)
{
	struct day30_info info;
	struct day30_run_req req;
	struct day30_run_result res;
	uint8_t *map;
	uint8_t *src;
	uint8_t *dst;
	int rc = 0;
	uint32_t i;
	int mismatch_index = -1;
	uint8_t mismatch_expected = 0;
	uint8_t mismatch_actual = 0;
	size_t map_bytes;

	if (get_info(fd, &info) != 0)
		return 1;

	req.len = (uint32_t)strtoul(len_arg, NULL, 0);
	req.pattern_seed = (uint32_t)strtoul(seed_arg, NULL, 0);
	map_bytes = info.map_bytes;

	if (!req.len || req.len > info.max_verify_len) {
		fprintf(stderr,
			"invalid verify len %u, max_verify_len=%u\n",
			req.len, info.max_verify_len);
		return 1;
	}

	map = mmap(NULL, map_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %s\n", strerror(errno));
		return 1;
	}

	src = map + info.src_off;
	dst = map + info.dst_off;

	fill_pattern(src, req.len, req.pattern_seed);
	memset(dst, 0, req.len);

	if (ioctl(fd, DAY30_IOC_RUN_DMA, &req) != 0) {
		fprintf(stderr, "DAY30_IOC_RUN_DMA failed: %s\n", strerror(errno));
		munmap(map, map_bytes);
		return 1;
	}

	if (ioctl(fd, DAY30_IOC_GET_RESULT, &res) != 0) {
		perror("ioctl(GET_RESULT)");
		munmap(map, map_bytes);
		return 1;
	}

	for (i = 0; i < req.len; ++i) {
		if (src[i] != dst[i]) {
			mismatch_index = (int)i;
			mismatch_expected = src[i];
			mismatch_actual = dst[i];
			rc = 1;
			break;
		}
	}

	printf("verify_len=%u\n", req.len);
	printf("verify_seed=0x%x\n", req.pattern_seed);
	printf("verify_ok=%u\n", rc == 0 && res.run_ok ? 1U : 0U);
	printf("mismatch_index=%d\n", mismatch_index);
	printf("mismatch_expected=0x%02x\n", mismatch_expected);
	printf("mismatch_actual=0x%02x\n", mismatch_actual);
	printf("run_ok=%u\n", res.run_ok);
	printf("run_error=%d\n", res.run_error);
	printf("irq_delta=%u\n", res.irq_delta);
	printf("last_dma_cmd=0x%08x\n", res.last_dma_cmd);
	printf("mmap_ok=%u\n", res.mmap_ok);
	printf("mmap_error=%d\n", res.mmap_error);
	printf("mmap_len=%u\n", res.mmap_len);
	printf("mmap_pgoff=%u\n", res.mmap_pgoff);

	munmap(map, map_bytes);
	return (rc == 0 && res.run_ok) ? 0 : 1;
}

int main(int argc, char **argv)
{
	int fd, rc = 0;

	if (argc < 3) {
		usage(argv[0]);
		return 2;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %s\n", argv[1], strerror(errno));
		return 3;
	}

	if (strcmp(argv[2], "info") == 0)
		rc = do_info(fd);
	else if (strcmp(argv[2], "read-state") == 0)
		rc = do_read_state(fd);
	else if (strcmp(argv[2], "mmap-verify") == 0) {
		if (argc < 5) {
			usage(argv[0]);
			rc = 2;
		} else {
			rc = do_mmap_verify(fd, argv[3], argv[4]);
		}
	} else if (strcmp(argv[2], "mmap-invalid-len") == 0) {
		if (argc < 4) {
			usage(argv[0]);
			rc = 2;
		} else {
			rc = do_mmap_invalid_len(fd, argv[3]);
		}
	} else if (strcmp(argv[2], "mmap-invalid-offset") == 0) {
		if (argc < 4) {
			usage(argv[0]);
			rc = 2;
		} else {
			rc = do_mmap_invalid_offset(fd, argv[3]);
		}
	} else if (strcmp(argv[2], "result") == 0)
		rc = do_result(fd);
	else if (strcmp(argv[2], "reset-stats") == 0)
		rc = do_reset_stats(fd);
	else {
		usage(argv[0]);
		rc = 2;
	}

	close(fd);
	return rc;
}
