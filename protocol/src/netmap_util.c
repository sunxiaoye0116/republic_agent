/*
 * netmap_util.c
 *
 *  Created on: Nov 16, 2015
 *      Author: xs6
 */

#include "netmap_util.h"

void nmr_arg_bdg_attach() {
	uint16_t v = curr_nmr.nr_arg1;
	printf("arg1:      %d [", v);
	if (v == 0) {
		printf("no host rings");
	} else if (v == NETMAP_BDG_HOST) {
		printf("BDG_HOST");
	} else {
		printf("???");
	}
	printf("]\n");
	nmr_arg_unexpected(2);
	nmr_arg_unexpected(3);
}

void nmr_arg_bdg_detach() {
	nmr_arg_unexpected(1);
	nmr_arg_unexpected(2);
	nmr_arg_unexpected(3);
}

void nmr_arg_bdg_list() {
}

void nmr_arg_lookup_reg() {
}

void nmr_arg_vnet_hdr() {
	printf("arg1:      %d [vnet hdr len]", curr_nmr.nr_arg1);
	nmr_arg_unexpected(2);
	nmr_arg_unexpected(3);
}

void nmr_arg_error() {
	nmr_arg_unexpected(1);
	nmr_arg_unexpected(2);
	nmr_arg_unexpected(3);
}

void nmr_arg_extra() {
	printf("arg1:      %d [%sextra rings]\n", curr_nmr.nr_arg1, (curr_nmr.nr_arg1 ? "" : "no "));
	printf("arg2:      %d [%s memory allocator]\n", curr_nmr.nr_arg2, (curr_nmr.nr_arg2 == 0 ? "global" : "private"));
	printf("arg3:      %d [%sextra buffers]\n", curr_nmr.nr_arg3, (curr_nmr.nr_arg3 ? "" : "no "));
}

void do_nmr_dump() {
	u_int ringid = curr_nmr.nr_ringid & NETMAP_RING_MASK;
	nmr_arg_interp_fun arg_interp;

	snprintf(nmr_name, IFNAMSIZ + 1, "%s", curr_nmr.nr_name);
	nmr_name[IFNAMSIZ] = '\0';
	printf("name:      %s\n", nmr_name);
	printf("version:   %d\n", curr_nmr.nr_version);
	printf("offset:    %d\n", curr_nmr.nr_offset);
	printf("memsize:   %d [", curr_nmr.nr_memsize);
	if (curr_nmr.nr_memsize < (1 << 20)) {
		printf("%d KiB", curr_nmr.nr_memsize >> 10);
	} else {
		printf("%d MiB", curr_nmr.nr_memsize >> 20);
	}
	printf("]\n");
	printf("tx_slots:  %d\n", curr_nmr.nr_tx_slots);
	printf("rx_slots:  %d\n", curr_nmr.nr_rx_slots);
	printf("tx_rings:  %d\n", curr_nmr.nr_tx_rings);
	printf("rx_rings:  %d\n", curr_nmr.nr_rx_rings);
	printf("ringid:    %x [", curr_nmr.nr_ringid);
	if (curr_nmr.nr_ringid & NETMAP_SW_RING) {
		printf("host rings");
	} else if (curr_nmr.nr_ringid & NETMAP_HW_RING) {
		printf("hw ring %d", ringid);
	} else {
		printf("hw rings");
	}
	if (curr_nmr.nr_ringid & NETMAP_NO_TX_POLL) {
		printf(", no tx poll");
	}
	printf(", region %d", curr_nmr.nr_arg2);
	if (curr_nmr.nr_ringid & NETMAP_DO_RX_POLL) { /* add by xiaoye */
		printf(", do rx poll");
	}
	printf("]\n");
	printf("cmd:       %d", curr_nmr.nr_cmd);
	if (curr_nmr.nr_cmd) {
		printf(" [");
		switch (curr_nmr.nr_cmd) {
		case NETMAP_BDG_ATTACH:
			printf("BDG_ATTACH");
			arg_interp = nmr_arg_bdg_attach;
			break;
		case NETMAP_BDG_DETACH:
			printf("BDG_DETACH");
			arg_interp = nmr_arg_bdg_detach;
			break;
		case NETMAP_BDG_LIST:
			printf("BDG_LIST");
			arg_interp = nmr_arg_bdg_list;
			break;
		case NETMAP_BDG_REGOPS:
			printf("BDG_LOOKUP_REG");
			arg_interp = nmr_arg_lookup_reg;
			break;
		case NETMAP_BDG_VNET_HDR:
			printf("BDG_VNET_HDR");
			arg_interp = nmr_arg_vnet_hdr;
			break;
		case NETMAP_BDG_NEWIF:
			printf("BDG_NEWIF");
			arg_interp = nmr_arg_error;
			break;
		case NETMAP_BDG_DELIF:
			printf("BDG_DELIF");
			arg_interp = nmr_arg_error;
			break;
		default:
			printf("???");
			arg_interp = nmr_arg_error;
			break;
		}
		printf("]\n");
	} else {
		arg_interp = nmr_arg_extra;
	}
	printf("\n");
	arg_interp();
	printf("flags:     %x [", curr_nmr.nr_flags);
	switch (curr_nmr.nr_flags & NR_REG_MASK) {
	case NR_REG_DEFAULT:
		printf("obey ringid");
		break;
	case NR_REG_ALL_NIC:
		printf("ALL_NIC");
		break;
	case NR_REG_SW:
		printf("SW");
		break;
	case NR_REG_NIC_SW:
		printf("NIC_SW");
		break;
	case NR_REG_ONE_NIC:
		printf("ONE_NIC(%d)", ringid);
		break;
	case NR_REG_PIPE_MASTER:
		printf("PIPE_MASTER(%d)", ringid);
		break;
	case NR_REG_PIPE_SLAVE:
		printf("PIPE_SLAVE(%d)", ringid);
		break;
	default:
		printf("???");
		break;
	}
	if (curr_nmr.nr_flags & NR_MONITOR_TX) {
		printf(", MONITOR_TX");
	}
	if (curr_nmr.nr_flags & NR_MONITOR_RX) {
		printf(", MONITOR_RX");
	}
	printf("]\n");
	printf("spare2[0]: %x\n", curr_nmr.spare2[0]);
}

/* Check the payload of the packet for errors (use it for debug).
 * Look for consecutive ascii representations of the size of the packet.
 */
//void dump_payload(char *p, int len, struct netmap_ring *ring, int cur) {
//	char buf[128];
//	int i, j, i0;
//	printf("ring %p cur %5d [buf %6d flags 0x%04x len %5d]\n", ring, cur, ring->slot[cur].buf_idx, ring->slot[cur].flags, len);
//	for (i = 0; i < len;) {
//		memset(buf, sizeof(buf), ' ');
//		sprintf(buf, "%5d: ", i);
//		i0 = i;
//		for (j = 0; j < 16 && i < len; i++, j++)
//			sprintf(buf + 7 + j * 3, "%02x ", (uint8_t) (p[i]));
//		i = i0;
//		for (j = 0; j < 16 && i < len; i++, j++)
//			sprintf(buf + 7 + j + 48, "%c", isprint(p[i]) ? p[i] : '.');
//		printf("%s\n", buf);
//	}
//}
/*
 * how many packets on this set of queues ?
 */
int pkt_queued(struct nm_desc *d, int tx) {
	u_int i, tot = 0;

	if (tx) {
		for (i = d->first_tx_ring; i <= d->last_tx_ring; i++) {
//			tot += nm_ring_space(NETMAP_TXRING(d->nifp, i));
			tot += nm_tx_pending(NETMAP_TXRING(d->nifp, i));
		}
	} else {
		for (i = d->first_rx_ring; i <= d->last_rx_ring; i++) {
			tot += nm_ring_space(NETMAP_RXRING(d->nifp, i));
		}
	}
	return tot;
}

/*
 * parse the vale configuration in conf and put it in nmr.
 * Return the flag set if necessary.
 * The configuration may consist of 0 to 4 numbers separated
 * by commas: #tx-slots,#rx-slots,#tx-rings,#rx-rings.
 * Missing numbers or zeroes stand for default values.
 * As an additional convenience, if exactly one number
 * is specified, then this is assigned to both #tx-slots and #rx-slots.
 * If there is no 4th number, then the 3rd is assigned to both #tx-rings
 * and #rx-rings.
 */
int parse_nmr_config(const char* conf, struct nmreq *nmr) {
	char *w, *tok;
	int i, v;

	nmr->nr_tx_rings = nmr->nr_rx_rings = 0;
	nmr->nr_tx_slots = nmr->nr_rx_slots = 0;
	if (conf == NULL || !*conf)
		return 0;
	w = strdup(conf);
	for (i = 0, tok = strtok(w, ","); tok; i++, tok = strtok(NULL, ",")) {
		v = atoi(tok);
		switch (i) {
		case 0:
			nmr->nr_tx_slots = nmr->nr_rx_slots = v;
			break;
		case 1:
			nmr->nr_rx_slots = v;
			break;
		case 2:
			nmr->nr_tx_rings = nmr->nr_rx_rings = v;
			break;
		case 3:
			nmr->nr_rx_rings = v;
			break;
		default:
			printf("ignored config: %s", tok);
			break;
		}
	}
//	D("txr %d txd %d rxr %d rxd %d", nmr->nr_tx_rings, nmr->nr_tx_slots, nmr->nr_rx_rings, nmr->nr_rx_slots);
	free(w);
	return (nmr->nr_tx_rings || nmr->nr_tx_slots || nmr->nr_rx_rings || nmr->nr_rx_slots) ? NM_OPEN_RING_CFG : 0;
}

void createGlobalArg(struct glob_arg * g, char * if_data) {
	bzero(g, sizeof(struct glob_arg));

//	g->burst = DEFAULT_BURST;
//	int frags; /* fragments per packet */
//	g->nthreads = 1;
//	g->cpus = 1; /* cpus used for running */
//	g->system_cpus = system_ncpus(); /* cpus on the system */
//	zlog_info(zc, "system_cpus [%d]", g->system_cpus);
//	int tx_rate;
//	struct timespec tx_period;
//	int main_fd;
//	struct nm_desc *nmd;
//	int report_interval; /* milliseconds between prints */

//	void *mmap_addr;
	sprintf(g->ifname, "netmap:%s-%d", if_data, queue_index);
	sprintf(g->ifname_host, "netmap:%s^", if_data);
//	char *nmr_config;
	g->zerocopy = 1;
	g->wait_link = 5;
}

void createTArg(struct targ *t, struct glob_arg *g) { //, pthread_t thread, int aff) {
	bzero(t, sizeof(struct targ));
	t->g = g;
//	int fd;
//	struct nm_desc *nmd;
//	t->thread = thread;
//	t->affinity = aff;
}

int startNetmapfd(struct targ *t) {
	/* open the netmap interfaces */
	zlog_info(zc, "open netmap interface (fd) for nic[%s] host[%s]", (t->g)->ifname, (t->g)->ifname_host);

	/* open host interface */
	t->nmd_host = nm_open((t->g)->ifname_host, NULL, 0, NULL); /* host stack */
	if (t->nmd_host == NULL) {
		zlog_fatal(zc, "cannot open %s", (t->g)->ifname_host);
		return (-1);
	}
	t->fd_host = t->nmd_host->fd;

	/* open nic interface */
	t->nmd = nm_open((t->g)->ifname, NULL, NM_OPEN_NO_MMAP, t->nmd_host); /* NIC */
	if (t->nmd == NULL) {
		zlog_fatal(zc, "cannot open %s", (t->g)->ifname);
		nm_close(t->nmd_host);
		return (-1);
	}
	t->fd = t->nmd->fd;

	/* check zerocopy */
	t->g->zerocopy = t->g->zerocopy && (t->nmd_host->mem == t->nmd->mem);
	zlog_info(zc, "zerocopy %ssupported", t->g->zerocopy ? "" : "NOT ");

	/* mmap address */
	zlog_info(zc, "mapped %dKB at %p", t->nmd_host->req.nr_memsize >> 10, t->nmd_host->mem);
	uint16_t devqueues = t->nmd_host->req.nr_rx_rings;
	zlog_info(zc, "devqueues [%d]", devqueues);

	/* wait for the interface is ready */
	zlog_info(zc, "Wait %d secs for phy reset", t->g->wait_link);
	sleep(t->g->wait_link);
	zlog_info(zc, "Ready...");

	return 0;
}
