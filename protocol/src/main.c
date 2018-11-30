/*
 ============================================================================
 Name : HErOMCFTP.c
 Author :
 Version :
 Copyright : Your copyright notice
 Description : Ansi-style
 ============================================================================
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()

#include <netdb.h>            // struct addrinfo
#include <sys/types.h>        // needed for socket(), uint8_t, uint16_t, uint32_t
#include <sys/socket.h>       // needed for socket()
#include <netinet/in.h>       // IPPROTO_ICMP, INET_ADDRSTRLEN
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/ip_icmp.h>  // struct icmp, ICMP_ECHO
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/ioctl.h>        // macro ioctl is defined
#include <sys/un.h>
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl.
#include <net/if.h>           // struct ifreq
#include <linux/if_ether.h>   // ETH_P_IP = 0x0800, ETH_P_IPV6 = 0x86DD
#include <linux/if_packet.h>  // struct sockaddr_ll (see man 7 packet)
#include <net/ethernet.h>
#include <linux/tcp.h>
#include <sys/time.h>         // gettimeofday()
#include <errno.h>            // errno, perror()

#include <pthread.h>
#include <assert.h>
#include <limits.h>
#include <sys/poll.h>
#include <sched.h>
#include <math.h>

#include <sys/un.h>
#include <bits/signum.h>

#ifndef NETMAP_WITH_LIBS
#define NETMAP_WITH_LIBS
#endif
#include "net/netmap_user.h"

#include <zlog.h>
#include "global.h"
#include "netmap_util.h"

#include "thread_recv.h"
#include "thread_send.h"
#include "thread_data.h"
#include "thread_service.h"
#include "hero.h"

/* Print usage statement and exit */
void printUsage(void) {
	printf("Usage: <tag> [options]\n");
	printf("             -e <id>: Electrical NIC name (default: %s)\n", DEFAULT_CTRL_NIC_NAME);
	printf("             -o <id>: Optical NIC name (default: %s)\n", DEFAULT_DATA_NIC_NAME);
	printf("          -c <index>: index of the CPU netmap thread pinned to\n");
	printf("          -q <index>: index of the queue netmap uses\n");
	printf("            -r <x.y>: Send at x.y Gbps\n");
	printf("            -R <x.y>: Send at x.y Mpps\n");
	printf("           -b <size>: max burst in a system call\n");
	printf("            -a <x.y>: attempt sending at x.y of sending rate\n");
	printf("                  -z: zero seq notification to application\n");
	printf("                  -t: connect to multicast controller\n");
	exit(1);
}

void parseUsage(int argc, char **argv) {
	char c;
	/* Get command line args */
	while ((c = getopt(argc, argv, "s:S:d:c:i:j:e:o:r:R:b:a:q:zptf")) != -1) {
		switch (c) {
		case 's':
			data_channel_scheduling_policy = (int32_t) strtoull(optarg, NULL, 0);
			break;
		case 'S':
			data_channel_scheduling_priority = (int32_t) strtoull(optarg, NULL, 0);
			break;
		case 'd':
			data_channel_affinity = (uint32_t) strtoull(optarg, NULL, 0);
			break;
		case 'c':
			ctrl_channel_affinity = (uint32_t) strtoull(optarg, NULL, 0);
			break;
		case 'i':
			api_channel_affinity = (uint32_t) strtoull(optarg, NULL, 0);
			break;
		case 'j':
			data_payload_len = (uint32_t) strtoull(optarg, NULL, 0);
			break;
		case 'e':
			ctrl_nic_name = optarg;
			break;
		case 'o':
			data_nic_name = optarg;
			break;
		case 'q':
			queue_index = (uint8_t) strtoull(optarg, NULL, 0);
			break;
		case 'r':
			rate_type = RATE_GBPS;
			if ((sscanf(optarg, "%lg", &rate_f) != 1) || !(rate_f > 0.0 && rate_f <= 10.0)) {
				printf("Bandwidth in Gbps must be >0 and <=10.0\n");
				printUsage();
			}
			break;
		case 'R':
			rate_type = RATE_MPPS;
			if ((sscanf(optarg, "%lg", &rate_f) != 1) || !(rate_f > 0.0 && rate_f <= 14.8)) {
				printf("Packet rate in Mpps must be >0 and <=14.8\n");
				printUsage();
			}
			break;
		case 'b':
			nm_burst = (pktnum_t) strtoull(optarg, NULL, 0);
			break;
		case 'z':
			seq_zero_notify = 1;
			break;
		case 'p':
			msg_padding = 1;
			break;
		case 't':
			connect_controller = 1;
			break;
		case 'f':
			first_first_seq_fullrate = 1;
			break;
		case 'a':
			if ((sscanf(optarg, "%lg", &attempt_scale) != 1) || !(attempt_scale > 0.0 && attempt_scale <= 1.0)) {
				printf("Attempt scale must be in (0.0, 1.0]\n");
				printUsage();
			}
			break;
		default:
			printUsage();
		}
	}
}

int main(int argc, char **argv) {
	parseUsage(argc, argv);
	D("********** Transceiver Configuration **********");
	D("*\tdata channel nic:\t%s", data_nic_name);
	D("*\tctrl channel nic:\t%s", ctrl_nic_name);
	D("*\tnic queue index:\t%d", queue_index);
	D("*\tearly notification:\t%s", seq_zero_notify ? "True" : "False");
	D("*\tmsg padding:\t\t%s", msg_padding ? "True" : "False");
	D("*\tnetmap burst:\t\t%d", nm_burst);
	D("*\tattempt scale:\t\t%.3f", attempt_scale);
	D("*\tconnect controller:\t%s", connect_controller ? "True" : "False");
	D("*\tdata rate:\t\t%g%s", rate_f, (rate_type == RATE_GBPS) ? "Gbps" : (rate_type == RATE_MPPS) ? "Mpps" : "");
	D("***********************************************");

	num_cores = system_ncpus();

	int rc;
	rc = zlog_init("zlog_default.conf");
	if (rc) {
		printf("init failed\n");
		return -1;
	}

	zc = zlog_get_category("my_cat");
	if (!zc) {
		printf("get cat fail\n");
		zlog_fini();
		return -2;
	}

	signal(SIGUSR1, user1_handler);

	/* global argument */
	struct glob_arg g;
	createGlobalArg(&(g), data_nic_name);

	/* netmap thread argument */
	NetmapArg n_arg;
	struct targ *t = &(n_arg.t);
	createTArg(t, &(g)); //, pthread_NetmapR, cpu_index); /* for the new server, has to be pinned to core 0*/
	/* open netmap interfaces */
	startNetmapfd(t);
	D("**** NETMAP config HOST stack");
	curr_nmr = t->nmd_host->req;
	do_nmr_dump();
	D("**** NETMAP config NIC");
	curr_nmr = t->nmd->req;
	do_nmr_dump();

	/*initiate pthread config*/
// TODO: should be in a wrapper....
	createNicNameMap();
	setHostChannel(ctrl_nic_name, hostname_ctrl, &hostinaddress_ctrl, &hostethhdr_ctrl);
	setHostChannel(data_nic_name, hostname_data, &hostinaddress_data, &hostethhdr_data);
	deleteRamdiskFiles();

	initSharedDataStructures();

	/* create pipes between threads */
	int st;
	st = pipe(pipe_fd_n2c); // pipe from netmap to ctrl recv thread
	if (st < 0) {
		perror("pipe error");
	}
	FdState * fds_n2c_p = createFdState(pipe_fd_n2c[0], PIPE_N2C_RECV, NULL, NULL);
	List_push(rsockList, fds_n2c_p);

	st = pipe(pipe_fd_i2c_recv); // pipe from API to ctrl recv thread
	if (st < 0) {
		perror("pipe error");
	}
	FdState * fds_i2c_recv_p = createFdState(pipe_fd_i2c_recv[0], PIPE_I2C_RECV, NULL, NULL);
	List_push(rsockList, fds_i2c_recv_p);

	st = pipe(pipe_fd_c2i); // pipe from netmap to ctrl recv thread
	if (st < 0) {
		perror("pipe error");
	}
	FdState * fds_c2i_p = createFdState(pipe_fd_c2i[0], PIPE_C2I_UDS, NULL, NULL);
	List_push(lsockList, fds_c2i_p);

	st = pipe(pipe_fd_c_send2i); // pipe from netmap to ctrl recv thread
	if (st < 0) {
		perror("pipe error");
	}
	FdState * fds_c_send2i_p = createFdState(pipe_fd_c_send2i[0], PIPE_C_SEND2I_UDS, NULL, NULL);
	List_push(lsockList, fds_c_send2i_p);

	void *status;

	/*initiate pthread attributes*/
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/* from sender */
	SendSocketArg ss_arg;
	pthread_create(&pthread_SendSocket, &attr, thread_sendSocket, (void *) (&ss_arg));

	RecvSocketArg rs_arg;
	pthread_create(&pthread_RecvSocket, &attr, thread_recvSocket, (void *) (&rs_arg));

	DomainSocketArg ds_arg;
	pthread_create(&pthread_LocalSocket, &attr, thread_domainSocket, (void *) (&ds_arg));

	/*start threads*/
	pthread_create(&pthread_NetmapR, &attr, thread_netmap, (void *) (&n_arg));
	pthread_attr_destroy(&attr);

	pthread_join(pthread_NetmapR, &status);
	pthread_join(pthread_LocalSocket, &status);
	pthread_join(pthread_RecvSocket, &status);

	return (EXIT_SUCCESS);
}
