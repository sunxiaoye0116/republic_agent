/*
 * netmap_util.h
 *
 *  Created on: Nov 16, 2015
 *      Author: xs6
 */

#ifndef SRC_NETMAP_UTIL_H_
#define SRC_NETMAP_UTIL_H_
#include <stdio.h> /* printf */
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <zlog.h>

#ifndef NETMAP_WITH_LIBS
#define NETMAP_WITH_LIBS
#endif
#include "net/netmap_user.h"

#include "sys_util.h"
#include "global.h"

#define MAX_IFNAMELEN	64  /* our buffer for ifname */
#define MAX_BODYSIZE	16384
//#define VIRT_HDR_1	10	/* length of a base vnet-hdr */
//#define VIRT_HDR_2	12	/* length of the extenede vnet-hdr */
//#define VIRT_HDR_MAX	VIRT_HDR_2

//struct virt_header {
//	uint8_t fields[VIRT_HDR_MAX];
//};

/* counters to accumulate statistics */
struct my_ctrs {
	uint64_t pkts, bytes, events;
	struct timeval t;
};

//struct pkt {
//	struct virt_header vh;
//	struct ether_header eh;
//	struct ip ip;
//	struct udphdr udp;
//	uint8_t body[MAX_BODYSIZE];	// XXX hardwired
//}__attribute__((__packed__));

/*
 * global arguments for all threads
 */
struct glob_arg {
	int pkt_size;
//	int burst;
	int forever;
	int npackets; /* total packets to send */
	int frags; /* fragments per packet */
	int nthreads;
//	int cpus; /* cpus used for running */
//	int system_cpus; /* cpus on the system */

	int options; /* testing */
#define OPT_PREFETCH	1
#define OPT_ACCESS	2
#define OPT_COPY	4
#define OPT_MEMCPY	8
#define OPT_TS		16	/* add a timestamp */
#define OPT_INDIRECT	32	/* use indirect buffers, tx only */
#define OPT_DUMP	64	/* dump rx/tx traffic */
#define OPT_RUBBISH	256	/* send wathever the buffers contain */
#define OPT_RANDOM_SRC  512
#define OPT_RANDOM_DST  1024
	int dev_type;
//#ifndef NO_PCAP
//	pcap_t *p;
//#endif

	int tx_rate;
	struct timespec tx_period;

	int affinity;
	int main_fd;
	struct nm_desc *nmd;
	int report_interval; /* milliseconds between prints */
	void *(*td_body)(void *);
	void *mmap_addr;
	char ifname[MAX_IFNAMELEN];
	char ifname_host[MAX_IFNAMELEN];
	char *nmr_config;
	int dummy_send;
	int virt_header; /* send also the virt_header */
	int extra_bufs; /* goes in nr_arg3 */
	int extra_pipes; /* goes in nr_arg1 */
	char *packet_file; /* -P option */

	int8_t zerocopy;
	int8_t wait_link;
};

/*
 * Arguments for a new thread. The same structure is used by
 * the source and the sink
 */
struct targ {
	struct glob_arg *g;
	int used;
	int completed;
	int cancel;
	int fd;
	int fd_host;
	struct nm_desc *nmd;
	struct nm_desc *nmd_host;
	/* these ought to be volatile, but they are
	 * only sampled and errors should not accumulate
	 */
	struct my_ctrs ctr;

	struct timespec tic, toc;
	int me;
//	pthread_t thread;
//	int affinity;

//	struct pkt pkt;
	void *frame;
};

typedef void (*nmr_arg_interp_fun)();
#define nmr_arg_unexpected(n) \
  printf("arg%d:      %d%s\n", n, curr_nmr.nr_arg ## n, \
    (curr_nmr.nr_arg ## n ? "???" : ""))

void do_nmr_dump();
void dump_payload(char *p, int len, struct netmap_ring *ring, int cur);

int pkt_queued(struct nm_desc *d, int tx);
void createGlobalArg(struct glob_arg * g, char * if_data);
void createTArg(struct targ *t, struct glob_arg *g); //, pthread_t thread, int aff);
int startNetmapfd(struct targ *t);
#endif /* SRC_NETMAP_UTIL_H_ */
