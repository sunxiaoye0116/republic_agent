/*
 *
 */

#include "hero.h"

MulticastState *activeMulticast_p = NULL; /* pointer to current active MulticastState */

uint8_t need_release_path = 0;
struct netmap_ring * am_txring_p = NULL;
uint32_t am_txring_head = 0;
uint32_t am_txring_tail_last = 0;

uint8_t nicNum = 0;
struct _nicName2ethHdr nicName2ethHdr[10];

RateType rate_type = RATE_GBPS;
double rate_f = 10.0;

int lst_move = 0;

FdState *createLocalSocketState(int fd, struct sockaddr_un addr, int id) {

	LocalSocketState *lsock_p = (LocalSocketState *) malloc(sizeof(LocalSocketState));

	lsock_p->id = id; /* id of a local socket state */
	lsock_p->client_addr = addr;

	// TODO: a local socket might have multiple recver, now we just assume one
	lsock_p->reading = 0;

	FdState * fds_p = createFdState(fd, IFACE_UDS, lsock_p, pool_lsock_p);

	return fds_p;
}

AppState *createAppState(AppID *app_id_p, char* app_id_str_p, char* master_addr_p) {
	AppState * app_p = (AppState *) malloc(sizeof(AppState));
	if (!app_p) {
		zlog_fatal(zc, "malloc() fail");
		abort();
	}

	app_p->app_id = *app_id_p;
	strcpy(app_p->app_id_str, app_id_str_p);
	AppIdtoDir(app_id_p, app_p->bcd_dir_str);
	createDir(app_p->bcd_dir_str);
	strcpy(app_p->master_addr, master_addr_p);

	sprintf(app_p->stats_filename, LOG_DIRECTORY, app_id_str_p);
	app_p->fd_stats_p = fopen(app_p->stats_filename, "a+");
	if (!(app_p->fd_stats_p)) {
		zlog_fatal("open stats file [%s] fails", app_p->stats_filename);
		abort();
	}
	setbuf(app_p->fd_stats_p, NULL);

	app_p->uds_hash = apr_hash_make(pool_lsock_p);
	app_p->bcd_sender_hash = apr_hash_make(pool_lsock_p);

	return app_p;
}

RecverState *createRecverState(BcdID *bcd_id_p) {
	char directory[512];
	AppIdtoDir(&bcd_id_p->app_id, directory);
	struct stat sb;
	if (!(stat(directory, &sb) == 0 && S_ISDIR(sb.st_mode))) {
		return NULL;
	}

	RecverState * recv_p = (RecverState *) malloc(sizeof(RecverState));

	bzero(recv_p, sizeof(RecverState));

	recv_p->lock_processingFastData = 0;	// this will be set as 1 when data from the fast path is being processed

	recv_p->bcd_id = *bcd_id_p; /* key */

	/* seq state */
	recv_p->is_seq_fst = 0;	// if the first seq has been received
	recv_p->seq_tail = 0;		// this is the seq received from fast path right before the seq wrap around

	/* updated by the zero packet */
	recv_p->is_seq_zero = 0;
	recv_p->data_size = 0;		// number of bytes in the data
	recv_p->seq_max = 0;	// number of data packet in the data
	recv_p->byte_payload = data_payload_len;	// number of bytes in all the packet except for the last one
	recv_p->byte_payload_min = 0;	// number of bytes in the last one
	recv_p->tail_patch_buffer = NULL;
	recv_p->tail_patch_buffer_len = 0;

	/* file descriptor for received file */
	BcdIdtoFilename(bcd_id_p, recv_p->bcd_filename);

	if ((recv_p->fd_p = fopen(recv_p->bcd_filename, "w")) == NULL) {				// file descriptor for writing from the fast path
		zlog_fatal(zc, "fopen fail on [%s] errno [%d]", recv_p->bcd_filename, errno);
		free(recv_p);
		return NULL;
	}
	setbuf(recv_p->fd_p, NULL);

	if ((recv_p->fd_patch_p = fopen(recv_p->bcd_filename, "w")) == NULL) { // file descriptor for writing from the slow path
		zlog_fatal(zc, "fopen fail on patching [%s] errno [%d]", recv_p->bcd_filename, errno);
		free(recv_p);
		return NULL;
	}
	setbuf(recv_p->fd_patch_p, NULL);

	/* packet counter */
	recv_p->num_fast_recv_ed = 0;
	recv_p->num_slow_recv_ed = 0;

	/* recver state */
	recv_p->is_fastpath = 0; /* can the fast path receive data */
	recv_p->is_finished = 0;		// the status of the receiving: 0->the file has not completely received..
	recv_p->fileMetadata.jump = -1;
	recv_p->fileMetadata.start = -1;

	recv_p->fds_p = NULL;

	recv_p->delete_counter = 0;

	return recv_p;

}

databyte_t recverState_updateBcdMetadata(RecverState *recv_p, BcdMetadata *bm_p) {
	if (recv_p->is_seq_zero) {
		zlog_error(zc, "zero data did [%ld] has received", recv_p->bcd_id.data_id);
		return -1;
	}

	recv_p->data_size = bm_p->data_size; /* update data size */
	assert(strlen(bm_p->app_id) < APPID_LEN);
	assert(recv_p->data_size != 0L);

	strncpy(recv_p->app_id_str, bm_p->app_id, APPID_LEN);

	recv_p->seq_max = (seqnum_t) ceil(1.0 * recv_p->data_size / (1.0 * recv_p->byte_payload));
	recv_p->byte_payload_min = recv_p->data_size % recv_p->byte_payload; /* number of bytes in the last message */
	if (recv_p->byte_payload_min == 0) { //TODO: not tested...
		zlog_info(zc, "size_min = size_max [%ld]/[%d]", recv_p->data_size, recv_p->byte_payload);
		recv_p->byte_payload_min = recv_p->byte_payload;
	}
	zlog_info(zc, "app_id[%s], data size[%ld] of did[%ld] = size_max[%d] x (seq_max[%d]-1) + size_min[%d]", recv_p->app_id_str, recv_p->data_size, recv_p->bcd_id.data_id, recv_p->byte_payload,
		(recv_p->byte_payload_min == recv_p->byte_payload) ? (recv_p->seq_max - 1) : recv_p->seq_max, recv_p->byte_payload_min);

	if (recv_p->seq_tail) { /* there MIGHT be packet loss at the end of the data, did not get zero packet from fast path */
		if (recv_p->tail_patch_buffer) {
			zlog_error(zc, "tail patch buffer has been allocated");
			return -1;
		}

		int len = (recv_p->seq_max - recv_p->seq_tail - 1 - 1) * (recv_p->byte_payload) + (recv_p->byte_payload_min);
		if (len <= 0) { /* no need to allocate buffer */
			zlog_info(zc, "no data loss at the end");
//			recv_p->seq_tail = 0; // TODO should be removed???
			recv_p->tail_patch_buffer_len = 0;
		} else { /* allocate buffer for the tail */
			zlog_info(zc, "malloc [%ld] bytes for tail patch buffer", recv_p->tail_patch_buffer_len);
			recv_p->tail_patch_buffer = (char *) malloc(sizeof(char) * len); /* allocate the buffer */
			if (!(recv_p->tail_patch_buffer)) {
				zlog_fatal(zc, "malloc fail for tail patch buffer");
				return -1;
			}
			recv_p->tail_patch_buffer_len = len;
		}
	}

	/* update Metadata */
	if (recv_p->fileMetadata.start < 0) {
		recv_p->fileMetadata.start = (recv_p->data_size - (recv_p->byte_payload * recv_p->seq_fst) - recv_p->tail_patch_buffer_len) % recv_p->data_size;
	} else {
		if (recv_p->fileMetadata.start != (recv_p->data_size - (recv_p->byte_payload * recv_p->seq_fst) - recv_p->tail_patch_buffer_len) % recv_p->data_size) {
			zlog_error(zc, "[%ld]vs[%ld] start compare fail", recv_p->fileMetadata.start, (recv_p->data_size - (recv_p->byte_payload * recv_p->seq_fst) - recv_p->tail_patch_buffer_len) % recv_p->data_size);
		}
	}

	recv_p->fileMetadata.jump = recv_p->data_size - recv_p->tail_patch_buffer_len;
	zlog_info(zc, "file_start at [%ld]", recv_p->fileMetadata.start);

	recv_p->is_seq_zero = 1;

	return recv_p->tail_patch_buffer_len;
}

FdState * createFdState(int fd, FdType fd_type, void* state_p, apr_pool_t * pool_p) {
	FdState *fds_p = (FdState *) malloc(sizeof(FdState));

	fds_p->fd = fd;
	fds_p->fd_type = fd_type;
	fds_p->is_trashed = 0;
	fds_p->state_p = state_p;

	fds_p->recv_ed_byte = 0;
	fds_p->recv_expect_byte = BYTE_MAX;
	fds_p->mh.type = MSG_NONTYPE;

	if (fd_type == CTRL_RECV_TCP || fd_type == IFACE_UDS) {
		apr_pool_create(&(fds_p->pool_p), pool_p);
		apr_queue_create(&(fds_p->sendQueue_p), APR_QUEUE_CAPACITY, fds_p->pool_p);
		fds_p->remainMsgToken_p = NULL;
	}
	return fds_p;
}

FdState *createRecvSocketState(char *ip_address, char *nic_name) {
	int fd, status;
	RecvSocketState *rsock_p = (RecvSocketState *) malloc(sizeof(RecvSocketState));

	zlog_info(zc, "create RecvSocketState for ip[%s] @ [%p]", ip_address, (void * )rsock_p);

	strcpy(rsock_p->ip_address, ip_address);
	strcpy(rsock_p->nic_name, nic_name);

	/* socket and address */
	struct hostent *he_p = gethostbyname(rsock_p->ip_address); /* convert server domain name to IP address */
	if (he_p == NULL) {
		perror("No such a host name");
		exit(EXIT_FAILURE);
	}
	unsigned int server_addr = *(unsigned int *) he_p->h_addr_list[0];
	zlog_debug(zc, "master address [%s]", inet_ntoa(*(struct in_addr * )(he_p->h_addr_list[0])));

	/* create a fd */
	fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd < 0) {
		perror("opening TCP socket\n");
		exit(EXIT_FAILURE);
	}

	/* configure fd */
	status = setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, rsock_p->nic_name, strlen(rsock_p->nic_name));
	if (status < 0) {
		perror("setsockopt() SO_RCVTIMEO failed ");
		exit(EXIT_FAILURE);
	}

	rsock_p->server_addr.sin_family = AF_INET; /* fill in the server's address */
	rsock_p->server_addr.sin_addr.s_addr = server_addr;
	rsock_p->server_addr.sin_port = htons(CTRL_DST_PORT);

	/* connect to the server */
	status = connect(fd, (struct sockaddr *) &(rsock_p->server_addr), sizeof(struct sockaddr_in));
	if (status < 0) {
		perror("connect to server failed\n");
		exit(EXIT_FAILURE);
	}
	zlog_debug(zc, "connected");

	/* set the tcp socket as NON-blocking */
	fcntl(fd, F_SETFL, O_NONBLOCK);

	FdState * fds_p = createFdState(fd, CTRL_RECV_TCP, rsock_p, pool_rsock_p);
	zlog_debug(zc, "FdState created");
	return fds_p;
}

static MsgToken *_createMsgToken(const bytes_t tcp_payload_len, uint8_t padding) {
	MsgToken *mt_p = (MsgToken *) malloc(sizeof(MsgToken)); /* initiate the msg tuple */
	mt_p->buf_p = (char *) malloc(tcp_payload_len); /* msg buffer for the entire message including the msg header and the payload */
	mt_p->msg_len = tcp_payload_len;
	mt_p->padding_len = padding ? (INTERFACE_MTU - IP4_HDRLEN - TCP_HDRLEN - mt_p->msg_len) : 0;
	mt_p->byte_sent = 0;
	gettimeofday(&(mt_p->tv), NULL);
	return mt_p;
}

static MsgToken * createMsgToken(BcdID *bcd_id_p, msg_type_t type, char *msg_payload, bytes_t msg_payload_len, uint8_t padding) {
	bytes_t tcp_payload_len = MSG_HDRLEN + msg_payload_len; // msgLen is the bytes in TCP payload
	MsgToken * mt_p = _createMsgToken(tcp_payload_len, padding);
	createMsg(mt_p->buf_p, bcd_id_p, type, msg_payload, tcp_payload_len, mt_p->padding_len);
	return mt_p;
}

/**
 * @payload_len is the length of the payload, not including the message header
 * @return is the length of the message, including the payload and the message header
 */
uint8_t insertMsg2SendQ(apr_queue_t *sendQueue_p, BcdID *bcd_id_p, msg_type_t type, char *msg_payload, bytes_t msg_payload_len, uint8_t padding, uint8_t priority) {
	// struct timeval tv_insertmsg_1, tv_insertmsg_2, tv_insertmsg_3;
	// gettimeofday(&tv_insertmsg_1, NULL);
	if ((!priority) && (apr_queue_size(sendQueue_p) > APR_QUEUE_CAPACITY / 2)) {
		return 0;
	}

	MsgToken *mt_p = createMsgToken(bcd_id_p, type, msg_payload, msg_payload_len, padding);
	apr_status_t s;
	do { // TODO: there should be two thread poll and push the queue... now there is only one thread. so the it will stuck here...
		s = apr_queue_trypush(sendQueue_p, (void *) mt_p);
	} while (s == APR_EINTR);
	if (s == APR_EAGAIN) {
		deleteMsgToken(mt_p);
		return 0;
	}

	// gettimeofday(&tv_insertmsg_2, NULL);
	// timersub(&tv_insertmsg_2, &tv_insertmsg_1, &tv_insertmsg_3);
	// float tv_insertmsg_diff_ms = tv_insertmsg_3.tv_sec * 1000.0 + tv_insertmsg_3.tv_usec / 1000.0;
	// if (tv_insertmsg_diff_ms > 0.1 || tv_insertmsg_diff_ms < -1) {
	// 	D("insert msg time [%.3f]ms", tv_insertmsg_diff_ms);
	// }
	return 0xff;
}

cmd_elem_t * createCmd(apr_queue_t *sendQueue_p, BcdID *bcd_id_p, msg_type_t type, char *msg_payload, bytes_t msg_payload_len, uint8_t padding) {
	cmd_elem_t *cmd_p = (cmd_elem_t *) malloc(sizeof(cmd_elem_t));
	if (!cmd_p) {
		return NULL;
	}
	APR_RING_ELEM_INIT(cmd_p, link);
	cmd_p->sendQueue_p = sendQueue_p;
	cmd_p->mt_p = createMsgToken(bcd_id_p, type, msg_payload, msg_payload_len, padding);
	cmd_p->bcd_id = *bcd_id_p;
	if (!(cmd_p->mt_p)) {
		free(cmd_p);
		return NULL;
	}
	return cmd_p;
}

int8_t recverState_hasGottenAllData(RecverState *recv_p) {
	return (recv_p->num_fast_recv_ed + recv_p->num_slow_recv_ed == recv_p->seq_max);
}

void recverState_finishRecv(RecverState *recv_p) {
	/* get the end of the file */
	fseek(recv_p->fd_p, 0, SEEK_END);
	long int desTell = ftell(recv_p->fd_p);
	fseek(recv_p->fd_patch_p, 0, SEEK_END);
	long int desPatchTell = ftell(recv_p->fd_patch_p);

	/* get the fd pointing to the end */
	FILE *fd_tail = desTell >= desPatchTell ? recv_p->fd_p : recv_p->fd_patch_p;

	/* get the position of the end of the file */
	recv_p->fileMetadata.jump = desTell >= desPatchTell ? desTell : desPatchTell;
	zlog_info(zc, "update jump [%ld] with tail patch buffer len [%d] (des[%ld], desPatch[%ld])", recv_p->fileMetadata.jump, recv_p->tail_patch_buffer_len, desTell, desPatchTell);

	/* write the tail patch buffer to the file */
	fwrite(recv_p->tail_patch_buffer, sizeof(char), recv_p->tail_patch_buffer_len, fd_tail);

	/* finish the data transfer */
	recv_p->is_finished = 1; /* the data transfer has been finished */
	/* get the finish time */
	gettimeofday(&(recv_p->tv_finish), NULL);

	closeFile(&(recv_p->fd_p)); /* close the files */
	closeFile(&(recv_p->fd_patch_p)); /* close the files */
	free(recv_p->tail_patch_buffer); /* free the buffer */

	return;
}

void recverState_stats(RecverState * recv_p, FILE* fd_stats_p) {
	zlog_debug(zc, "recverState_stats is called for did[%ld]", recv_p->bcd_id.data_id);
	if (!(recv_p->is_stats_logged)) {
		recv_p->is_stats_logged = 1;

		char finish_buf[TIME_BUF_LEN];
		gettimeofmillisecond(finish_buf, TIME_BUF_LEN, &(recv_p->tv_finish));

		/* update statistics */
		struct timeval tv_fastpath, tv_after_fastpath, tv_total; //, tv_before_fast;
		/* optical data receiving time */
		timersub(&(recv_p->tv_finish_fastpath), &(recv_p->tv_start_fastpath), &tv_fastpath);
		/* from the end of the last optical to the total finishing time */
		timersub(&(recv_p->tv_finish), &(recv_p->tv_finish_fastpath), &tv_after_fastpath);
		/* total time used for receiving the entire tdata */
		timersub(&(recv_p->tv_finish), &(recv_p->tv_start_fastpath), &tv_total);
		float fastpath_ms = tv_fastpath.tv_sec * 1000.0 + tv_fastpath.tv_usec / 1000.0;
		float total_ms = tv_total.tv_sec * 1000.0 + tv_total.tv_usec / 1000.0;

		/* log to stats file */
		fprintf(fd_stats_p, "%s [RECV][%s][%s][%016lx][%016lx][%ld]\t|", finish_buf, recv_p->app_id_str, inet_ntoa(hostinaddress_ctrl), recv_p->bcd_id.app_id.msb, recv_p->bcd_id.app_id.lsb, recv_p->bcd_id.data_id);
		fprintf(fd_stats_p, "data:[%ld]=[%d]x[%d]+[%d]\t|loss:[%.6f]%%=[%d]/[%d]\t|", recv_p->data_size, recv_p->byte_payload, recv_p->seq_max - 1, recv_p->byte_payload_min, (recv_p->num_slow_recv_ed) * 100.0 / (recv_p->seq_max),
			recv_p->num_slow_recv_ed, recv_p->seq_max);
		fprintf(fd_stats_p, "in/after_fast:[%.6f]ms/[%.6f]ms\t|total:[%.6f]ms\t|effic:[%.6f]~[%.6f]%%|firstseq[%d]|[%s]\n", fastpath_ms, tv_after_fastpath.tv_sec * 1000.0 + tv_after_fastpath.tv_usec / 1000.0, total_ms,
			(recv_p->data_size / (10.0 * 1000.0 * 1000.0) * 8) * 100.0 / fastpath_ms, (recv_p->data_size / (10.0 * 1000.0 * 1000.0) * 8) * 100.0 / total_ms, recv_p->seq_fst, hostname_ctrl);
	}
	return;
}

uint8_t setHostChannel(const char * iface_name, char * hostname_p, struct in_addr * hostinaddr_p, struct ethhdr *hostethhdr_p) {

	int i;
	for (i = 0; i < nicNum; i++) {
		if (strcmp(iface_name, nicName2ethHdr[i].nicName) == 0) {
			*hostinaddr_p = nicName2ethHdr[i].ipAddress;
			*hostethhdr_p = nicName2ethHdr[i].ethHdr;
			struct hostent *he_p;
			if ((he_p = gethostbyaddr(hostinaddr_p, sizeof(struct in_addr), AF_INET)) == NULL) {
				zlog_error(zc, "cannot get hostent of hostaddr [%s]", inet_ntoa(*hostinaddr_p));
				return 0x00;
			}
			strcpy(hostname_p, he_p->h_name);
			zlog_debug(zc, "h_name [%s]", he_p->h_name);
			zlog_debug(zc, "h_addrtype [%d]", he_p->h_addrtype);
			zlog_debug(zc, "h_length [%d]", he_p->h_length);
			zlog_debug(zc, "h_addr_list[0][%s]", inet_ntoa(*(struct in_addr * ) (he_p->h_addr_list[0])));
			zlog_debug(zc, "h_aliases[0][%s]", he_p->h_aliases[0]);
			return 0xff;
		}
	}
	return 0x00;
}

void createNicNameMap() {
	struct ifreq ifr;
	struct ifconf ifc;

	char buf[16384];
	unsigned char ethDest[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1) {
		/* handle error*/
		zlog_fatal(zc, "socket()");
	}

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
		/* handle error */
		zlog_fatal(zc, "ioctl()");
	}

	struct ifreq* it = ifc.ifc_req;
	const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

	for (; it != end; ++it) {
		strcpy(ifr.ifr_name, it->ifr_name);
		if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
			if (!(ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
				if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
					// the nic name
					strcpy(nicName2ethHdr[nicNum].nicName, ifr.ifr_name);

					// the ethernet header
					memcpy(nicName2ethHdr[nicNum].ethHdr.h_source, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
					memcpy(nicName2ethHdr[nicNum].ethHdr.h_dest, ethDest, ETH_ALEN);
					nicName2ethHdr[nicNum].ethHdr.h_proto = htons(ETH_P_IP);

					// the ip address
					ifr.ifr_addr.sa_family = AF_INET;
					ioctl(sock, SIOCGIFADDR, &ifr);
					nicName2ethHdr[nicNum].ipAddress = ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr;
					++nicNum;
				}
			}
		}

	}
	close(sock);

	return;
}

//struct ethhdr * getEthHdr(const char * nicName) {
//	int i;
//	for (i = 0; i < nicNum; i++) {
//		if (!strcmp(nicName, nicName2ethHdr[i].nicName)) {
//			return &(nicName2ethHdr[i].ethHdr);
//		}
//	}
//	return NULL;
//}

//struct in_addr getIPAddress(const char * nicName) {
//	int i;
//	struct in_addr ret = { .s_addr = 0 };
//	for (i = 0; i < nicNum; i++) {
//		if (!strcmp(nicName, nicName2ethHdr[i].nicName)) {
//			return nicName2ethHdr[i].ipAddress;
//		}
//	}
//	return ret;
//}

void deleteRamdiskFiles() {
	system("exec rm -rf /mnt/ramdisk/*");
	return;
}

PatchTuple * createPatchTuple(seqnum_t seqStart, seqnum_t seqEnd) {
	PatchTuple * pt = (PatchTuple *) malloc(sizeof(PatchTuple));
	pt->stt_seq = seqStart;
	pt->end_seq = seqEnd;
	return pt;
}

int sendMsg(int socket, apr_queue_t * queue_p, MsgToken ** msgToken_pp) {
	ssize_t bytes_sent;
	ssize_t total_bytes_sent = 0; // bytes sent by this function
	MsgToken *msgToken_p = NULL;
	MsgHdr msgHdr;

	do {
		if (*msgToken_pp == NULL) { // if nothing remain, put it in msgTuple_p if there is something to send
			// zlog_debug(zc, "[before trypop] queue_p [%p] mt_pp [%p] mt_p [%p] (APR_EINTR[%d] APR_EAGAIN[%d] APR_EOF[%d] APR_SUCCESS[%d])", queue_p, msgToken_pp, *msgToken_pp, APR_EINTR, APR_EAGAIN, APR_EOF, APR_SUCCESS);
			apr_status_t status = apr_queue_trypop(queue_p, (void **) (msgToken_pp));
			if (!(status == APR_SUCCESS && (*msgToken_pp) != NULL)) { // nothing to send
				// zlog_debug(zc, "[get thing to send] [nothing] msg queue status [%d], mt_pp [%p]", status, msgToken_pp);
				(*msgToken_pp) = NULL;
				// zlog_debug(zc, "[get thing to send] [nothing] mt_pp [%p], mt_p [%p]", msgToken_pp, *msgToken_pp);
				break;
			} else {
				// zlog_debug(zc, "[get thing to send] [queue] msg queue status [%d], msgToken [%p]", status, *msgToken_pp);
			}
		} else {
			// zlog_debug(zc, "[get thing to send] [last round] sending bytes remaining, msgToken [%p]", *msgToken_pp);
		}

		msgToken_p = *msgToken_pp;
		parseMsgHdr(&msgHdr, msgToken_p->buf_p);
		// zlog_debug(zc, "[sending] [before send] byte_sent [%d], msg_len [%d], padding_len [%d] (BcdID[%016lx][%016lx][%ld], type[%d])", msgToken_p->byte_sent, msgToken_p->msg_len, msgToken_p->padding_len, msgHdr.bcd_id.app_id.msb, msgHdr.bcd_id.app_id.lsb, msgHdr.bcd_id.data_id, msgHdr.type);
		if (msgToken_p->byte_sent < msgToken_p->msg_len) { // send message
			bytes_sent = send(socket, (msgToken_p->buf_p) + msgToken_p->byte_sent, (msgToken_p->msg_len - msgToken_p->byte_sent), MSG_DONTWAIT);
		} else { // send padding
			bytes_sent = send(socket, zeros, (msgToken_p->msg_len + msgToken_p->padding_len) - msgToken_p->byte_sent, MSG_DONTWAIT);
		}
		// zlog_debug(zc, "[sending] [after send] bytes_sent [%d]", bytes_sent);

		if (bytes_sent > 0) {
			msgToken_p->byte_sent += bytes_sent;
			total_bytes_sent += bytes_sent;
		} else if (bytes_sent < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				perror("");
				// zlog_debug(zc, "[sent] send() returns -1 EAGAIN|EINTR errno [%d]", errno);
			} else {
				perror("");
				// zlog_debug(zc, "[sent] send() returns -1 errno [%d]", errno);
			}
			*msgToken_pp = msgToken_p;
			break;
		} else {
			perror("");
			// zlog_debug(zc, "[sent] send() returns 0 errno [%d]", errno);
			*msgToken_pp = msgToken_p;
			break;
		}

		if (msgToken_p->byte_sent == (msgToken_p->msg_len + msgToken_p->padding_len)) { // finish sending the message including the padding
			deleteMsgToken(msgToken_p);
			*msgToken_pp = NULL;
		} else {
			// zlog_debug(zc, "[done] message is sent partially");
			*msgToken_pp = msgToken_p;
			break;
		}
	} while (1);
	// zlog_debug(zc, "return [%d]", total_bytes_sent);
	return total_bytes_sent;
}

void recverState_stopFathPath(RecverState *recv_p) {
	if (recv_p->is_fastpath) {
		gettimeofday(&(recv_p->tv_finish_fastpath), NULL);
		recv_p->is_fastpath = 0;
	} else {
		zlog_warn(zc, "fastpath has stopped, did[%ld]", recv_p->bcd_id.data_id);
	}
	return;
}

void recverState_startFathPath(RecverState *recv_p) {
	if (!(recv_p->is_fastpath)) {
		gettimeofday(&(recv_p->tv_start_fastpath), NULL);
		recv_p->is_fastpath = 1;
	} else {
		zlog_warn(zc, "fastpath has started, did[%ld]", recv_p->bcd_id.data_id);
	}
	return;
}

void deleteSendQueue(apr_queue_t * sendQueue_p) {
	if (!sendQueue_p) { /* empty queue */
		zlog_warn(zc, "apr_queue_t is nil");
		return;
	}

	apr_status_t s;
	MsgToken *mt_p;

	while (1) { /* delete queue */
		s = apr_queue_interrupt_all(sendQueue_p);
		if (s != APR_SUCCESS) {
			zlog_error(zc, "apr_queue_interrupt_all unSUCCESS");
			break;
		}
		s = apr_queue_trypop(sendQueue_p, (void **) (&mt_p));
		if (s == APR_EAGAIN) {
			// D("[debug] queue is empty");
			break;
		} else if (s == APR_EOF) { /* empty / terminated queue */
			// D("[debug] queue has been terminated");
			break;
		} else if (s == APR_SUCCESS) {
			deleteMsgToken(mt_p);
		} else {	//TODO
		}
	}
	apr_queue_term(sendQueue_p);

	return;
}

void deleteMsgToken(MsgToken *mt_p) {
	if (!mt_p) {
		// D("msgTuple is NULL");
		return;
	}
	free(mt_p->buf_p);
	free(mt_p);
	return;
}

void deletePatchTuple(PatchTuple *pt_p) {
	if (!pt_p) {
		zlog_warn(zc, "PatchTuple is nil");
		return;
	}
	free(pt_p);
}

void deleteRecverState(RecverState *recv_p) {
	if (!recv_p) {
		zlog_warn(zc, "RecverState is nil");
		return;
	}
	if (!(recv_p->tail_patch_buffer)) {
		free(recv_p->tail_patch_buffer);
	}
	closeFile(&(recv_p->fd_p));
	closeFile(&(recv_p->fd_patch_p));

	removeData(recv_p->bcd_filename);
	free(recv_p);
	return;
}

void trashFdState(FdState *fds_p) {
	assert(fds_p);
	assert(!fds_p->is_trashed); // fds_p has not been trashed
	zlog_debug(zc, "fds[%p] is marked as trashed", fds_p);
	zlog_debug(zc, "fds_p->state_p[%p] is marked as trashed", fds_p->state_p);
	fds_p->is_trashed = 1;
	gettimeofday(&(fds_p->tv_trashed), NULL);
	close(fds_p->fd);
}

void deleteFdState(FdState *fds_p) {
	assert(fds_p);
	assert(fds_p->is_trashed);

	switch (fds_p->fd_type) {
	case CTRL_RECV_TCP:
		deleteRecvSocketState(fds_p->state_p);
		break;
	case IFACE_UDS:
		deleteUDSState(fds_p->state_p);
		break;
	default:
		break;
	}

	deleteSendQueue(fds_p->sendQueue_p); /* send */
	deleteMsgToken(fds_p->remainMsgToken_p);
	apr_pool_destroy(fds_p->pool_p);

	free(fds_p);
}

void deleteUDSState(LocalSocketState *lsock_p) {
	if (!lsock_p) {
		zlog_warn(zc, "LocalSocketState is nil");
		return;
	}
	zlog_debug(zc, "delete LocalSocketState");

	/* remove UDSocket from hashmap */
	AppState *app_p = lsock_p->app_p;
	apr_hash_set(app_p->uds_hash, &(lsock_p->id), sizeof(int), NULL);

	if (apr_hash_count(app_p->uds_hash) == 0) {
		deleteAppState(app_p);
	}

	free(lsock_p);
}

void deleteAppState(AppState * app_p) {
	if (!app_p) {
		zlog_warn(zc, "deleteAppState is nil");
		return;
	}

	zlog_debug(zc, "delete AppState %s", app_p->app_id_str);
	closeFile(&(app_p->fd_stats_p));
	apr_hash_clear(app_p->uds_hash);

	apr_hash_index_t *hi;
	MulticastState *multi_p;
	for (hi = apr_hash_first(NULL, app_p->bcd_sender_hash); hi; hi = apr_hash_next(hi)) {
		apr_hash_this(hi, NULL, NULL, (void*) &multi_p);
		closeFile(&(multi_p->fd));
		closeFile(&(multi_p->fd_patch));
	}
	apr_hash_clear(app_p->bcd_sender_hash);

	deleteDir(app_p->bcd_dir_str);
}

void deleteRecvSocketState(RecvSocketState *rsock_p) {
	if (!rsock_p) {
		zlog_warn(zc, "RecvSocketState is nil");
		return;
	}
	free(rsock_p);
}

void user1_handler(int sig) {
//	D("--------------- user1 handler");
	return;
}

void removeData(char * filename) {
	if (!remove(filename)) {
		zlog_info(zc, "remove file %s", filename);
	} else {
		zlog_warn(zc, "remove file fail %s", filename);
	}
}

void closeFile(FILE ** fd_pp) {
	if (*fd_pp) {
		fflush(*fd_pp);
		fclose(*fd_pp);
		*fd_pp = 0;
	}
}

void initSharedDataStructures() {
	if (apr_initialize() != APR_SUCCESS) {
		abort();
	}
	atexit(apr_terminate);

	/* from sender */
	apr_pool_create(&pool_NetmapS_p, NULL);
	apr_pool_create(&pool_ssock_p, NULL);
	apr_pool_create(&pool_HashMulticast_S_p, NULL);
	hashMulticast = apr_hash_make(pool_HashMulticast_S_p);

	ssockList = List_create();

	/* recevr hashmap */
	apr_pool_create(&(pool_rsock_p), NULL);
	apr_pool_create(&(pool_lsock_p), NULL);

	apr_pool_create(&pool_HashRecver_L_p, NULL);
	hashRecver = apr_hash_make(pool_HashRecver_L_p);

	lsockList = List_create();
	rsockList = List_create();

	activeMulticast_p = NULL;
	apr_skiplist_init(&(queuePush_p), pool_lsock_p); /* init the priority queue for the senders */
	apr_skiplist_set_compare(queuePush_p, (apr_skiplist_compare) pushcmp, (apr_skiplist_compare) pushcmp);

	hashApp = apr_hash_make(pool_lsock_p);
}

// TODO: merge to FdState
SendSocketState *createSendSocketState(int sock, struct sockaddr_in addr) {
	SendSocketState *ssock_p = (SendSocketState *) malloc(sizeof(SendSocketState));
	bzero(ssock_p, sizeof(SendSocketState));
	apr_pool_create(&(ssock_p->pool_ssock_p), pool_ssock_p);

	ssock_p->socket = sock;
	ssock_p->client_addr = addr;

	ssock_p->recv_ed_byte = 0;
	ssock_p->recv_expect_byte = BYTE_MAX;

	apr_queue_create(&(ssock_p->sendQueue_p), APR_QUEUE_CAPACITY, ssock_p->pool_ssock_p); /* flag to indicate whether there is more data to send */

// init the ring holding the patch token
	ssock_p->ringPatchTuple = (my_ring_t *) apr_palloc(ssock_p->pool_ssock_p, sizeof(my_ring_t));
	APR_RING_INIT(ssock_p->ringPatchTuple, _my_elem_t, link);
	ssock_p->mh.type = MSG_NONTYPE;

	return ssock_p;
}

SenderState * createSenderState(int id, const struct in_addr *skey_p, BcdID *bcd_id_p, SendSocketState *ssock_p) {
	SenderState * sender_p = (SenderState *) malloc(sizeof(SenderState));
	bzero(sender_p, sizeof(SenderState));

	sender_p->skey = *skey_p;
	sender_p->bcd_id = *bcd_id_p;
	sender_p->id = id;

	sender_p->ssock_p = ssock_p;
	gettimeofday(&(sender_p->tv_init), NULL);

//	sender_p->has_first_seq = 0;
//	sender_p->has_fast_done = 0;
	sender_p->has_all_done = 0;
	sender_p->is_distinct_rack = ((sender_p->skey.s_addr >> 24) - IP_OFFSET) / NUM_SERVER_PER_RACK != ((hostinaddress_ctrl.s_addr >> 24) - IP_OFFSET) / NUM_SERVER_PER_RACK;
	zlog_debug(zc, "is_distinct_rack r[%s] vs s[%s] -> [%d]", inet_ntoa(sender_p->skey), inet_ntoa(hostinaddress_ctrl), sender_p->is_distinct_rack);

	return sender_p;
}

static int SenderStateCmp(void *a, void *b) {
	return ((SenderState *) a)->seqEnd < ((SenderState *) b)->seqEnd ? -1 : 1;
}

int pushcmp(void *a, void *b) {
	return timercmp(&(((MulticastState *) a)->tv_push), &(((MulticastState *) b)->tv_push), <) ? 1 : -1;
}

MulticastState *createBcdState(BcdID *bcd_id_p, MsgIntQ * msgIntQ_p, MsgWrtQ * msgWrtQ_p, uint8_t rate_type, double rate_f, char *nic_name) {
	MulticastState *multi_p = (MulticastState *) malloc(sizeof(MulticastState));
	bzero(multi_p, sizeof(MulticastState));

	zlog_info(zc, "create MulticastState for did[%ld] @ [%p]", bcd_id_p->data_id, (void * )multi_p);

	apr_pool_create(&(multi_p->pool_ssock_p), pool_ssock_p);

	apr_skiplist_init(&(multi_p->sender_end_seq_pq_p), multi_p->pool_ssock_p); /* init the priority queue for the senders */
	apr_skiplist_set_compare(multi_p->sender_end_seq_pq_p, (apr_skiplist_compare) SenderStateCmp, (apr_skiplist_compare) SenderStateCmp);

	multi_p->sender_addr_hash_p = apr_hash_make(multi_p->pool_ssock_p);
	multi_p->sender_end_seq_arr_idx_start = 0;
	multi_p->sender_end_seq_arr_idx_end = 0;
	multi_p->delete_counter = 0;

	multi_p->bcd_id = *bcd_id_p; /* key of the multicast state */
	BcdIdtoFilename(bcd_id_p, multi_p->bcd_filename);
	multi_p->msgIntQ = *msgIntQ_p;
	multi_p->msgWrtQ = *msgWrtQ_p;

	zlog_debug(zc, "bcd_filename %s", multi_p->bcd_filename);

	multi_p->fd = fopen(multi_p->bcd_filename, "r"); /* file descriptor for reading the data */
	if (!(multi_p->fd)) {
		zlog_fatal(zc, "file [%s] does not exist", multi_p->bcd_filename);
		return NULL;
	}
	fseek(multi_p->fd, 0L, SEEK_END);
	multi_p->data_size = ftell(multi_p->fd);
	zlog_info(zc, "data size [%ld] vs [%ld]", multi_p->data_size, multi_p->msgWrtQ.bcd_size);
	if (multi_p->data_size <= 0) {
		zlog_fatal(zc, "data size is zero");
		return NULL;
	}
	rewind(multi_p->fd);

	multi_p->fd_patch = fopen(multi_p->bcd_filename, "r"); /* file descriptor for reading the data */
	rewind(multi_p->fd_patch);

	multi_p->byte_payload = data_payload_len; /* number of bytes in each data packet */
	multi_p->seq_max = multi_p->data_size / multi_p->byte_payload;
	multi_p->byte_payload_min = multi_p->data_size % multi_p->byte_payload; /* number of bytes in the last data packet */
	if (!(multi_p->byte_payload_min)) {
		multi_p->byte_payload_min = multi_p->byte_payload;
	} else {
		multi_p->seq_max++;
	}

	multi_p->bcd_state = BCDSTATE_DEAD;

	multi_p->n_init = 0; /* number of initiated senders */
	multi_p->n_first_seq = 0; /* number of finished senders */
	multi_p->n_all_done = 0; /* number of finished senders */
	multi_p->is_first_seq_distinct_rack = 0;
	multi_p->is_all_done_distinct_rack = 0;
	multi_p->is_distinct_rack = 0;

	multicastState_reset(multi_p);
	multi_p->seq_cur = 0L; /* current sequence */

	multi_p->eth_hdr_p = &hostethhdr_data;
// TODO ip header should be unique to each machine... no need to have it for each single multicast data
	initIPHeader(&(multi_p->ip_hdr), hostinaddress_data, IPPROTO_UDP);
	initUDPHeader(&(multi_p->udp_hdr), DATA_SRC_PORT, DATA_DST_PORT);

// rate control and statistics
	multi_p->rate_type = rate_type;    // the rate limit type BW or PPS
	multi_p->rate = rate_f;     // the rate limit value

// std_rate_f_delta ---> micro-seconds per byte
	if (multi_p->rate_type == RATE_GBPS) { // Gbps (Gigabit per second)
		/**
		 *  8 bit     u          1             u * sec       8          8
		 * ------- * --- * -------------  =>  --------- * -------  =>  ---
		 *  byte      u       G * bit            byte      u * G        k
		 *                  [---------]
		 *                      sec
		 *
		 */
		multi_p->std_rate_f_delta = 1.0 / (rate_f * 1.0e9 / (8.0)) * 1.0e6;
	} else if (multi_p->rate_type == RATE_MPPS) { // Mpps ( Mega-packet per second)
		/**
		 *   u               1                      u * sec       1
		 *  --- * ---------------------------  =>  --------- * -------  =>  1
		 *   u       M * packet       byte            byte      u * M
		 *         [------------] * --------
		 *              sec          packet
		 *
		 */
		multi_p->std_rate_f_delta = 1.0 / ((rate_f * multi_p->byte_payload * 8.0 / 1000) * 1.0e9 / (8.0)) * 1.0e6;
	} else if (multi_p->rate_type == RATE_UNKNOWN) {
		zlog_fatal(zc, "unknown data rate type");
		return NULL;
	}
	multi_p->rate_f_delta = multi_p->std_rate_f_delta;
	multi_p->is_stats_logged = 0;

	zlog_debug(zc, "rate_type[%d], rate[%g], rate_f_delta[%g]", multi_p->rate_type, multi_p->rate, multi_p->std_rate_f_delta);

	return multi_p;
}

void multicastState_stats(MulticastState * multi_p, FILE* fd_stats_p) {
	zlog_debug(zc, "multicastState_stats is called for did[%ld]", multi_p->bcd_id.data_id);
	if (!(multi_p->is_stats_logged)) {
		multi_p->is_stats_logged = 1;
		char finish_buf[TIME_BUF_LEN];
		gettimeofmillisecond(finish_buf, TIME_BUF_LEN, &(multi_p->tv_last_all_done));

		/* get the sender stats */
		struct timeval tv_active, tv_first_seq_vary, tv_all_done_vary, tv_attempt, tv_fullrate, tv_entire;
		struct timeval tv_path_request_approve, tv_path_occupy;
		float tv_active_ms, tv_first_seq_vary_ms, tv_all_done_vary_ms, tv_attemptive_ms, tv_fullrate_ms, tv_entire_ms, tv_std_ms;
		float tv_path_request_approve_ms, tv_path_occupy_ms;
		timersub(&(multi_p->tv_last_data), &(multi_p->tv_first_data), &tv_active);
		timersub(&(multi_p->tv_last_first_seq), &(multi_p->tv_first_first_seq_distinct_rack), &(tv_first_seq_vary));
		timersub(&(multi_p->tv_last_all_done), &(multi_p->tv_first_all_done_distinct_rack), &(tv_all_done_vary));
		timersub(&(multi_p->tv_attempt_finish), &(multi_p->tv_first_data), &(tv_attempt));
		timersub(&(multi_p->tv_last_data), &(multi_p->tv_attempt_finish), &(tv_fullrate));
		timersub(&(multi_p->tv_last_all_done), &(multi_p->tv_first_data), &(tv_entire));
		timersub(&(multi_p->tv_path_approve), &(multi_p->tv_path_request), &(tv_path_request_approve));
		timersub(&(multi_p->tv_path_release), &(multi_p->tv_path_approve), &(tv_path_occupy));

		tv_active_ms = tv_active.tv_sec * 1000.0 + tv_active.tv_usec / 1000.0;
		tv_first_seq_vary_ms = tv_first_seq_vary.tv_sec * 1000.0 + tv_first_seq_vary.tv_usec / 1000.0;
		tv_all_done_vary_ms = tv_all_done_vary.tv_sec * 1000.0 + tv_all_done_vary.tv_usec / 1000.0;
		tv_attemptive_ms = tv_attempt.tv_sec * 1000.0 + tv_attempt.tv_usec / 1000.0;
		tv_fullrate_ms = tv_fullrate.tv_sec * 1000.0 + tv_fullrate.tv_usec / 1000.0;
		tv_entire_ms = tv_entire.tv_sec * 1000.0 + tv_entire.tv_usec / 1000.0;
		tv_path_request_approve_ms = tv_path_request_approve.tv_sec * 1000.0 + tv_path_request_approve.tv_usec / 1000.0;
		tv_path_occupy_ms = tv_path_occupy.tv_sec * 1000.0 + tv_path_occupy.tv_usec / 1000.0;
		tv_std_ms = (multi_p->data_size + DATA_PAYLOAD_OFFSET * multi_p->seq_max) * multi_p->std_rate_f_delta / 1000.0;

		/* log to stats */
		fprintf(fd_stats_p, "%s [SEND][%s][%s][%016lx][%016lx][%ld]\t|", finish_buf, multi_p->msgIntQ.app_id, inet_ntoa(hostinaddress_ctrl), multi_p->bcd_id.app_id.msb, multi_p->bcd_id.app_id.lsb, multi_p->bcd_id.data_id);
		fprintf(fd_stats_p, "data:[%ld]=[%d]x[%d]+[%d]\t|dup_send:[%ld]/[%ld]=[%.6f]x\t|", multi_p->data_size, multi_p->byte_payload, multi_p->seq_max - 1, multi_p->byte_payload_min, multi_p->n_byte_total,
			(multi_p->data_size + BCDMETADATA_LEN + DATA_PAYLOAD_OFFSET * multi_p->seq_max), 1.0 * multi_p->n_byte_total / (multi_p->data_size + BCDMETADATA_LEN + DATA_PAYLOAD_OFFSET * multi_p->seq_max));
		fprintf(fd_stats_p, "active:[%.6f]ms,ineffic[%.6f]x~[*]/[%.6f]ms\t|fs/ad vary:[%.6f]ms,[%.6f]ms\t|", tv_active_ms, (tv_active_ms) / (tv_std_ms) - 1, tv_std_ms, tv_first_seq_vary_ms, tv_all_done_vary_ms);
		fprintf(fd_stats_p, "attempt/full:[%.6f]ms,[%.6f]ms=[%.6f]%%,[%.6f]%%=[%d],[%d]pkts=[%ld],[%ld]bytes\t|", tv_attemptive_ms, tv_fullrate_ms, (tv_attemptive_ms) / (tv_active_ms) * 100.0, (tv_fullrate_ms) / (tv_active_ms) * 100.0,
			multi_p->n_packet_attempt, multi_p->n_packet_total - multi_p->n_packet_attempt, multi_p->n_byte_attempt, multi_p->n_byte_total - multi_p->n_byte_attempt);
		fprintf(fd_stats_p, "entire:[%.6f]ms\t|ctrl_overhead[%.6f]ms[%.6f]x\t|patch:[%d],[%.6f]%%\t|manager:[%.6f]ms,[%.6f]ms|[%s]\n", tv_entire_ms, (tv_entire_ms) - (tv_std_ms), ((tv_entire_ms) / (tv_std_ms) - 1), multi_p->n_patch_total,
			100.0 * multi_p->n_patch_total / multi_p->seq_max, tv_path_request_approve_ms, tv_path_occupy_ms, hostname_ctrl);
	}
}

void multicastState_reset(MulticastState * multi_p) {
	if (!multi_p)
		return;
	multi_p->seq_end = SEQNUM_MAX;
	if (multi_p->bcd_state == BCDSTATE_DEAD) { /* reset all the rate related counter */
		multi_p->seq_end_max = 0;
		multi_p->n_byte = 0;    // # of bytes have been sent
		multi_p->n_packet = 0;    // # of bytes have been sent
		gettimeofday(&(multi_p->tv_sendStart), NULL);
	}
}

void multicastState_updateRate(MulticastState * multi_p, double new_rate_f_delta) {
	if (!multi_p)
		return;
	multi_p->n_byte = 0;
	multi_p->n_packet = 0;
	gettimeofday(&(multi_p->tv_sendStart), NULL);
	multi_p->rate_f_delta = new_rate_f_delta;
	return;
}

// TODO: double check the index...
void multicastState_updateSeqEnd(MulticastState * multi_p, SenderState * sender_p) {
// seqnum_t seq_end;
	seqnum_t seq_fst = sender_p->msf.seq_first;
	uint32_t round_num;
	/* decide the seq end of the current msf */
	if (seq_fst >= (multi_p->seq_cur % multi_p->seq_max)) { /* no need to send another round */
		round_num = multi_p->seq_cur / multi_p->seq_max;
		// seq_end = (multi_p->seq_cur / multi_p->seq_max) * multi_p->seq_max + msf_p->seq_first;
	} else { /* need to send another round */
		round_num = multi_p->seq_cur / multi_p->seq_max + 1;
		// seq_end = ((multi_p->seq_cur / multi_p->seq_max) + 1) * multi_p->seq_max + msf_p->seq_first;
	}
	sender_p->seqEnd = round_num * multi_p->seq_max + seq_fst; /* last seq of this paticular receiver */

	SenderState ** s_pp = (SenderState **) apr_skiplist_alloc(multi_p->sender_end_seq_pq_p, sizeof(SenderState *));
	*s_pp = sender_p;
	if (!apr_skiplist_insert(multi_p->sender_end_seq_pq_p, s_pp)) { /* insert seqEnd (SenderState) to the priority queue for the seqEnd of each recver */
		zlog_fatal(zc, "apr_skiplist_insert fail");
	}
	zlog_info(zc, "did[%ld] seq_first[%d], seq_end[%d], seq_cur[%d], round_num[%d]", sender_p->bcd_id.data_id, sender_p->msf.seq_first, sender_p->seqEnd, multi_p->seq_cur, round_num);

	if (sender_p->seqEnd > multi_p->seq_end_max) {
		zlog_info(zc, "update seq_end_max from [seq: %d(rn: %d)] to [seq: %d(rn: %d)]", multi_p->seq_end_max % multi_p->seq_max, multi_p->seq_end_max / multi_p->seq_max, (sender_p->seqEnd) % multi_p->seq_max,
			(sender_p->seqEnd) / multi_p->seq_max);
		multi_p->seq_end_max = sender_p->seqEnd;
	}
	if (multi_p->n_init == multi_p->n_first_seq) { // received all FIRST_SEQ
		zlog_info(zc, "got the last first_seq, update seq_end from [seq: %d(rn: %d)] to [seq: %d(rn: %d)]", multi_p->seq_end % multi_p->seq_max, multi_p->seq_end / multi_p->seq_max, multi_p->seq_end_max % multi_p->seq_max,
			multi_p->seq_end_max / multi_p->seq_max);
		multi_p->seq_end = multi_p->seq_end_max;
	}
	return;

}

my_elem_t *createPatchTupleRingElement(BcdID *bcd_id_p, PatchTuple *pt, struct timeval *tv_init) {
	my_elem_t *elem_p = (my_elem_t *) malloc(sizeof(my_elem_t));
	if (elem_p == NULL) {
		zlog_fatal(zc, "malloc fails for my_elem_t");
	}
	APR_RING_ELEM_INIT(elem_p, link);
	elem_p->bcd_id = *bcd_id_p;
	elem_p->pt = *pt;
	elem_p->tv_init = *tv_init;
// D("tv_init ------------- sec[%d], usec[%d]", (elem_p->tv_init).tv_sec, (elem_p->tv_init).tv_usec);
	gettimeofday(&(elem_p->tv_patchReqRecv), NULL);
	return elem_p;
}

void processPatchTuple(SendSocketState *ssock_p) {
	my_elem_t *elem_p;
	PatchTuple *pt_p;
//	int dataBytes, payloadBytes;
	char *payload_p;
	uint8_t ret;

// TODO: should check the capacity of the queue in the while condition (not sure if this is the right way to check...)
	while (!APR_RING_EMPTY(ssock_p->ringPatchTuple, _my_elem_t, link) && apr_queue_size(ssock_p->sendQueue_p) < (APR_QUEUE_CAPACITY - 100)) { /* loop over all the patchTuple in the ring */
		elem_p = APR_RING_LAST(ssock_p->ringPatchTuple); /* peek the first element (patchTuple) */
		if (!elem_p) {
			zlog_error(zc, "APR_RING_LAST returns nil");
			break;
		}
		pt_p = &(elem_p->pt); /* get the patch tuple */

		MulticastState *multi_p = (MulticastState *) apr_hash_get(hashMulticast, (const void*) (&(elem_p->bcd_id)), (apr_ssize_t) sizeof(BcdID)); /* get the multicast state */
//		const SenderKey sk = { .xid = ssock_p->mh.xid, .sin_addr = ssock_p->client_addr.sin_addr, .sin_port = ssock_p->client_addr.sin_port, .pad = 0 }; /* mistake... */
		// SenderState *sender_p = (SenderState *) apr_hash_get(multi_p->hashSender, (const void *) (&sk), (apr_ssize_t) sizeof(SenderKey)); /* get the sender state */
//		printSenderHashmap();
//		D("[patch] .xid[%d] .sin_port[%d] .s_addr[%d] sender_p[%p]", sk.xid, sk.sin_port, sk.sin_addr, sender_p);

		int fp = fseek(multi_p->fd_patch, pt_p->stt_seq * multi_p->byte_payload, SEEK_SET);
		if (fp < 0) { /* seek to the stt pos */
			perror("fseek() returns -1");
		}

		while (pt_p->stt_seq < pt_p->end_seq) { /* loop over the seq in the patchTuple */
			payload_p = (char *) malloc(9216); /*create the msg header, payload, i.e.,(data header + data) */

			uint16_t payloadBytes = createData(payload_p, multi_p, multi_p->fd_patch, pt_p->stt_seq);		//, 0, 0, 0, 0, 0, 0);

//			dataBytes = (pt_p->stt_seq != multi_p->seq_max - 1) ? (multi_p->payload_size) : (multi_p->payload_size_last); /* # of bytes of the pure data */
//			payloadBytes = (pt_p->stt_seq != 0) ? (DATA_HDRLEN + dataBytes) : (DATA_HDRLEN + dataBytes + DATA_METADATALEN); /* # of bytes in the payload of a message */
//			payload_p = (uint8_t *) malloc(payloadBytes); /*create the msg header, payload, i.e.,(data header + data) */

			// printf("MSG_PATCH_DATA: [%d]\n", pt_p->stt_seq);
//			create_data_from_file(payload_p, multi_p->xid, pt_p->stt_seq, dataBytes, multi_p->fd_patch, &(sender_p->tv_init), &(pt_p->tv_lossTiming),
//				&(pt_p->tv_patchReqSent), &(elem_p->tv_patchReqRecv));
//
//			if (pt_p->stt_seq == 0) { /* append the metadata to the 0 packet */
//				createDataMetadata(payload_p + payloadBytes - DATA_METADATALEN, multi_p->data_size);
//			}

			ret = insertMsg2SendQ(ssock_p->sendQueue_p, &(multi_p->bcd_id), MSG_PATCH_DATA, payload_p, payloadBytes, 0, 0);

			free(payload_p); // TODO: the payload should have only one mem malloc

			if (!ret) {
				zlog_warn(zc, "cannot insert MSG_PATCH_DATA seq[%d] did[%d]", pt_p->stt_seq, multi_p->bcd_id.data_id);
				break;
			}

			// zlog_debug(zc, "did[%ld] sent MSG_PATCH_DATA seq[%d] to [%s]", multi_p->bcd_id.data_id, pt_p->stt_seq, inet_ntoa(ssock_p->client_addr.sin_addr));
			(pt_p->stt_seq)++;
		}

		if (pt_p->stt_seq == pt_p->end_seq) { /* the peeked tuple should be removed from the ring if all its seqs have been processed, else break the loop, since the sendQueue is full */
			APR_RING_REMOVE(elem_p, link);
			free(elem_p);
		} else {/*the current tuple has not been fully processed, don't remove the tuple and just break the loop */
			// D("[warn] tuple is not fully processed: sst[%d] end[%d] @ queueSize[%d]\n", pt_p->stt_seq, pt_p->end_seq, apr_queue_size(ssock_p->sendQueue_p));
			break;
		}
	}
	return;
}

void deleteRingPatchTuple(my_ring_t *ringPatchTuple) {
	return;
}

void deleteSendSocketState(SendSocketState * ssock_p) {
	if (!ssock_p) {
		zlog_warn(zc, "SendSocketState is nil");
		return;
	}
	close(ssock_p->socket);
	deleteSendQueue(ssock_p->sendQueue_p);
	deleteMsgToken(ssock_p->remainMsgToken_p);
	deleteRingPatchTuple(ssock_p->ringPatchTuple);
	apr_pool_destroy(ssock_p->pool_ssock_p);
	free(ssock_p);
	return;
}

void deleteSenderState(SenderState * send_p) {
	if (!send_p) {
		zlog_warn(zc, "SenderState is nil");
		return;
	}
	free(send_p);
	return;
}

void deleteMulticastState(MulticastState * multi_p) {
	if (!multi_p) {
		zlog_warn(zc, "MulticastState is nil");
		return;
	}

//	apr_skiplist_destroy(multi_p->pQueue_Sender, NULL);
//	apr_hash_clear(multi_p->hashSender);
//	apr_pool_destroy(multi_p->pool_ssock_p);
	free(multi_p->sender_end_seq_arr_p);
	closeFile(&(multi_p->fd)); /* file descriptor for reading the data */
	closeFile(&(multi_p->fd_patch));
	removeData(multi_p->bcd_filename);
	free(multi_p);
	return;
}

bytes_t createData(char *payload_p, MulticastState *multi_p, FILE *fd, seqnum_t seq) { //, uint32_t f1, uint32_t f2, uint32_t f3, uint32_t f4, uint32_t f5, uint32_t f6) {
	bytes_t dataBytes = ((seq + 1) == multi_p->seq_max) ? (multi_p->byte_payload_min) : (multi_p->byte_payload); /* # of bytes of data content */
	bytes_t payloadBytes = (seq == 0) ? (DATA_HDRLEN + dataBytes + BCDMETADATA_LEN) : (DATA_HDRLEN + dataBytes); /* # of bytes in the udp payload */
	createDataFromFile(payload_p, &(multi_p->bcd_id), seq, dataBytes, fd); //, f1, f2, f3, f4, f5, f6, fd); /* create udp payload */
	if (seq == 0) { /* append the metadata to the 0 packet */
		createBcdMetadata((payload_p + payloadBytes - BCDMETADATA_LEN), multi_p->data_size, multi_p->msgIntQ.app_id);
	}

// D("1 dataBytes[%ld] mstate_p->payload_size_max[%d] mstate_p->seq_max[%d] mstate_p->data_size[%ld]", dataBytes, mstate_p->payload_size_max, mstate_p->seq_max, mstate_p->data_size);
//	dataBytes = ((multi_p->seq_cur + 1) % multi_p->seq_max) ? (multi_p->payload_size) : (multi_p->payload_size_last); /* # of bytes of the pure data */
//	payloadBytes = (multi_p->seq_cur % multi_p->seq_max) ? (DATA_HDRLEN + dataBytes) : (DATA_HDRLEN + dataBytes + DATA_METADATALEN); /* # of bytes in the payload (after the udp header) of a message */

	/* fill the body of the packets */
//	create_data_from_file(payload_p, multi_p->xid, (multi_p->seq_cur) % (multi_p->seq_max), dataBytes, multi_p->fd, NULL, NULL, NULL, NULL); /* payload */
//	memcpy(txbuf, multi_p->eth_hdr, ETH_HLEN); /* ethernet header */
//	memcpy(ip_p, &(multi_p->ip_hdr), sizeof(struct ip)); /* ip header */
//	ip_p->ip_len = htons(IP4_HDRLEN + UDP_HDRLEN + payloadBytes);
//	ip_p->ip_sum = 0;
//	ip_p->ip_sum = computeIPchecksum((uint16_t *) ip_p, IP4_HDRLEN);
//	memcpy(udp_p, &(multi_p->udp_hdr), sizeof(struct udphdr)); /* udp header */
//	udp_p->len = htons(UDP_HDRLEN + payloadBytes);
//	if (!(multi_p->seq_cur % multi_p->seq_max)) { /* meta data */
//		*(uint32_t *) (payload_p + payloadBytes - DATA_METADATALEN) = ntohl(multi_p->data_size);
//	}
//	uint16_t bytes = ETH_HLEN + IP4_HDRLEN + UDP_HDRLEN + payloadBytes; /* # bytes sent */
//	dataBytes = (pt_p->stt_seq != multi_p->seq_max - 1) ? (multi_p->payload_size) : (multi_p->payload_size_last); /* # of bytes of the pure data */
//	payloadBytes = (pt_p->stt_seq != 0) ? (DATA_HDRLEN + dataBytes) : (DATA_HDRLEN + dataBytes + DATA_METADATALEN); /* # of bytes in the payload of a message */
//	payload_p = (uint8_t *) malloc(payloadBytes); /*create the msg header, payload, i.e.,(data header + data) */
// printf("MSG_PATCH_DATA: [%d]\n", pt_p->stt_seq);
//	create_data_from_file(payload_p, multi_p->xid, pt_p->stt_seq, dataBytes, multi_p->fd_patch, &(sender_p->tv_init), &(pt_p->tv_lossTiming),
////		&(pt_p->tv_patchReqSent), &(elem_p->tv_patchReqRecv));
//
//	if (pt_p->stt_seq == 0) { /* append the metadata to the 0 packet */
//		createDataMetadata(payload_p + payloadBytes - DATA_METADATALEN, multi_p->data_size);
//	}
	return payloadBytes;
}

databyte_t getFilePosition(seqnum_t cur, seqnum_t first, seqnum_t tail, databyte_t payload, databyte_t start) {
	if (cur >= first) {
		if (tail > 0 && cur > tail) {
			return (cur) * payload;
		} else {
			return (cur - first) * payload;
		}
	} else {
		return start + (cur) * payload;
	}
	return -1;
}

databyte_t extendFile(FILE ** f_pp, databyte_t extend_db) {
	databyte_t original_db = ftell(*f_pp);
	databyte_t current_db = original_db;

	while (current_db < original_db + extend_db) {
		current_db = lseek(fileno(*f_pp), original_db + extend_db, SEEK_SET);
		if (current_db < 0) {
			return -1;
		}
	}
	assert(fseek(*f_pp, original_db + extend_db, SEEK_SET) == 0);
	assert(ftell(*f_pp) == original_db + extend_db);

	return extend_db;

}

