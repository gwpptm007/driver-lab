// SPDX-License-Identifier: GPL-2.0
/*
 * net_observer.c — userspace 加载器 + 报告生成器
 *
 * Phase 5: 整合 Phase 1-4 的观测能力，形成统一的网络可观测性项目工具。
 *
 * 功能:
 *   1. 加载 BPF 程序 (原生 libbpf API)
 *   2. 轮询 ringbuf，实时打印事件
 *   3. 跟踪 per-interface / per-CPU 统计
 *   4. 结束时生成结构化 markdown 报告 (控制台 + 文件)
 *
 * 用法:
 *   sudo build/net_observer -v -d 15                    # 控制台输出
 *   sudo build/net_observer -v -d 15 -o report.md       # 输出到文件
 *   sudo build/net_observer -v -d 15 -i ens33           # 仅过滤 ens33
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

#include "net_observer.h"

static volatile sig_atomic_t running = 1;
static int verbose = 0;

/* ================================================================
 * drop_reason 名称映射
 * ================================================================ */
static const char *drop_reason_name(__u16 reason)
{
	switch (reason) {
	case 0:                    return "?";
	case DROP_NOT_SPECIFIED:   return "NOT_SPECIFIED";
	case DROP_NO_SOCKET:       return "NO_SOCKET";
	case DROP_PKT_TOO_SMALL:   return "PKT_TOO_SMALL";
	case DROP_TCP_CSUM:        return "TCP_CSUM";
	case DROP_SOCKET_FILTER:   return "SOCKET_FILTER";
	case DROP_UDP_CSUM:        return "UDP_CSUM";
	case DROP_NETFILTER_DROP:  return "NETFILTER_DROP";
	case DROP_OTHERHOST:       return "OTHERHOST";
	case DROP_IP_CSUM:         return "IP_CSUM";
	case DROP_IP_INHDR:        return "IP_INHDR";
	case DROP_IP_RPFILTER:     return "IP_RPFILTER";
	case DROP_XFRM_POLICY:     return "XFRM_POLICY";
	case DROP_IP_NOPROTO:      return "IP_NOPROTO";
	case DROP_SOCKET_RCVBUFF:  return "SOCKET_RCVBUFF";
	case DROP_PROTO_MEM:       return "PROTO_MEM";
	case DROP_TCP_ZEROWINDOW:  return "TCP_ZEROWINDOW";
	case DROP_TCP_OLD_DATA:    return "TCP_OLD_DATA";
	case DROP_TCP_OVERWINDOW:  return "TCP_OVERWINDOW";
	case DROP_TCP_OFOMERGE:    return "TCP_OFOMERGE";
	case DROP_TCP_RFC7323_PAWS:return "TCP_RFC7323_PAWS";
	case DROP_TCP_INVALID_SEQ: return "TCP_INVALID_SEQ";
	case DROP_TCP_RESET:       return "TCP_RESET";
	case DROP_TCP_INVALID_SYN: return "TCP_INVALID_SYN";
	case DROP_IP_OUTNOROUTES:  return "IP_OUTNOROUTES";
	case DROP_NEIGH_DEAD:      return "NEIGH_DEAD";
	case DROP_TC_EGRESS:       return "TC_EGRESS";
	case DROP_QDISC_DROP:      return "QDISC_DROP";
	case DROP_CPU_BACKLOG:     return "CPU_BACKLOG";
	case DROP_XDP:             return "XDP";
	case DROP_TC_INGRESS:      return "TC_INGRESS";
	case DROP_UNHANDLED_PROTO: return "UNHANDLED_PROTO";
	case DROP_SKB_CSUM:        return "SKB_CSUM";
	case DROP_NOMEM:           return "NOMEM";
	case DROP_HDR_TRUNC:       return "HDR_TRUNC";
	default:                   return "UNKNOWN";
	}
}

static const char *event_name(__u32 type)
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

/* ================================================================
 * per-interface 统计 (userspace 内存跟踪)
 * ================================================================ */
#define MAX_IFACE_STATS 32

struct iface_stats {
	char  name[16];
	__u64 rx_pkts;
	__u64 rx_bytes;
	__u64 gro_events;
	__u64 tx_queue_pkts;
	__u64 tx_queue_bytes;
	__u64 tx_xmit_pkts;
	__u64 tx_xmit_bytes;
	__u64 drop_pkts;
	__u64 drop_reasons[64];  /* 按 drop_reason 分类 */
};

static struct iface_stats iface_tab[MAX_IFACE_STATS];
static int nifaces = 0;

static struct iface_stats *get_iface(const char *name)
{
	int i;
	for (i = 0; i < nifaces; i++) {
		if (strcmp(iface_tab[i].name, name) == 0)
			return &iface_tab[i];
	}
	if (nifaces >= MAX_IFACE_STATS) {
		/* 使用最后一个 slot 作为 "other" */
		if (nifaces == MAX_IFACE_STATS) {
			strcpy(iface_tab[MAX_IFACE_STATS-1].name, "<OTHER>");
			nifaces++;
		}
		return &iface_tab[MAX_IFACE_STATS-1];
	}
	strncpy(iface_tab[nifaces].name, name[0] ? name : "<?>", 15);
	iface_tab[nifaces].name[15] = '\0';
	return &iface_tab[nifaces++];
}

/* ================================================================
 * ringbuf 回调: 处理每个事件
 * ================================================================ */
static int handle_event(void *ctx, void *data, size_t data_sz)
{
	const struct net_event *ev = data;
	__u64 ts_ms = ev->timestamp / 1000000ULL;

	/* 更新 per-interface 统计 */
	struct iface_stats *ifs = get_iface(ev->ifname[0] ? ev->ifname : "<?>");

	switch (ev->type) {
	case EVENT_RX:
		ifs->rx_pkts++;
		ifs->rx_bytes += ev->len;
		break;
	case EVENT_GRO:
		ifs->gro_events++;
		break;
	case EVENT_TX_QUEUE:
		ifs->tx_queue_pkts++;
		ifs->tx_queue_bytes += ev->len;
		break;
	case EVENT_TX_XMIT:
		ifs->tx_xmit_pkts++;
		ifs->tx_xmit_bytes += ev->len;
		break;
	case EVENT_DROP:
		ifs->drop_pkts++;
		if (ev->drop_reason < 64)
			ifs->drop_reasons[ev->drop_reason]++;
		break;
	}

	/* 控制台实时输出 */
	if (ev->type == EVENT_DROP) {
		printf("[%8llu ms] cpu=%-2u  %-8s  len=%-5u  %-7s  reason=%-16s (%u)\n",
		       (unsigned long long)ts_ms, ev->cpu,
		       event_name(ev->type), ev->len,
		       ev->ifname[0] ? ev->ifname : "<?>",
		       drop_reason_name(ev->drop_reason), ev->drop_reason);
	} else if (verbose) {
		printf("[%8llu ms] cpu=%-2u  %-8s  len=%-5u  %s\n",
		       (unsigned long long)ts_ms, ev->cpu,
		       event_name(ev->type), ev->len,
		       ev->ifname[0] ? ev->ifname : "<?>");
	}

	return 0;
}

static void sig_handler(int sig) { running = 0; }

static void print_usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [-d SEC] [-v] [-o REPORT.md] [-i IFACE] [-h]\n"
		"  -d SEC       运行时长 (默认 10s, 0=直到 Ctrl+C)\n"
		"  -v            详细模式 (打印每个事件)\n"
		"  -o REPORT.md  输出 markdown 报告文件 (默认: 仅控制台)\n"
		"  -i IFACE      仅显示指定接口 (默认: 全部)\n"
		"  -h            帮助\n", prog);
}

/* ================================================================
 * 报告生成
 * ================================================================ */
static void generate_report(FILE *f, time_t start, time_t end,
			    __u64 *per_cpu_counts, int ncpus,
			    const char *iface_filter)
{
	long duration = (long)(end - start);

	fprintf(f, "# Linux Network Observability Report\n\n");
	fprintf(f, "| 项目 | 值 |\n");
	fprintf(f, "|------|----|\n");
	fprintf(f, "| **日期** | %s", ctime(&start));
	fprintf(f, "| **时长** | %ld seconds |\n", duration);
	fprintf(f, "| **CPU 数量** | %d |\n", ncpus);
	if (iface_filter)
		fprintf(f, "| **过滤接口** | %s |\n", iface_filter);
	fprintf(f, "\n");

	/* === per-interface 统计 === */
	fprintf(f, "## 1. Per-Interface 统计\n\n");
	fprintf(f, "| Interface | RX Pkts | RX Bytes | GRO | TX-Q Pkts | TX-Q Bytes | TX-X Pkts | TX-X Bytes | DROP |\n");
	fprintf(f, "|-----------|---------|----------|-----|-----------|------------|-----------|------------|------|\n");

	__u64 total_rx = 0, total_txq = 0, total_txx = 0, total_drop = 0, total_gro = 0;

	for (int i = 0; i < nifaces; i++) {
		struct iface_stats *ifs = &iface_tab[i];
		if (iface_filter && strcmp(ifs->name, iface_filter) != 0)
			continue;
		if (ifs->rx_pkts == 0 && ifs->tx_queue_pkts == 0 &&
		    ifs->tx_xmit_pkts == 0 && ifs->drop_pkts == 0)
			continue;

		fprintf(f, "| %-9s | %7llu | %8llu | %3llu | %9llu | %10llu | %9llu | %10llu | %4llu |\n",
		       ifs->name,
		       (unsigned long long)ifs->rx_pkts,
		       (unsigned long long)ifs->rx_bytes,
		       (unsigned long long)ifs->gro_events,
		       (unsigned long long)ifs->tx_queue_pkts,
		       (unsigned long long)ifs->tx_queue_bytes,
		       (unsigned long long)ifs->tx_xmit_pkts,
		       (unsigned long long)ifs->tx_xmit_bytes,
		       (unsigned long long)ifs->drop_pkts);

		total_rx   += ifs->rx_pkts;
		total_gro  += ifs->gro_events;
		total_txq  += ifs->tx_queue_pkts;
		total_txx  += ifs->tx_xmit_pkts;
		total_drop += ifs->drop_pkts;
	}

	/* 合计行 */
	fprintf(f, "| **TOTAL** | **%5llu** |  | **%3llu** | **%7llu** |  | **%7llu** |  | **%4llu** |\n",
	       (unsigned long long)total_rx,
	       (unsigned long long)total_gro,
	       (unsigned long long)total_txq,
	       (unsigned long long)total_txx,
	       (unsigned long long)total_drop);
	fprintf(f, "\n");

	/* === DROP 原因分类 === */
	if (total_drop > 0) {
		fprintf(f, "### 1.1 DROP 原因分类\n\n");
		fprintf(f, "| Interface | Drop Reason | Count |\n");
		fprintf(f, "|-----------|-------------|-------|\n");

		for (int i = 0; i < nifaces; i++) {
			struct iface_stats *ifs = &iface_tab[i];
			if (ifs->drop_pkts == 0)
				continue;
			for (int r = 0; r < 64; r++) {
				if (ifs->drop_reasons[r] > 0) {
					fprintf(f, "| %-9s | %-11s | %5llu |\n",
					       ifs->name,
					       drop_reason_name((__u16)r),
					       (unsigned long long)ifs->drop_reasons[r]);
				}
			}
		}
		fprintf(f, "\n");
	}

	/* === per-CPU 分布 (来自 BPF per-CPU map) === */
	fprintf(f, "## 2. Per-CPU 分布\n\n");

	/* 从 per_cpu_counts 中提取: 布局为 event_type × ncpus */
	fprintf(f, "| CPU | RX | GRO | TX-QUEUE | TX-XMIT | DROP | Total |\n");
	fprintf(f, "|-----|----|-----|----------|---------|------|-------|\n");

	for (int cpu = 0; cpu < ncpus; cpu++) {
		__u64 rx  = per_cpu_counts[EVENT_RX       * ncpus + cpu];
		__u64 gro = per_cpu_counts[EVENT_GRO      * ncpus + cpu];
		__u64 txq = per_cpu_counts[EVENT_TX_QUEUE * ncpus + cpu];
		__u64 txx = per_cpu_counts[EVENT_TX_XMIT  * ncpus + cpu];
		__u64 drp = per_cpu_counts[EVENT_DROP     * ncpus + cpu];
		__u64 total = rx + gro + txq + txx + drp;

		if (total == 0)
			continue;

		fprintf(f, "| %3d | %2llu | %3llu | %8llu | %7llu | %4llu | %5llu |\n",
		       cpu, (unsigned long long)rx, (unsigned long long)gro,
		       (unsigned long long)txq, (unsigned long long)txx,
		       (unsigned long long)drp, (unsigned long long)total);
	}
	fprintf(f, "\n");

	/* === 路径分析 === */
	fprintf(f, "## 3. 路径分析\n\n");

	fprintf(f, "| 不变量 | 值 | 比例 | 判定 |\n");
	fprintf(f, "|--------|----|------|------|\n");

	/* RX → GRO */
	{
		int ratio = total_rx > 0 ? (int)(total_gro * 100 / total_rx) : 0;
		fprintf(f, "| RX → GRO | %llu → %llu | %d%% | %s |\n",
		       (unsigned long long)total_rx, (unsigned long long)total_gro,
		       ratio, ratio >= 90 ? "OK" : "CHECK");
	}

	/* TX-QUEUE → TX-XMIT */
	{
		int ok = (total_txq == total_txx);
		fprintf(f, "| TX-QUEUE → TX-XMIT | %llu → %llu | %d%% | %s |\n",
		       (unsigned long long)total_txq, (unsigned long long)total_txx,
		       total_txq > 0 ? 100 : 0, ok ? "OK" : "CHECK");
	}

	/* DROP rate */
	{
		__u64 total_pkts = total_rx + total_txq;
		int ratio = total_pkts > 0 ? (int)(total_drop * 10000 / total_pkts) : 0;
		fprintf(f, "| DROP rate | %llu / %llu | %d.%02d%% | %s |\n",
		       (unsigned long long)total_drop, (unsigned long long)total_pkts,
		       ratio / 100, ratio % 100,
		       total_drop == 0 ? "OK" : "INFO");
	}
	fprintf(f, "\n");

	/* === 总结 === */
	fprintf(f, "## 4. 总结\n\n");
	fprintf(f, "| Event | Count |\n");
	fprintf(f, "|-------|-------|\n");
	fprintf(f, "| RX | %llu |\n", (unsigned long long)total_rx);
	fprintf(f, "| GRO | %llu |\n", (unsigned long long)total_gro);
	fprintf(f, "| TX-QUEUE | %llu |\n", (unsigned long long)total_txq);
	fprintf(f, "| TX-XMIT | %llu |\n", (unsigned long long)total_txx);
	fprintf(f, "| DROP | %llu |\n", (unsigned long long)total_drop);

	if (total_drop == 0 && total_rx > 0 && total_txq > 0) {
		fprintf(f, "\n**状态: 网络路径正常，无丢包。**\n");
	} else if (total_drop > 0) {
		fprintf(f, "\n**状态: 检测到 %llu 个丢包事件，详见 1.1 DROP 原因分类。**\n",
		       (unsigned long long)total_drop);
	}
}

/* ================================================================
 * main
 * ================================================================ */
int main(int argc, char **argv)
{
	struct bpf_object *obj = NULL;
	struct bpf_program *prog;
	struct bpf_link *links[16] = {0};
	int nlinks = 0;
	struct ring_buffer *rb = NULL;
	int duration = 10;
	int opt, err = 0;
	const char *bpffile = "build/net_observer.bpf.o";
	const char *outfile = NULL;
	const char *iface_filter = NULL;

	while ((opt = getopt(argc, argv, "d:o:i:vh")) != -1) {
		switch (opt) {
		case 'd': duration = atoi(optarg); break;
		case 'o': outfile = optarg; break;
		case 'i': iface_filter = optarg; break;
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

	time_t end = time(NULL);

	/* 6. 读取 BPF per-CPU 统计 */
	struct bpf_map *counts_map = NULL;
	bpf_object__for_each_map(counts_map, obj) {
		if (strcmp(bpf_map__name(counts_map), "event_counts") == 0)
			break;
	}

	int ncpus = libbpf_num_possible_cpus();
	__u64 *per_cpu_counts = NULL;

	if (counts_map && ncpus > 0) {
		per_cpu_counts = calloc(EVENT_MAX * ncpus, sizeof(__u64));
		if (per_cpu_counts) {
			for (__u32 key = 0; key < EVENT_MAX; key++) {
				__u64 *per_cpu = malloc(ncpus * sizeof(__u64));
				if (per_cpu) {
					if (bpf_map_lookup_elem(bpf_map__fd(counts_map),
								&key, per_cpu) == 0) {
						for (int i = 0; i < ncpus; i++)
							per_cpu_counts[key * ncpus + i] = per_cpu[i];
					}
					free(per_cpu);
				}
			}
		}
	}

	/* 7. 生成报告 */
	printf("\n");
	generate_report(stdout, start, end, per_cpu_counts, ncpus, iface_filter);

	if (outfile) {
		FILE *f = fopen(outfile, "w");
		if (f) {
			generate_report(f, start, end, per_cpu_counts, ncpus, iface_filter);
			fclose(f);
			printf("\n报告已保存到: %s\n", outfile);
		} else {
			fprintf(stderr, "ERROR: 无法写入 %s: %s\n", outfile, strerror(errno));
		}
	}

	free(per_cpu_counts);

cleanup:
	if (rb) ring_buffer__free(rb);
	for (int i = 0; i < nlinks; i++)
		if (links[i]) bpf_link__destroy(links[i]);
	if (obj) bpf_object__close(obj);
	return err < 0 ? 1 : 0;
}
