/*
 *
 */

#include "msg.h"

void createBcdId(char *buf_p, const BcdID * bcd_id_p) {
	BcdID *p = (BcdID *) buf_p;
	p->app_id.msb = htobe64(bcd_id_p->app_id.msb);
	p->app_id.lsb = htobe64(bcd_id_p->app_id.lsb);
	p->data_id = htobe64(bcd_id_p->data_id);
	return;
}

void createMsgHdr(char *buf_p, const BcdID *bcd_id_p, const msg_type_t type, const bytes_t len) {
	MsgHdr *p = (MsgHdr *) buf_p;
	createBcdId((char *) &(p->bcd_id), bcd_id_p);
	p->type = htonl(type);
	p->len = htonl(len);
	gettimeofday(&(p->tv), NULL);
	return;
}

void createDataHdr(char *buf_p, const BcdID *bcd_id_p, const seqnum_t seq, const bytes_t len) { //, uint32_t f1, uint32_t f2, uint32_t f3, uint32_t f4, uint32_t f5, uint32_t f6) {
	DataHdr *p = (DataHdr *) buf_p;
	createBcdId((char *) &(p->bcd_id), bcd_id_p);
	p->seq_num = htonl(seq);
	p->len = htonl(len);
//	p->f1 = htonl(f1);
//	p->f2 = htonl(f2);
//	p->f3 = htonl(f3);
//	p->f4 = htonl(f4);
//	p->f5 = htonl(f5);
//	p->f6 = htonl(f6);
	return;
}

void parseBcdId(BcdID *bi_p, const char *buf_p) {
	BcdID *p = (BcdID *) buf_p;
	bi_p->app_id.msb = be64toh(p->app_id.msb);
	bi_p->app_id.lsb = be64toh(p->app_id.lsb);
	bi_p->data_id = be64toh(p->data_id);
	return;
}

void parseMsgHdr(MsgHdr *mh_p, const char *buf_p) {
	MsgHdr *p = (MsgHdr *) buf_p;
	parseBcdId(&(mh_p->bcd_id), (const char *) &(p->bcd_id));
	mh_p->type = ntohl(p->type);
	mh_p->len = ntohl(p->len);
	mh_p->tv = p->tv;
	return;
}

bytes_t createMsg(char *buf_p, const BcdID *bcd_id_p, const msg_type_t type, const char *msg_payload, const bytes_t tcp_payload_len, const bytes_t padding_len) {
	createMsgHdr(buf_p, bcd_id_p, type, tcp_payload_len + padding_len); /* assign the message header */
	if (msg_payload) { /* copy the payload */
		memcpy(buf_p + MSG_HDRLEN, msg_payload, tcp_payload_len - MSG_HDRLEN);
	}
	return tcp_payload_len;
}

void parseMsgSeqFirst(MsgSeqFirst *msf_p, const char *buf_p) {
	MsgSeqFirst *p = (MsgSeqFirst *) buf_p;
	msf_p->seq_first = ntohl(p->seq_first);
	return;
}

void parse_patch_req_msg(uint8_t *buf, PatchTuple *pt_p) {
	pt_p->stt_seq = ((PatchTuple *) buf)->stt_seq;
	pt_p->end_seq = ((PatchTuple *) buf)->end_seq;
	return;
}

bytes_t createDataFromFile(char * buf_p, const BcdID *bcd_id_p, const seqnum_t seq, const bytes_t len, FILE *fd) { //uint32_t f1, uint32_t f2, uint32_t f3, uint32_t f4, uint32_t f5, uint32_t f6) {
	createDataHdr(buf_p, bcd_id_p, seq, len); //, f1, f2, f3, f4, f5, f6);
	size_t ret = fread(buf_p + DATA_HDRLEN, sizeof(uint8_t), len, fd);
	if (ret == 0) {
		rewind(fd);
		fread(buf_p + DATA_HDRLEN, sizeof(uint8_t), len, fd);
	}
	return DATA_HDRLEN + len;
}

void parseMsgPatchReq(const char *buf, PatchTuple *pt_p) {
	pt_p->stt_seq = ((PatchTuple *) buf)->stt_seq;
	pt_p->end_seq = ((PatchTuple *) buf)->end_seq;
	return;
}

//void create_eth(struct ethhdr *ethhdr, char *nic_name) {
//	/*optical socket*/
//	int sd;
//	int i;
//
//	/*the ethernet header of the ethernet frame*/
//	struct timeval t11, t22, t33;
//	gettimeofday(&t11, NULL);
//	struct sockaddr_ll device;
//	struct ifreq ifr;
//	//  int frame_length = ETH_FRAME_LEN;
//
//	// Submit request for a socket descriptor to look up interface.
//	if ((sd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
//		perror("socket() failed to get socket descriptor for using ioctl() ");
//		exit(EXIT_FAILURE);
//	}
//
//	// Use ioctl() to look up interface name and get its MAC address.
//
//	// printf("nic_name: %s", nic_name);
//
//	memset(&ifr, 0, sizeof(ifr));
//	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", nic_name);
//	if (ioctl(sd, SIOCGIFHWADDR, &ifr) < 0) {
//		perror("ioctl() failed to get source MAC address ");
//		exit(EXIT_FAILURE);
//	}
//
//	// Copy source MAC address.
//	memcpy(ethhdr->h_source, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
//
//	// Report source MAC address to stdout.
//	printf("MAC address for interface %s is ", nic_name);
//	for (i = 0; i < 5; i++) {
//		printf("%02x:", ethhdr->h_source[i]);
//	}
//	printf("%02x\n", ethhdr->h_source[5]);
//
//	// Find interface index from interface name and store index in
//	// struct sockaddr_ll device, which will be used as an argument of sendto().
//
//	memset(&device, 0, sizeof(device));
//	if ((device.sll_ifindex = if_nametoindex(nic_name)) == 0) {
//		perror("if_nametoindex() failed to obtain interface index ");
//		exit(EXIT_FAILURE);
//	}
//
//	// printf("Index for interface %s is %i\n", nic_name, device.sll_ifindex);
//
//	// Report source MAC address to stdout.
//	for (i = 0; i < 6; i++) {
//		ethhdr->h_dest[i] = 0xff;
//	}
//
//	// ethernet protocol number
//	ethhdr->h_proto = htons(ETH_P_IP);
//
//	// Fill out sockaddr_ll.
//	device.sll_family = AF_PACKET;
//	memcpy(device.sll_addr, ethhdr->h_source, ETH_ALEN);
//	device.sll_halen = htons(ETH_ALEN);
//
//	// close the optical file descriptor
//	gettimeofday(&t22, NULL);
//	timersub(&t22, &t11, &t33);
//	printf("time is [%d]ms\n", (t33.tv_sec * 1000 * 1000 + t33.tv_usec) / 1000);
//	shutdown(sd, SHUT_RDWR);
//	gettimeofday(&t22, NULL);
//	timersub(&t22, &t11, &t33);
//	printf("time is [%d]ms\n", (t33.tv_sec * 1000 * 1000 + t33.tv_usec) / 1000);
//	return;
//}

// Computing the internet checksum (RFC 1071).
// Note that the internet checksum does not preclude collisions.
//uint16_t checksum(uint16_t *addr, int len) {
//	int count = len;
//	register uint32_t sum = 0;
//	uint16_t answer = 0;
//
//	// Sum up 2-byte values until none or only one byte left.
//	while (count > 1) {
//		sum += *(addr++);
//		count -= 2;
//	}
//
//	// Add left-over byte, if any.
//	if (count > 0) {
//		sum += *(uint8_t *) addr;
//	}
//
//	// Fold 32-bit sum into 16 bits; we lose information by doing this,
//	// increasing the chances of a collision.
//	// sum = (lower 16 bits) + (upper 16 bits shifted right 16 bits)
//	while (sum >> 16) {
//		sum = (sum & 0xffff) + (sum >> 16);
//	}
//
//	// Checksum is one's compliment of sum.
//	answer = ~sum;
//
//	return (answer);
//}
//
//void create_ip(struct iphdr *iphdr, char *src_ip, char *dst_ip, int datalen) {
//
//	// IPv4 header
//
//	// IPv4 header length (4 bits): Number of 32-bit words in header = 5
//	iphdr->ip_hl = IP4_HDRLEN / sizeof(uint32_t);
//
//	// Internet Protocol version (4 bits): IPv4
//	iphdr->ip_v = 4;
//
//	// Type of service (8 bits)
//	iphdr->ip_tos = 0;
//
//	// Total length of datagram (16 bits): IP header + UDP header + datalen
//	iphdr->ip_len = htons(IP4_HDRLEN + UDP_HDRLEN + datalen);
//
//	// ID sequence number (16 bits): unused, since single datagram
//	iphdr->ip_id = htons(0);
//
//	// Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram
//
//	int ip_flags[4];
//	// Zero (1 bit)
//	ip_flags[0] = 0;
//
//	// Do not fragment flag (1 bit)
//	ip_flags[1] = 0;
//
//	// More fragments following flag (1 bit)
//	ip_flags[2] = 0;
//
//	// Fragmentation offset (13 bits)
//	ip_flags[3] = 0;
//
//	iphdr->ip_off = htons((ip_flags[0] << 15) + (ip_flags[1] << 14) + (ip_flags[2] << 13) + ip_flags[3]);
//
//	// Time-to-Live (8 bits): default to maximum value
//	iphdr->ip_ttl = 255;
//
//	// Transport layer protocol (8 bits): 17 for UDP
//	iphdr->ip_p = IPPROTO_UDP;
//
//	int status;
//	// Source IPv4 address (32 bits)
//	if ((status = inet_pton(AF_INET, src_ip, &(iphdr->ip_src))) != 1) {
//		fprintf(stderr, "inet_pton() failed.\nError message: %s", strerror(status));
//		exit(EXIT_FAILURE);
//	}
//
//	// Destination IPv4 address (32 bits)
//	if ((status = inet_pton(AF_INET, dst_ip, &(iphdr->ip_dst))) != 1) {
//		fprintf(stderr, "inet_pton() failed.\nError message: %s", strerror(status));
//		exit(EXIT_FAILURE);
//	}
//
//	// IPv4 header checksum (16 bits): set to 0 when calculating checksum
//	iphdr->ip_sum = 0;
//	iphdr->ip_sum = checksum((uint16_t *) iphdr, IP4_HDRLEN);
//
//	return;
//
//}

//void create_udp(struct udphdr *udphdr, int source, int dest, int datalen) {
//	// UDP header
//
//	// Source port number (16 bits): pick a number
//	udphdr->source = htons(source);
//
//	// Destination port number (16 bits): pick a number
//	udphdr->dest = htons(dest);
//
//	// Length of UDP datagram (16 bits): UDP header + UDP data
//	udphdr->len = htons(UDP_HDRLEN + datalen);
//
//	// UDP checksum (16 bits)
//	udphdr->check = 0;
//
//	return;
//
//}

void parseDataHdr(DataHdr *dh_p, const char *buf_p) {
	DataHdr *p = (DataHdr *) buf_p;
	parseBcdId(&(dh_p->bcd_id), (const char *) &(p->bcd_id));
	dh_p->seq_num = ntohl(p->seq_num);
	dh_p->len = ntohl(p->len);
//	dh_p->f1 = ntohl(p->f1);
//	dh_p->f2 = ntohl(p->f2);
//	dh_p->f3 = ntohl(p->f3);
//	dh_p->f4 = ntohl(p->f4);
//	dh_p->f5 = ntohl(p->f5);
//	dh_p->f6 = ntohl(p->f6);
	return;
}

void initIPHeader(struct iphdr *ip_hdr, struct in_addr ip_src, u_int8_t ip_p) {
//	#if __BYTE_ORDER == __LITTLE_ENDIAN
//		unsigned int ip_hl:4;		/* header length */
//		unsigned int ip_v:4;		/* version */
//	#endif
//	#if __BYTE_ORDER == __BIG_ENDIAN
//		unsigned int ip_v:4;		/* version */
//		unsigned int ip_hl:4;		/* header length */
//	#endif
//		u_int8_t ip_tos;			/* type of service */
//		u_short ip_len;			/* total length */
//		u_short ip_id;			/* identification */
//		u_short ip_off;			/* fragment offset field */
//	#define	IP_RF 0x8000			/* reserved fragment flag */
//	#define	IP_DF 0x4000			/* dont fragment flag */
//	#define	IP_MF 0x2000			/* more fragments flag */
//	#define	IP_OFFMASK 0x1fff		/* mask for fragmenting bits */
//		u_int8_t ip_ttl;			/* time to live */
//		u_int8_t ip_p;			/* protocol */
//		u_short ip_sum;			/* checksum */
//		struct in_addr ip_src, ip_dst;	/* source and dest address */

// IPv4 header length (4 bits): Number of 32-bit words in header = 5
//	ip_hdr->ip_hl = IP4_HDRLEN / sizeof(uint32_t);
	ip_hdr->ihl = IP4_HDRLEN / sizeof(uint32_t);

	// Internet Protocol version (4 bits): IPv4
//	ip_hdr->ip_v = 4;
	ip_hdr->version = 4;

	// Type of service (8 bits)
//	ip_hdr->ip_tos = 0;
	ip_hdr->tos = 0;

	// Total length of datagram (16 bits): IP header + UDP header + datalen
//	ip_hdr->ip_len = htons(0); // filled later
	ip_hdr->tot_len = htons(0); // filled later

	// ID sequence number (16 bits): unused, since single datagram
//	ip_hdr->ip_id = htons(0);
	ip_hdr->id = htons(0);

	// Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram
	int ip_flags[4];
	// Zero (1 bit)
	ip_flags[0] = 0;

	// Do not fragment flag (1 bit)
	ip_flags[1] = 0;

	// More fragments following flag (1 bit)
	ip_flags[2] = 0;

	// Fragmentation offset (13 bits)
	ip_flags[3] = 0;

//	ip_hdr->ip_off = htons((ip_flags[0] << 15) + (ip_flags[1] << 14) + (ip_flags[2] << 13) + ip_flags[3]);
	ip_hdr->frag_off = htons((ip_flags[0] << 15) + (ip_flags[1] << 14) + (ip_flags[2] << 13) + ip_flags[3]);

	// Time-to-Live (8 bits): default to maximum value
//	ip_hdr->ip_ttl = 255;
	ip_hdr->ttl = 255;

	// Transport layer protocol (8 bits): 17 for UDP
	ip_hdr->protocol = ip_p;
//	ip_hdr->ip_p = ip_p;

	int status;
	// Source IPv4 address (32 bits)
//	ip_hdr->ip_src = ip_src;
	ip_hdr->saddr = ip_src.s_addr;

	// Create the multicast address for this machine
	char ip_dst[16];
	uint8_t ip_dst_suffix = (uint8_t) (((uint32_t) (ip_src.s_addr)) >> (3 * 8)); /* least digits already put at high bit */
	sprintf(ip_dst, "%s%d", MULTICAST_IP_PREFIX, ip_dst_suffix);
	// Destination IPv4 address (32 bits)
//	if ((status = inet_pton(AF_INET, ip_dst, &(ip_hdr->ip_dst))) != 1) {
	if ((status = inet_pton(AF_INET, ip_dst, &(ip_hdr->daddr))) != 1) {
		fprintf(stderr, "inet_pton() failed.\nError message: %s", strerror(status));
		exit(EXIT_FAILURE);
	}
	// printf("[debug] multicast address %s\n", ip_dst);

	// IPv4 header checksum (16 bits): set to 0 when calculating checksum
//	ip_hdr->ip_sum = 0; // filed later
	ip_hdr->check = 0; // filed later

	return;

}

//uint16_t computeIPchecksum(uint16_t *addr, int len) {
//	int count = len;
//	register uint32_t sum = 0;
//	uint16_t answer = 0;
//
//	// Sum up 2-byte values until none or only one byte left.
//	while (count > 1) {
//		sum += *(addr++);
//		count -= 2;
//	}
//
//	// Add left-over byte, if any.
//	if (count > 0) {
//		sum += *(uint8_t *) addr;
//	}
//
//	// Fold 32-bit sum into 16 bits; we lose information by doing this,
//	// increasing the chances of a collision.
//	// sum = (lower 16 bits) + (upper 16 bits shifted right 16 bits)
//	while (sum >> 16) {
//		sum = (sum & 0xffff) + (sum >> 16);
//	}
//
//	// Checksum is one's compliment of sum.
//	answer = ~sum;
//
//	return (answer);
//}

void initUDPHeader(struct udphdr *udphdr, int source, int dest) {
	// Source port number (16 bits): pick a number
	udphdr->source = htons(source);

	// Destination port number (16 bits): pick a number
	udphdr->dest = htons(dest);

	// Length of UDP datagram (16 bits): UDP header + UDP data
	udphdr->len = htons(0);

	// UDP checksum (16 bits)
	udphdr->check = 0;

	return;

}

void createBcdMetadata(char *buf_p, const databyte_t data_size, const char *app_id) {
	BcdMetadata *p = (BcdMetadata *) buf_p;
	p->data_size = htobe64(data_size);
	strncpy(p->app_id, app_id, APPID_LEN);
	return;
}

void parseBcdMetadata(BcdMetadata *bm_p, const char *buf_p) {
	BcdMetadata *p = (BcdMetadata *) buf_p;
	bm_p->data_size = be64toh(p->data_size);
	strncpy(bm_p->app_id, p->app_id, APPID_LEN);
	return;
}

void parseMsgIntQ(MsgIntQ *intQ_p, const char *buf_p) {
	MsgIntQ *p = (MsgIntQ *) buf_p;
	strncpy(intQ_p->master_addr, p->master_addr, IP_ADDR_LEN); // TODO: this is always IP A.B.C.D?
	intQ_p->executor_id = ntohl(p->executor_id);
	intQ_p->masterinaddr = *(struct in_addr *) (gethostbyname(intQ_p->master_addr)->h_addr_list[0]);
	strncpy(intQ_p->app_id, p->app_id, APPID_LEN);
}

void parseMsgWrtQ(MsgWrtQ *wrtQ_p, const char *buf_p) {
	MsgWrtQ *p = (MsgWrtQ *) buf_p;
	wrtQ_p->bcd_size = be64toh(p->bcd_size);
}

void parseMsgPshQ(MsgPshQ * pshQ_p, const char *buf_p) {
	MsgPshQ *p = (MsgPshQ *) buf_p;

	pshQ_p->fan_out = (int32_t) be64toh(*(int64_t * )&(p->fan_out)); /* UDS has int64_t long only */
	pshQ_p->fan_out_effective = 0;
	pshQ_p->slaves = (struct in_addr *) malloc(sizeof(struct in_addr) * pshQ_p->fan_out); /* flexible fields (slaves) */

	int i;
	struct hostent * he_p;
	char * hostname_p;
	for (i = 0; i < pshQ_p->fan_out; i++) {
		/* convert hostname to address */
		hostname_p = &(((char *) (&(p->slaves)))[i * IP_ADDR_LEN]);
		if ((he_p = gethostbyname(hostname_p)) == NULL) {
			zlog_error(zc, "gethostbyname() failed for hostname [%s]", hostname_p);
		}
		zlog_debug(zc, "hostname [%s] <-> address [%s]", hostname_p, inet_ntoa(*(struct in_addr * ) (he_p->h_addr_list[0])));

		/* add to struct if not the node having master */
		if (((struct in_addr *) (he_p->h_addr_list[0]))->s_addr == hostinaddress_ctrl.s_addr) {
			zlog_debug(zc, "local address, skipped");
		} else {
			pshQ_p->slaves[pshQ_p->fan_out_effective] = *(struct in_addr *) (he_p->h_addr_list[0]);
			pshQ_p->fan_out_effective++;
			zlog_debug(zc, "remove address, added");
		}
	}

	zlog_debug(zc, "number of slaves [%ld]: {", pshQ_p->fan_out_effective);
	for (i = 0; i < pshQ_p->fan_out_effective; i++) {
		zlog_debug(zc, "[%s]", inet_ntoa(pshQ_p->slaves[i]));
	}
	zlog_debug(zc, "}");
}

void BcdIdtoFilename(const BcdID *bcd_id_p, char *buf_p) {
	sprintf(buf_p, "%sbroadcast-%08lx-%04lx-%04lx-%04lx-%012lx/broadcast_%ld", BCD_DIR_PREFIX, ((bcd_id_p->app_id.msb) & (0xffffffff00000000)) >> 32, ((bcd_id_p->app_id.msb) & (0x00000000ffff0000)) >> 16,
		(bcd_id_p->app_id.msb) & (0x000000000000ffff), ((bcd_id_p->app_id.lsb) & (0xffff000000000000)) >> 48, (bcd_id_p->app_id.lsb) & (0x0000ffffffffffff), bcd_id_p->data_id);
	return;
}

void BcdId2str(const BcdID *bcd_id_p, char *buf_p) {
	sprintf(buf_p, "bcdid[appid[msb[%016lx]lsb[%016lx]]dataid[%ld]]", bcd_id_p->app_id.msb, bcd_id_p->app_id.lsb, bcd_id_p->data_id);
	return;
}

void AppIdtoDir(const AppID *app_id_p, char *buf_p) {
	sprintf(buf_p, "%sbroadcast-%08lx-%04lx-%04lx-%04lx-%012lx/", BCD_DIR_PREFIX, ((app_id_p->msb) & (0xffffffff00000000)) >> 32, ((app_id_p->msb) & (0x00000000ffff0000)) >> 16, (app_id_p->msb) & (0x000000000000ffff),
		((app_id_p->lsb) & (0xffff000000000000)) >> 48, (app_id_p->lsb) & (0x0000ffffffffffff));
}

void createDir(char *dir_str) {
	struct stat st;
	if (stat(dir_str, &st) == 0) {
		zlog_warn(zc, "directory %s exist", dir_str);
	} else {
		zlog_debug(zc, "directory %s does not exist", dir_str);
		if (mkdir(dir_str, 0755) == 0) {
			zlog_debug(zc, "mkdir successes");
		} else {
			zlog_fatal(zc, "mkdir fails");
		}
	}
}

void deleteDir(char *dir_str) {
	char buf[256];
	struct stat st;
	if (stat(dir_str, &st) == 0) {
		zlog_debug(zc, "directory %s exist", dir_str);
		sprintf(buf, "exec rm -rf %s", dir_str);
		system(buf);
	} else {
		zlog_warn(zc, "directory %s does not exist", dir_str);
	}
}

uint8_t existAppDir(AppID * app_id_p) {
	char bcd_dir[FILENAME_LEN];
	AppIdtoDir(app_id_p, bcd_dir);
	struct stat st;
	return stat(bcd_dir, &st) == 0 ? 0x0f : 0;
}
