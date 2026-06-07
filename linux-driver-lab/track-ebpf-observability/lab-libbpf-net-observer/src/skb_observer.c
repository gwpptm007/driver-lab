// SPDX-License-Identifier: GPL-2.0
/*
 * skb_observer.c — userspace 加载器 (原生 libbpf API 版本)
 *
 * 不使用 bpftool skeleton (libbpf 0.5 兼容性问题)，
 * 直接用 bpf_object__open / bpf_object__load / bpf_program__attach 流程。
 *
 * 运行: sudo build/skb_observer -v -d 10
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>

#include "skb_observer.h"

static volatile sig_atomic_t running = 1;
static int verbose = 0;

static const char *event_name(unsigned int type)
{
	switch (type) {
	case EVENT_RX:       return "RX";
	case EVENT_GRO:      return "GRO";
	case EVENT_TX_QUEUE: return "TX-QUEUE";
	case EVENT_TX_XMIT:  return "TX-XMIT";
	case EVENT_DROP:     return "DROP";
	default:             return "?";
	}
}

static int handle_event(void *ctx, void *data, size_t data_sz)
{
	const struct skb_event *ev = data;
	__u64 ts = ev->timestamp / 1000000ULL;

	printf("[%6u ms] cpu=%-2u  %-8s  len=%-5u  %s\n",
	       (unsigned)ts, ev->cpu,
	       event_name(ev->type), ev->len,
	       ev->ifname[0] ? ev->ifname : "<?>");

	return 0;
}

static void sig_handler(int sig) { running = 0; }

static void print_usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [-d SEC] [-v] [-h]\n"
		"  -d SEC  运行时长 (默认 10s, 0=直到 Ctrl+C)\n"
		"  -v       详细模式\n"
		"  -h       帮助\n", prog);
}

int main(int argc, char **argv)
{
	struct bpf_object *obj = NULL;
	struct bpf_program *prog;
	struct bpf_link *links[16] = {0};
	int nlinks = 0;
	struct ring_buffer *rb = NULL;
	int duration = 10;
	int opt, err = 0;
	const char *bpffile = "build/skb_observer.bpf.o";

	while ((opt = getopt(argc, argv, "d:vh")) != -1) {
		switch (opt) {
		case 'd': duration = atoi(optarg); break;
		case 'v': verbose = 1; break;
		case 'h': print_usage(argv[0]); return 0;
		default:  print_usage(argv[0]); return 1;
		}
	}

	if (getuid() != 0) { fprintf(stderr, "ERROR: need root\n"); return 1; }

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	/* 1. 打开 BPF 对象 */
	obj = bpf_object__open(bpffile);
	if (!obj) { fprintf(stderr, "ERROR: bpf_object__open\n"); goto cleanup; }
	if (verbose) printf("[init] bpf_object__open OK\n");

	/* 2. 加载到内核 */
	err = bpf_object__load(obj);
	if (err) { fprintf(stderr, "ERROR: bpf_object__load: %d\n", err); goto cleanup; }
	if (verbose) printf("[init] bpf_object__load OK\n");

	/* 3. attach 所有程序 */
	bpf_object__for_each_program(prog, obj) {
		struct bpf_link *link = bpf_program__attach(prog);
		if (!link) {
			fprintf(stderr, "ERROR: attach %s failed\n",
				bpf_program__name(prog));
			goto cleanup;
		}
		links[nlinks++] = link;
		if (verbose) printf("[init] attached %s\n", bpf_program__name(prog));
	}
	if (verbose) printf("[init] %d programs attached\n", nlinks);

	/* 4. ringbuf */
	struct bpf_map *events_map = NULL;
	bpf_object__for_each_map(events_map, obj) {
		if (strcmp(bpf_map__name(events_map), "events") == 0)
			break;
	}
	if (!events_map) { fprintf(stderr, "ERROR: map 'events' not found\n"); goto cleanup; }

	rb = ring_buffer__new(bpf_map__fd(events_map), handle_event, NULL, NULL);
	if (!rb) { fprintf(stderr, "ERROR: ring_buffer__new\n"); goto cleanup; }
	if (verbose) printf("[init] ringbuf ready, waiting events...\n\n");

	/* 5. 主循环 */
	time_t start = time(NULL);

	while (running) {
		err = ring_buffer__poll(rb, 100);
		if (err == -EINTR) break;
		if (err < 0) { fprintf(stderr, "ERROR: poll: %d\n", err); break; }
		if (duration > 0 && time(NULL) - start >= duration) break;
	}

	/* 6. 读取统计 */
	struct bpf_map *counts_map = NULL;
	bpf_object__for_each_map(counts_map, obj) {
		if (strcmp(bpf_map__name(counts_map), "event_counts") == 0)
			break;
	}

	printf("\n=== skb_observer summary ===\n");
	printf("duration: %ld seconds\n", (long)(time(NULL) - start));

	if (counts_map) {
		__u64 counts[EVENT_MAX] = {0};
		__u32 key;
		int ncpus = libbpf_num_possible_cpus();
		__u64 *per_cpu = malloc(ncpus * sizeof(__u64));

		if (per_cpu) {
			for (key = 0; key < EVENT_MAX; key++) {
				__u64 sum = 0;
				if (bpf_map_lookup_elem(bpf_map__fd(counts_map),
							&key, per_cpu) == 0) {
					for (int i = 0; i < ncpus; i++)
						sum += per_cpu[i];
				}
				counts[key] = sum;
			}
			free(per_cpu);

			printf("\n%-10s  %10s\n", "Event", "Count");
			printf("%-10s  %10s\n", "----------", "----------");
			for (key = 0; key < EVENT_MAX; key++)
				printf("%-10s  %10llu\n", event_name(key),
				       (unsigned long long)counts[key]);
		}
	}

cleanup:
	if (rb) ring_buffer__free(rb);
	for (int i = 0; i < nlinks; i++)
		if (links[i]) bpf_link__destroy(links[i]);
	if (obj) bpf_object__close(obj);
	return err < 0 ? 1 : 0;
}
