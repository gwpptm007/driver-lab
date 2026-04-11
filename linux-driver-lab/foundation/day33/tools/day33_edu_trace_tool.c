// SPDX-License-Identifier: GPL-2.0
/*
 * Day33 guest trace tool
 *
 * Day33 的主题从性能对比切换到 ftrace function_graph 解释。
 * 工具层继续复用 day32 已经跑通的 ioctl / mmap / dma 能力，
 * 这样 guest/init.day33 就可以直接拿它来驱动一条稳定、可解释的 trace workload。
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
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "day33_edu_uapi.h"

enum day33_bench_mode {
	DAY33_MODE_IOCTL = 0,
	DAY33_MODE_MMAP_BASELINE,
	DAY33_MODE_MMAP_OPTIMIZED,
	DAY33_MODE_DMA,
};

struct day33_timing_stats {
	double min_us;
	double avg_us;
	double p50_us;
	double p95_us;
	double p99_us;
	double max_us;
	double wall_total_us;
	double throughput_mbps;
	double cpu_user_pct;
	double cpu_sys_pct;
	double success_rate;
	uint32_t payload_bytes;
	uint32_t iterations;
	uint32_t warmup;
	uint32_t success_ops;
	uint32_t failed_ops;
};

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s <dev> info\n"
		"  %s <dev> read-state\n"
		"  %s <dev> mmap-verify <len> <seed>\n"
		"  %s <dev> bench-ioctl <iterations> <warmup>\n"
		"  %s <dev> bench-mmap-baseline <len> <iterations> <warmup> <seed>\n"
		"  %s <dev> bench-mmap <len> <iterations> <warmup> <seed>\n"
		"  %s <dev> bench-dma <len> <iterations> <warmup> <seed>\n"
		"  %s <dev> compare-mmap <len> <iterations> <warmup> <seed>\n"
		"  %s <dev> result\n"
		"  %s <dev> reset-stats\n",
		prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

static uint64_t mono_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint64_t timeval_to_us(const struct timeval *tv)
{
	return (uint64_t)tv->tv_sec * 1000000ULL + (uint64_t)tv->tv_usec;
}

static int cmp_double(const void *a, const void *b)
{
	const double da = *(const double *)a;
	const double db = *(const double *)b;

	if (da < db)
		return -1;
	if (da > db)
		return 1;
	return 0;
}

/*
 * 输入数组必须已经排序。Day33 延续 day31 的简单索引取样方式，
 * 目的是让 records 易读、易复现，而不是引入更复杂的插值实现。
 */
static double percentile_of_sorted(const double *arr, size_t n, double pct)
{
	size_t idx;

	if (n == 0)
		return 0.0;
	if (pct <= 0.0)
		return arr[0];
	if (pct >= 100.0)
		return arr[n - 1];

	idx = (size_t)(((pct / 100.0) * (double)(n - 1)) + 0.5);
	if (idx >= n)
		idx = n - 1;
	return arr[idx];
}

static int get_info(int fd, struct day33_info *info)
{
	if (ioctl(fd, DAY33_IOC_GET_INFO, info) != 0) {
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

static const char *mode_name(enum day33_bench_mode mode)
{
	switch (mode) {
	case DAY33_MODE_IOCTL:
		return "ioctl";
	case DAY33_MODE_MMAP_BASELINE:
		return "mmap-baseline";
	case DAY33_MODE_MMAP_OPTIMIZED:
		return "mmap-optimized";
	case DAY33_MODE_DMA:
		return "dma";
	default:
		return "unknown";
	}
}

static void print_stats_human(enum day33_bench_mode mode,
			      const struct day33_timing_stats *s)
{
	printf("mode=%s\n", mode_name(mode));
	printf("payload_bytes=%u\n", s->payload_bytes);
	printf("iterations=%u\n", s->iterations);
	printf("warmup=%u\n", s->warmup);
	printf("success_ops=%u\n", s->success_ops);
	printf("failed_ops=%u\n", s->failed_ops);
	printf("success_rate=%.2f\n", s->success_rate);
	printf("wall_total_us=%.2f\n", s->wall_total_us);
	printf("avg_us=%.3f\n", s->avg_us);
	printf("p50_us=%.3f\n", s->p50_us);
	printf("p95_us=%.3f\n", s->p95_us);
	printf("p99_us=%.3f\n", s->p99_us);
	printf("min_us=%.3f\n", s->min_us);
	printf("max_us=%.3f\n", s->max_us);
	printf("throughput_mbps=%.3f\n", s->throughput_mbps);
	printf("cpu_user_pct=%.3f\n", s->cpu_user_pct);
	printf("cpu_sys_pct=%.3f\n", s->cpu_sys_pct);
}

static void print_stats_csv(enum day33_bench_mode mode,
			    const struct day33_timing_stats *s)
{
	printf("csv,mode=%s,payload_bytes=%u,iterations=%u,warmup=%u,success_ops=%u,failed_ops=%u,success_rate=%.2f,avg_us=%.3f,p50_us=%.3f,p95_us=%.3f,p99_us=%.3f,min_us=%.3f,max_us=%.3f,throughput_mbps=%.3f,cpu_user_pct=%.3f,cpu_sys_pct=%.3f\n",
	       mode_name(mode), s->payload_bytes, s->iterations, s->warmup,
	       s->success_ops, s->failed_ops, s->success_rate, s->avg_us,
	       s->p50_us, s->p95_us, s->p99_us, s->min_us, s->max_us,
	       s->throughput_mbps, s->cpu_user_pct, s->cpu_sys_pct);
}

static void finish_stats(double *samples, uint32_t iterations,
			 struct day33_timing_stats *stats,
			 double sum_us,
			 uint64_t wall_begin_ns,
			 const struct rusage *ru_begin,
			 const struct rusage *ru_end,
			 uint32_t ok, uint32_t fail)
{
	uint64_t wall_end_ns = mono_ns();
	uint64_t user_us;
	uint64_t sys_us;

	qsort(samples, iterations, sizeof(*samples), cmp_double);
	stats->min_us = samples[0];
	stats->avg_us = sum_us / (double)iterations;
	stats->p50_us = percentile_of_sorted(samples, iterations, 50.0);
	stats->p95_us = percentile_of_sorted(samples, iterations, 95.0);
	stats->p99_us = percentile_of_sorted(samples, iterations, 99.0);
	stats->max_us = samples[iterations - 1];
	stats->success_ops = ok;
	stats->failed_ops = fail;
	stats->success_rate = iterations ? ((double)ok * 100.0 / (double)iterations) : 0.0;
	stats->wall_total_us = (double)(wall_end_ns - wall_begin_ns) / 1000.0;
	if (stats->wall_total_us > 0.0)
		stats->throughput_mbps = ((double)ok * (double)stats->payload_bytes * 8.0) / stats->wall_total_us;

	user_us = timeval_to_us(&ru_end->ru_utime) - timeval_to_us(&ru_begin->ru_utime);
	sys_us = timeval_to_us(&ru_end->ru_stime) - timeval_to_us(&ru_begin->ru_stime);
	if (stats->wall_total_us > 0.0) {
		stats->cpu_user_pct = ((double)user_us / stats->wall_total_us) * 100.0;
		stats->cpu_sys_pct = ((double)sys_us / stats->wall_total_us) * 100.0;
	}
}

static int prepare_mapping(int fd, uint32_t len, struct day33_info *info,
			   uint8_t **map_out, uint8_t **src_out, uint8_t **dst_out)
{
	uint8_t *map;

	if (get_info(fd, info) != 0)
		return -1;
	if (!len || len > info->max_verify_len) {
		fprintf(stderr, "invalid len %u, max_verify_len=%u\n", len, info->max_verify_len);
		return -1;
	}

	map = mmap(NULL, info->map_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %s\n", strerror(errno));
		return -1;
	}

	*map_out = map;
	*src_out = map + info->src_off;
	*dst_out = map + info->dst_off;
	return 0;
}

static int run_ioctl_mode(int fd, uint32_t iterations, uint32_t warmup,
			  struct day33_timing_stats *stats)
{
	double *samples = NULL;
	struct rusage ru_begin, ru_end;
	double sum_us = 0.0;
	uint64_t wall_begin_ns;
	uint32_t i, ok = 0, fail = 0;
	int rc = 0;

	memset(stats, 0, sizeof(*stats));
	stats->iterations = iterations;
	stats->warmup = warmup;
	stats->payload_bytes = 0;
	if (!iterations) {
		fprintf(stderr, "iterations must be > 0\n");
		return 1;
	}

	samples = calloc(iterations, sizeof(*samples));
	if (!samples) {
		perror("calloc");
		return 1;
	}

	getrusage(RUSAGE_SELF, &ru_begin);
	wall_begin_ns = mono_ns();
	for (i = 0; i < warmup + iterations; ++i) {
		uint64_t t0_ns = mono_ns();
		uint64_t t1_ns;
		bool success = true;
		struct day33_info info;

		if (get_info(fd, &info) != 0)
			success = false;
		t1_ns = mono_ns();

		if (i >= warmup) {
			double us = (double)(t1_ns - t0_ns) / 1000.0;
			samples[i - warmup] = us;
			sum_us += us;
			if (success)
				ok++;
			else
				fail++;
		}
	}
	getrusage(RUSAGE_SELF, &ru_end);
	finish_stats(samples, iterations, stats, sum_us, wall_begin_ns, &ru_begin, &ru_end, ok, fail);
	free(samples);
	return rc;
}

/*
 * Day33 的 baseline 模式故意把 GET_INFO + mmap + munmap 放在每轮循环里。
 * 这是为了给 trace 一个“可观察且可收敛”的热点对象：
 * - ioctl/syscall 开销
 * - VMA 建立与销毁
 * - 地址空间切换与页表路径
 */
static int run_mmap_baseline(int fd, uint32_t payload, uint32_t iterations,
			     uint32_t warmup, uint32_t seed,
			     struct day33_timing_stats *stats)
{
	double *samples = NULL;
	struct rusage ru_begin, ru_end;
	double sum_us = 0.0;
	uint64_t wall_begin_ns;
	uint32_t i, ok = 0, fail = 0;

	memset(stats, 0, sizeof(*stats));
	stats->payload_bytes = payload;
	stats->iterations = iterations;
	stats->warmup = warmup;
	if (!iterations) {
		fprintf(stderr, "iterations must be > 0\n");
		return 1;
	}

	samples = calloc(iterations, sizeof(*samples));
	if (!samples) {
		perror("calloc");
		return 1;
	}

	getrusage(RUSAGE_SELF, &ru_begin);
	wall_begin_ns = mono_ns();
	for (i = 0; i < warmup + iterations; ++i) {
		struct day33_info info;
		uint8_t *map = NULL, *src = NULL, *dst = NULL;
		uint64_t t0_ns = mono_ns();
		uint64_t t1_ns;
		bool success = true;

		if (prepare_mapping(fd, payload, &info, &map, &src, &dst) != 0) {
			success = false;
		} else {
			fill_pattern(src, payload, seed + i);
			memset(dst, 0, payload);
			memcpy(dst, src, payload);
			if (memcmp(src, dst, payload) != 0)
				success = false;
			munmap(map, info.map_bytes);
		}
		t1_ns = mono_ns();

		if (i >= warmup) {
			double us = (double)(t1_ns - t0_ns) / 1000.0;
			samples[i - warmup] = us;
			sum_us += us;
			if (success)
				ok++;
			else
				fail++;
		}
	}
	getrusage(RUSAGE_SELF, &ru_end);
	finish_stats(samples, iterations, stats, sum_us, wall_begin_ns, &ru_begin, &ru_end, ok, fail);
	free(samples);
	return 0;
}

/*
 * optimized 模式把“重复准备”搬到循环外：
 * - 只做一次 GET_INFO
 * - 只做一次 mmap
 * - 循环里只保留核心 memcpy/memcmp
 *
 * 这就是 Day33 要用 trace 与 timing 一起验证的最小优化点。
 */
static int run_mmap_optimized(int fd, uint32_t payload, uint32_t iterations,
			      uint32_t warmup, uint32_t seed,
			      struct day33_timing_stats *stats)
{
	struct day33_info info;
	uint8_t *map = NULL, *src = NULL, *dst = NULL;
	double *samples = NULL;
	struct rusage ru_begin, ru_end;
	double sum_us = 0.0;
	uint64_t wall_begin_ns;
	uint32_t i, ok = 0, fail = 0;

	memset(stats, 0, sizeof(*stats));
	stats->payload_bytes = payload;
	stats->iterations = iterations;
	stats->warmup = warmup;
	if (!iterations) {
		fprintf(stderr, "iterations must be > 0\n");
		return 1;
	}
	if (prepare_mapping(fd, payload, &info, &map, &src, &dst) != 0)
		return 1;

	samples = calloc(iterations, sizeof(*samples));
	if (!samples) {
		perror("calloc");
		munmap(map, info.map_bytes);
		return 1;
	}

	getrusage(RUSAGE_SELF, &ru_begin);
	wall_begin_ns = mono_ns();
	for (i = 0; i < warmup + iterations; ++i) {
		uint64_t t0_ns = mono_ns();
		uint64_t t1_ns;
		bool success = true;

		fill_pattern(src, payload, seed + i);
		memset(dst, 0, payload);
		memcpy(dst, src, payload);
		if (memcmp(src, dst, payload) != 0)
			success = false;
		t1_ns = mono_ns();

		if (i >= warmup) {
			double us = (double)(t1_ns - t0_ns) / 1000.0;
			samples[i - warmup] = us;
			sum_us += us;
			if (success)
				ok++;
			else
				fail++;
		}
	}
	getrusage(RUSAGE_SELF, &ru_end);
	finish_stats(samples, iterations, stats, sum_us, wall_begin_ns, &ru_begin, &ru_end, ok, fail);
	free(samples);
	munmap(map, info.map_bytes);
	return 0;
}

static int run_dma_mode(int fd, uint32_t payload, uint32_t iterations,
			uint32_t warmup, uint32_t seed,
			struct day33_timing_stats *stats)
{
	struct day33_info info;
	struct day33_run_req req;
	uint8_t *map = NULL, *src = NULL, *dst = NULL;
	double *samples = NULL;
	struct rusage ru_begin, ru_end;
	double sum_us = 0.0;
	uint64_t wall_begin_ns;
	uint32_t i, ok = 0, fail = 0;

	memset(stats, 0, sizeof(*stats));
	stats->payload_bytes = payload;
	stats->iterations = iterations;
	stats->warmup = warmup;
	if (!iterations) {
		fprintf(stderr, "iterations must be > 0\n");
		return 1;
	}
	if (prepare_mapping(fd, payload, &info, &map, &src, &dst) != 0)
		return 1;

	samples = calloc(iterations, sizeof(*samples));
	if (!samples) {
		perror("calloc");
		munmap(map, info.map_bytes);
		return 1;
	}

	getrusage(RUSAGE_SELF, &ru_begin);
	wall_begin_ns = mono_ns();
	for (i = 0; i < warmup + iterations; ++i) {
		uint64_t t0_ns = mono_ns();
		uint64_t t1_ns;
		bool success = true;

		fill_pattern(src, payload, seed + i);
		memset(dst, 0, payload);
		req.len = payload;
		req.pattern_seed = seed + i;
		if (ioctl(fd, DAY33_IOC_RUN_DMA, &req) != 0)
			success = false;
		else if (memcmp(src, dst, payload) != 0)
			success = false;
		t1_ns = mono_ns();

		if (i >= warmup) {
			double us = (double)(t1_ns - t0_ns) / 1000.0;
			samples[i - warmup] = us;
			sum_us += us;
			if (success)
				ok++;
			else
				fail++;
		}
	}
	getrusage(RUSAGE_SELF, &ru_end);
	finish_stats(samples, iterations, stats, sum_us, wall_begin_ns, &ru_begin, &ru_end, ok, fail);
	free(samples);
	munmap(map, info.map_bytes);
	return 0;
}

static int do_info(int fd)
{
	struct day33_info info;

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
	printf("total_run_calls=%" PRIu64 " total_run_ok=%" PRIu64 " total_run_fail=%" PRIu64 " last_run_ns=%" PRIu64 "\n",
	       (uint64_t)info.total_run_calls, (uint64_t)info.total_run_ok,
	       (uint64_t)info.total_run_fail, (uint64_t)info.last_run_ns);
	printf("run_len=%u run_seed=0x%x run_ok=%u run_error=%d\n",
	       info.last_run_len, info.last_run_seed, info.last_run_ok, info.last_run_error);
	printf("last_irq_delta=%u last_dma_cmd=0x%08x\n", info.last_irq_delta, info.last_dma_cmd);
	printf("mmap_ok=%u mmap_error=%d mmap_len=%u mmap_pgoff=%u\n",
	       info.last_mmap_ok, info.last_mmap_error, info.last_mmap_len, info.last_mmap_pgoff);
	return 0;
}

static int do_read_state(int fd)
{
	char buf[768];
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
	struct day33_run_result res;
	if (ioctl(fd, DAY33_IOC_GET_RESULT, &res) != 0) {
		perror("ioctl(GET_RESULT)");
		return 1;
	}
	printf("total_run_calls=%" PRIu64 "\n", (uint64_t)res.total_run_calls);
	printf("total_run_ok=%" PRIu64 "\n", (uint64_t)res.total_run_ok);
	printf("total_run_fail=%" PRIu64 "\n", (uint64_t)res.total_run_fail);
	printf("last_run_ns=%" PRIu64 "\n", (uint64_t)res.last_run_ns);
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
	if (ioctl(fd, DAY33_IOC_RESET_STATS) != 0) {
		perror("ioctl(RESET_STATS)");
		return 1;
	}
	puts("reset-stats ok");
	return 0;
}

static int do_mmap_verify(int fd, const char *len_arg, const char *seed_arg)
{
	struct day33_info info;
	struct day33_run_req req;
	struct day33_run_result res;
	uint8_t *map, *src, *dst;
	uint32_t i;
	int mismatch_index = -1;
	uint8_t mismatch_expected = 0;
	uint8_t mismatch_actual = 0;
	int rc = 0;

	if (prepare_mapping(fd, (uint32_t)strtoul(len_arg, NULL, 0), &info, &map, &src, &dst) != 0)
		return 1;

	req.len = (uint32_t)strtoul(len_arg, NULL, 0);
	req.pattern_seed = (uint32_t)strtoul(seed_arg, NULL, 0);
	fill_pattern(src, req.len, req.pattern_seed);
	memset(dst, 0, req.len);

	if (ioctl(fd, DAY33_IOC_RUN_DMA, &req) != 0) {
		fprintf(stderr, "DAY33_IOC_RUN_DMA failed: %s\n", strerror(errno));
		munmap(map, info.map_bytes);
		return 1;
	}
	if (ioctl(fd, DAY33_IOC_GET_RESULT, &res) != 0) {
		perror("ioctl(GET_RESULT)");
		munmap(map, info.map_bytes);
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
	printf("last_run_ns=%" PRIu64 "\n", (uint64_t)res.last_run_ns);
	printf("last_dma_cmd=0x%08x\n", res.last_dma_cmd);
	printf("mmap_ok=%u\n", res.mmap_ok);
	printf("mmap_error=%d\n", res.mmap_error);
	printf("mmap_len=%u\n", res.mmap_len);
	printf("mmap_pgoff=%u\n", res.mmap_pgoff);
	munmap(map, info.map_bytes);
	return (rc == 0 && res.run_ok) ? 0 : 1;
}

static int do_bench_ioctl(int fd, const char *iter_arg, const char *warmup_arg)
{
	struct day33_timing_stats stats;
	uint32_t iterations = (uint32_t)strtoul(iter_arg, NULL, 0);
	uint32_t warmup = (uint32_t)strtoul(warmup_arg, NULL, 0);
	if (run_ioctl_mode(fd, iterations, warmup, &stats) != 0)
		return 1;
	print_stats_human(DAY33_MODE_IOCTL, &stats);
	print_stats_csv(DAY33_MODE_IOCTL, &stats);
	return stats.failed_ops ? 1 : 0;
}

static int do_bench_mode(int fd, enum day33_bench_mode mode,
			 const char *len_arg, const char *iter_arg,
			 const char *warmup_arg, const char *seed_arg)
{
	struct day33_timing_stats stats;
	uint32_t len = (uint32_t)strtoul(len_arg, NULL, 0);
	uint32_t iterations = (uint32_t)strtoul(iter_arg, NULL, 0);
	uint32_t warmup = (uint32_t)strtoul(warmup_arg, NULL, 0);
	uint32_t seed = (uint32_t)strtoul(seed_arg, NULL, 0);
	int ret;

	switch (mode) {
	case DAY33_MODE_MMAP_BASELINE:
		ret = run_mmap_baseline(fd, len, iterations, warmup, seed, &stats);
		break;
	case DAY33_MODE_MMAP_OPTIMIZED:
		ret = run_mmap_optimized(fd, len, iterations, warmup, seed, &stats);
		break;
	case DAY33_MODE_DMA:
		ret = run_dma_mode(fd, len, iterations, warmup, seed, &stats);
		break;
	default:
		return 1;
	}
	if (ret != 0)
		return 1;
	print_stats_human(mode, &stats);
	print_stats_csv(mode, &stats);
	return stats.failed_ops ? 1 : 0;
}

static int do_compare_mmap(int fd, const char *len_arg,
			   const char *iter_arg,
			   const char *warmup_arg,
			   const char *seed_arg)
{
	struct day33_timing_stats base, opt;
	uint32_t len = (uint32_t)strtoul(len_arg, NULL, 0);
	uint32_t iterations = (uint32_t)strtoul(iter_arg, NULL, 0);
	uint32_t warmup = (uint32_t)strtoul(warmup_arg, NULL, 0);
	uint32_t seed = (uint32_t)strtoul(seed_arg, NULL, 0);
	double avg_gain = 0.0, p99_gain = 0.0, tp_gain = 0.0;

	if (run_mmap_baseline(fd, len, iterations, warmup, seed, &base) != 0)
		return 1;
	if (run_mmap_optimized(fd, len, iterations, warmup, seed, &opt) != 0)
		return 1;

	if (base.avg_us > 0.0)
		avg_gain = ((base.avg_us - opt.avg_us) / base.avg_us) * 100.0;
	if (base.p99_us > 0.0)
		p99_gain = ((base.p99_us - opt.p99_us) / base.p99_us) * 100.0;
	if (base.throughput_mbps > 0.0)
		tp_gain = ((opt.throughput_mbps - base.throughput_mbps) / base.throughput_mbps) * 100.0;

	printf("compare_mode=mmap\n");
	printf("payload_bytes=%u\n", len);
	printf("iterations=%u\n", iterations);
	printf("warmup=%u\n", warmup);
	printf("baseline_avg_us=%.3f\n", base.avg_us);
	printf("optimized_avg_us=%.3f\n", opt.avg_us);
	printf("avg_latency_gain_pct=%.2f\n", avg_gain);
	printf("baseline_p99_us=%.3f\n", base.p99_us);
	printf("optimized_p99_us=%.3f\n", opt.p99_us);
	printf("p99_latency_gain_pct=%.2f\n", p99_gain);
	printf("baseline_throughput_mbps=%.3f\n", base.throughput_mbps);
	printf("optimized_throughput_mbps=%.3f\n", opt.throughput_mbps);
	printf("throughput_gain_pct=%.2f\n", tp_gain);
	return 0;
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
		if (argc < 5) { usage(argv[0]); rc = 2; }
		else rc = do_mmap_verify(fd, argv[3], argv[4]);
	} else if (strcmp(argv[2], "bench-ioctl") == 0) {
		if (argc < 5) { usage(argv[0]); rc = 2; }
		else rc = do_bench_ioctl(fd, argv[3], argv[4]);
	} else if (strcmp(argv[2], "bench-mmap-baseline") == 0) {
		if (argc < 7) { usage(argv[0]); rc = 2; }
		else rc = do_bench_mode(fd, DAY33_MODE_MMAP_BASELINE, argv[3], argv[4], argv[5], argv[6]);
	} else if (strcmp(argv[2], "bench-mmap") == 0) {
		if (argc < 7) { usage(argv[0]); rc = 2; }
		else rc = do_bench_mode(fd, DAY33_MODE_MMAP_OPTIMIZED, argv[3], argv[4], argv[5], argv[6]);
	} else if (strcmp(argv[2], "bench-dma") == 0) {
		if (argc < 7) { usage(argv[0]); rc = 2; }
		else rc = do_bench_mode(fd, DAY33_MODE_DMA, argv[3], argv[4], argv[5], argv[6]);
	} else if (strcmp(argv[2], "compare-mmap") == 0) {
		if (argc < 7) { usage(argv[0]); rc = 2; }
		else rc = do_compare_mmap(fd, argv[3], argv[4], argv[5], argv[6]);
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
