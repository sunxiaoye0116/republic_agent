/*
 *
 */

#ifndef MSG_H_
#define MSG_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <netdb.h>            // struct addrinfo
#include <sys/types.h>        // needed for socket(), uint8_t, uint16_t, uint32_t
#include <sys/socket.h>       // needed for socket()
#include <netinet/in.h>       // IPPROTO_UDP, INET_ADDRSTRLEN
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/udp.h>      // struct udphdr
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/ioctl.h>        // macro ioctl is defined
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl.
#include <net/if.h>           // struct ifreq
#include <linux/if_ether.h>   // ETH_P_IP = 0x0800, ETH_P_IPV6 = 0x86DD
#include <linux/if_packet.h>  // struct sockaddr_ll (see man 7 packet)
#include <net/ethernet.h>
#include <fcntl.h>
#include <sys/stat.h>

#define _BSD_SOURCE
#include <endian.h>
#include <sys/time.h>
#include <time.h>

#include <zlog.h>
#include "global.h"

#define IP4_HDRLEN		20  // IPv4 header length
#define IP4_HDRLEN_MAX	60  // IPv4 header length
#define TCP_HDRLEN		20  // TCP header length
#define TCP_HDRLEN_MAX	60  // TCP header length
#define UDP_HDRLEN		8  // UDP header length, excludes data
#define UDP_HDRLEN_MAX	8  // UDP header length, excludes data

//#define LEN_MSG_INIT		0
//#define LEN_MSG_SEQ			(sizeof(struct first_seq_msg))
#define BCDMETADATA_LEN		(sizeof(BcdMetadata))

#define MSG_HDRLEN			(sizeof(MsgHdr))
#define DATA_HDRLEN			(sizeof(DataHdr))
#define DATA_HDR_OFFSET		(ETH_HLEN + IP4_HDRLEN + UDP_HDRLEN)
#define DATA_PAYLOAD_OFFSET	(ETH_HLEN + IP4_HDRLEN + UDP_HDRLEN + DATA_HDRLEN)

typedef struct _appID {
	app_id_sb_t msb;
	app_id_sb_t lsb;
} AppID;

typedef struct _bcdID {
	AppID app_id;
	data_id_t data_id;
} BcdID;

typedef struct _dataHdr {
	BcdID bcd_id;
	seqnum_t seq_num;
	bytes_t len;
//	uint32_t f1;
//	uint32_t f2;
//	uint32_t f3;
//	uint32_t f4;
//	uint32_t f5;
//	uint32_t f6;
} DataHdr;

typedef struct _msgHdr {
	BcdID bcd_id;
	msg_type_t type;
	bytes_t len;
	struct timeval tv;
} MsgHdr;

typedef struct _msgIntQ {
	char master_addr[IP_ADDR_LEN];
	int32_t executor_id;
	struct in_addr masterinaddr; /* used by agent only to check if master runs on the slave node */
	char app_id[APPID_LEN];
} MsgIntQ;

typedef struct _msgPshQ {
	int32_t fan_out; /* number of receiver nodes of the push */
	int32_t fan_out_effective; /* number of effective receiver nodes of the push, excluding the receiver on the same node of the sender */
	struct in_addr *slaves;
} MsgPshQ;

typedef struct _msgWrtQ {
	long bcd_size;
} MsgWrtQ;

//typedef struct _msgPullI2C {
//	void * master_fds_p;
//} MsgPullI2C;

void parseDataHdr(DataHdr *dh_p, const char *buf_p);
void parseMsgHdr(MsgHdr *mh_p, const char *buf_p);
void parseMsgIntQ(MsgIntQ *, const char *);
void parseMsgPshQ(MsgPshQ *, const char *);
void parseMsgWrtQ(MsgWrtQ *, const char *);

void AppIdtoDir(const AppID *app_id_p, char *buf_p);
void BcdIdtoFilename(const BcdID *, char *);
void BcdId2str(const BcdID *, char *);

void createDir(char *);
void deleteDir(char *);
uint8_t existAppDir(AppID *);

typedef struct _patchTuple {
	//	packets between the start (included) and end (included) sequence should be patched by a multicast sender
	seqnum_t stt_seq;
	seqnum_t end_seq;
} PatchTuple;

typedef struct _msgSeqFirst { /*first seq msg*/
	seqnum_t seq_first;
} MsgSeqFirst;

typedef struct _msgReadRpl { /* reply to the local socket */
	uint32_t code; // the status of the write request
	uint32_t padding;
	databyte_t start;     // the byte pointer of the first byte in the received file
	databyte_t jump;      //
	databyte_t file_size;
} MsgReadRpl;

typedef struct _msgDelRpl { /* reply to the local socket */
	uint32_t code; // the status of the write request
} MsgDelRpl;

typedef struct _msgIntRpl { /* reply to the local socket */
	uint32_t code; // the status of the write request
} MsgIntRpl;

typedef struct _msgPshRpl { /* reply to the local socket */
	uint32_t code; // the status of the push request
} MsgPshRpl;

typedef struct _msgWrtRpl { /* reply to the local socket */
	uint32_t code; // the status of the write request
} MsgWrtRpl;

/* message format used by the data packet */
typedef struct _BcdMetadata {
	databyte_t data_size;
	char app_id[APPID_LEN];
} BcdMetadata;

bytes_t createMsg(char *buf_p, const BcdID *bcd_id_p, const msg_type_t type, const char *msg_payload, const bytes_t tcp_payload_len, const bytes_t padding_len);
/*
 * sender function
 * */
bytes_t createDataFromFile(char * buf_p, const BcdID *bcd_id_p, const seqnum_t seq, const bytes_t len, FILE *fd); // uint32_t f1, uint32_t f2, uint32_t f3, uint32_t f4, uint32_t f5, uint32_t f6
//uint32_t create_data_from_file(char *buf, uint32_t xid, uint32_t seq, uint32_t data_len, FILE *filedes, struct timeval *tv_init, struct timeval *tv_lossTiming,
//	struct timeval * tv_patchReqSent, struct timeval *tv_patchReqRecv);
//void create_eth(struct ethhdr *ethhdr, char *nic_name);
//void create_ip(struct iphdr *iphdr, char *src_ip, char *dst_ip, int datalen);
//void create_udp(struct udphdr *udphdr, int source, int dest, int datalen);

void createBcdMetadata(char *buf_p, const databyte_t data_size, const char *app_id);
void parseBcdMetadata(BcdMetadata *bm_p, const char *buf_p);
void parseMsgSeqFirst(MsgSeqFirst *msf_p, const char *buf_p);

void parseMsgPatchReq(const char *buf, PatchTuple *pt_p);

void initIPHeader(struct iphdr *ip_hdr, struct in_addr ip_src, u_int8_t ip_p);
void initUDPHeader(struct udphdr *udphdr, int source, int dest);
//uint16_t computeIPchecksum(uint16_t *addr, int len);

#endif /* MSG_H_ */
