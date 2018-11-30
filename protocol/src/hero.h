/*
 *
 */

#ifndef HERO_H_
#define HERO_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <strings.h>          // bzero()
#include <math.h>			  // ceil()

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
#include <sys/time.h>
#include <time.h>
#include <errno.h>            // errno, perror()
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <assert.h>

#include <zlog.h>

#include <apr_ring.h>
#include <apr_hash.h>
#include <apr_queue.h>

#ifndef NETMAP_WITH_LIBS
#define NETMAP_WITH_LIBS
#endif
#include "net/netmap_user.h"
#include "net/netmap.h"

#include "global.h"

#include "netmap_util.h"
#include "msg.h"
#include "dll.h"

#define APR_QUEUE_CAPACITY	(200)
#define SEQ2WARDTHEEND		(SEQNUM_MAX)

typedef enum _fdType {
	IFACE_UDS, CTRL_RECV_TCP, //TODO RECV & SEND can be merged
	CTRL_SEND_TCP,
	PIPE_N2C_RECV, // TODO *_PIPE can be merged
	PIPE_I2C_RECV, // TODO *_PIPE can be merged
	PIPE_C2I_UDS,
	PIPE_C_SEND2I_UDS
} FdType;

typedef struct _msgToken {
	char *buf_p; // pointer to the content of the message. malloc() when MsgToken is created, free() when MsgToken is deleted
	bytes_t msg_len; // length of the message body
	bytes_t padding_len; // length of padding in this message
	bytes_t byte_sent; // # bytes has been sent
	struct timeval tv; // timeval when MsgToken is created (stats only)
} MsgToken;

typedef struct _fdState {
	int fd;
	FdType fd_type;
	void *state_p;
	uint8_t is_trashed; // if this FdState has been trashed
	struct timeval tv_trashed; // when th FdState was trashed

	char recv_buf[RECVBUF_LEN]; /* recv */
	bytes_t recv_ed_byte;
	bytes_t recv_expect_byte;
	MsgHdr mh; /* msg header used to parse recved message */

	apr_pool_t *pool_p;
	apr_queue_t *sendQueue_p; /* send */
	MsgToken *remainMsgToken_p;
} FdState;

typedef struct _cmd_elem_t {
	APR_RING_ENTRY (_cmd_elem_t) /* required fields node and link */
	link;
	/* various values depending on your program */
	BcdID bcd_id;
	apr_queue_t * sendQueue_p;
	MsgToken * mt_p;
} cmd_elem_t;
APR_RING_HEAD(_cmd_ring_t, _cmd_elem_t);
typedef struct _cmd_ring_t cmd_ring_t;

// TODO: merge FileMetadata and ReplyMsg
typedef struct _fileMetadata { /* metadata of the file: file read sequence [start, jump) -> [0, start) -> [jump, end] */
	databyte_t start; /* if the path data has been wrap around, (-1 means not...)*/
	databyte_t jump;
} FileMetadata;

typedef struct _appState {
	AppID app_id;
	char app_id_str[APPID_LEN];
	char bcd_dir_str[FILENAME_LEN];
	char master_addr[IP_ADDR_LEN];
	char stats_filename[FILENAME_LEN];
	FILE *fd_stats_p;
//	List *uds_list; /* list of Unix Domain Socket associate to this application */
	apr_hash_t *uds_hash;
	apr_hash_t *bcd_sender_hash;
} AppState;

typedef struct _localSocketState {
	struct sockaddr_un client_addr;
	int id; /* id of local socket state */

	MsgIntQ msgIntQ;
	AppState *app_p; /* app state the local socket associates to */

	// TODO: a local socket might have multiple recver, now we just assume one
	BcdID bcd_id; /* this is the bcd_id currently request for read */
	uint8_t reading;
} LocalSocketState;

typedef struct _recvSocketState { /* the states associated to a recv socket */
	struct sockaddr_in server_addr;
	char nic_name[16];
	char ip_address[24];
} RecvSocketState;

typedef struct _recverState {/* the state variable related to the receiver machine */
	/* lock between thread */
	uint8_t lock_processingFastData;	// this will be set as 1 when data from the fast path is being processed
	uint8_t is_stats_logged;

	BcdID bcd_id; /* key */
	char app_id_str[APPID_LEN];

	/* seq state */
	uint8_t is_seq_fst;	// if the first seq has been received
	seqnum_t seq_fst;		// the first seq received from fast path
	seqnum_t seq_lst;		// the packet received in the last round, temporary state...
	seqnum_t seq_tail;		// this is the seq received from fast path right before the seq wrap around
//	DataHdr dh_lst;

	/* updated by the zero packet */
	uint8_t is_seq_zero; /* the following vars in this block become available only after seq_zero is set to 1; */
	databyte_t data_size;		// number of bytes in the data
	seqnum_t seq_max;		// number of data packets in the data
	uint16_t byte_payload;	// number of bytes in all the packet except for the last one
	uint16_t byte_payload_min;	// number of bytes in the last one
	char *tail_patch_buffer;
	databyte_t tail_patch_buffer_len;

	/* file descripter for writing the files */
	FILE * fd_p;				// file descriptor for writing from the fast path
	FILE * fd_patch_p;		// file descriptor for writing from the slow path

	/* packet counter */
	pktnum_t num_fast_recv_ed;
	pktnum_t num_slow_recv_ed;

	/* recver state */
	uint8_t is_fastpath; /* can the fast path receive data */
	uint8_t is_finished;		// the status of the receiving: 0->the file has not completely received..
	FileMetadata fileMetadata;

	FdState * fds_p;

	struct timeval tv_start_fastpath;	// the time when the receiver got the first data
	struct timeval tv_finish_fastpath;	// the time when the receiver got the last data (might never got the last data)
	struct timeval tv_finish;			// the time when the receiver finish the receiving

	char bcd_filename[FILENAME_LEN];

	uint16_t delete_counter;
} RecverState;

// the struct stores all the nic name and its ethernet frame headers
struct _nicName2ethHdr {
	char nicName[IF_NAMESIZE];	// hard coded length
	struct ethhdr ethHdr;		// ethernet header of the nic including the mac address of the interface
	struct in_addr ipAddress;
};

extern uint8_t need_release_path;
extern struct netmap_ring * am_txring_p;
extern uint32_t am_txring_head;
extern uint32_t am_txring_tail_last;

extern uint8_t nicNum;
extern struct _nicName2ethHdr nicName2ethHdr[10];

typedef struct _netmap_arg {
	struct targ t;
	uint8_t retval;
} NetmapArg;

typedef struct _domain_socket_arg {
	uint8_t retval;
} DomainSocketArg;

typedef struct _recv_socket_arg {
	uint8_t retval;
} RecvSocketArg;

typedef struct _send_socket_arg {
	uint8_t retval;
} SendSocketArg;

//TODO: only the host name of the data and ctrl is needed. given the interface name, get the eth and ip hostname
uint8_t setHostChannel(const char * iface_name, char * hostname_p, struct in_addr * hostinaddr_p, struct ethhdr *hostethhdr_p);
void createNicNameMap();

PatchTuple * createPatchTuple(seqnum_t seqStart, seqnum_t seqEnd);
RecverState * createRecverState(BcdID *);
AppState *createAppState(AppID *app_id_p, char* app_id_str_p, char* master_addr_p);

FdState * createFdState(int fd, FdType fdt, void* state_p, apr_pool_t * pool_p);
FdState * createRecvSocketState(char *ip_address, char *nic_name);
FdState * createLocalSocketState(int socket, struct sockaddr_un addr, int id);

void deleteSendQueue(apr_queue_t * sendQueue_p);

void deletePatchTuple(PatchTuple *pt_p);
void deleteMsgToken(MsgToken *mt_p);

/*
 * This program has a connection with each executor (executor in Spark).
 * Domain Socket State is created when
 * 1. Executor starts (create connection)
 * Domain Socket State is deleted when
 * 1. Executor finishes (terminates the connection)
 */
void deleteUDSState(LocalSocketState *lsock_p);

/*
 * Receiver State is for each broadcast data
 * Receiver State is created when
 * 1. MSG_READ_REQ is received
 * Receiver State is deleted when
 * 1. MSG_DEL_REQ is received
 */
void deleteRecverState(RecverState *recv_p);
void deleteAppState(AppState * app_p);
/*
 * Recv Socket State is for the connection between slave and master
 *
 */
void deleteRecvSocketState(RecvSocketState *rsock_p);

databyte_t recverState_updateBcdMetadata(RecverState *recv_p, BcdMetadata *bm_p);
int8_t recverState_hasGottenAllData(RecverState *recv_p);
void recverState_finishRecv(RecverState *recv_p);
void recverState_stats(RecverState * recv_p, FILE* fd_stats_p);
void recverState_stopFathPath(RecverState *recv_p);
void recverState_startFathPath(RecverState *recv_p);

void recv_state_recordLossSize(RecverState *state_p, int16_t size);
void recv_state_recordLossTiming(RecverState *state_p, seqnum_t seq, DataHdr *dh);

uint8_t insertMsg2SendQ(apr_queue_t *sendQueue_p, BcdID *bcd_id_p, msg_type_t type, char *msg_payload, bytes_t msg_payload_len, uint8_t padding, uint8_t priority);
cmd_elem_t * createCmd(apr_queue_t *sendQueue_p, BcdID *bcd_id_p, msg_type_t type, char *msg_payload, bytes_t msg_payload_len, uint8_t padding);

//struct ethhdr * getEthHdr(const char * nicName);
//struct in_addr getIPAddress(const char * nicName);

void deleteRamdiskFiles();

int sendMsg(int socket, apr_queue_t * queue_p, MsgToken ** msgTuple_pp);

void user1_handler(int sig);
void removeData(char * filename);
void closeFile(FILE ** fd_pp);
void initSharedDataStructures();

typedef struct _my_elem_t {
	/* 'link' is just a name of member variable, as which you can choose any name.
	 * But, you have to care of its name in the following code like a type name.
	 * This seems weird, but don't care so much. */
	APR_RING_ENTRY(_my_elem_t)
	link;

	/* various values depending on your program */
	BcdID bcd_id; /* xid of the tuple, since a sendSocket might linked to multiple senders with difference xid */
	PatchTuple pt;
	struct timeval tv_init;
	struct timeval tv_patchReqRecv;

} my_elem_t;
APR_RING_HEAD(_my_ring_t, _my_elem_t);
typedef struct _my_ring_t my_ring_t;

typedef struct _sendSocketState {
	int socket;
	struct sockaddr_in client_addr;

	char recv_buf[RECVBUF_LEN];
	int recv_ed_byte;
	int recv_expect_byte;
	MsgHdr mh;

	apr_pool_t *pool_ssock_p;
	apr_queue_t *sendQueue_p; /* send */
	MsgToken *remainMsgToken_p;

	my_ring_t *ringPatchTuple;
} SendSocketState;

typedef struct _senderState {
	struct in_addr skey;
	BcdID bcd_id;

	int id;
	MsgSeqFirst msf;
	seqnum_t seqEnd; /* the estimated (might have multiple around...) largest seq send to the receiver */
//	uint8_t has_first_seq;
//	uint8_t has_fast_done;
	uint8_t has_all_done;
	uint8_t is_distinct_rack;

	SendSocketState *ssock_p;

	struct timeval tv_init;
	struct timeval tv_fast_done;

} SenderState;

typedef enum _RateType {
	RATE_UNKNOWN, RATE_GBPS, RATE_MPPS
} RateType;

typedef enum _BcdState {
	BCDSTATE_DEAD, BCDSTATE_INITIATED, BCDSTATE_ATTEMPT, BCDSTATE_FULLRATE
} BcdState;

typedef struct _multicastState {
	BcdID bcd_id; /* key of the multicast state */
	apr_pool_t * pool_ssock_p;
	apr_skiplist * sender_end_seq_pq_p;
	SenderState ** sender_end_seq_arr_p;
	int16_t sender_end_seq_arr_idx_start;
	int16_t sender_end_seq_arr_idx_end;
	apr_hash_t * sender_addr_hash_p;

	MsgPshQ msgPshQ;
	MsgIntQ msgIntQ;
	MsgWrtQ msgWrtQ;

	databyte_t data_size; /* number of bytes in the data */
	seqnum_t seq_max; /* number of packets for entire data */
	bytes_t byte_payload; /* number of bytes of data in each data packet */
	bytes_t byte_payload_min; /* number of bytes in the last data packet */

	char bcd_filename[FILENAME_LEN]; /* file name */
	FILE *fd; /* file descriptor for reading the data */
	FILE *fd_patch;

	BcdState bcd_state; /* state of the optical data transfer */

	uint16_t n_init; /* number of receivers having been initialized, this is used as sender id when create senderState */
	uint16_t n_first_seq; /* number of first_seq messages having been received, this is used to judge if seq_end should be updated */
	uint16_t n_all_done; /* number of all_done having been received */
	uint8_t is_first_seq_distinct_rack; /* if received FIRST_SEQ coming from a receiver from different racks */
	uint8_t is_all_done_distinct_rack; /* if received FIRST_SEQ coming from a receiver from different racks */
	uint8_t is_distinct_rack;
	uint8_t is_tv_last_data; // if the last data seq has been put in the buffer

	seqnum_t seq_end; /* the last seq the sender should send, initialized to a large number, updated when the last FIRST_SEQ is received*/
	seqnum_t seq_end_max; /* the max among the FISR_SEQed receivers want to receive */
	seqnum_t seq_cur; /* current sequence */

	databyte_t n_byte;			// number of bytes sent since last rate rectification
	pktnum_t n_packet;			// number of packets sent since last rate rectification

	databyte_t n_byte_total;	// number of bytes sent
	pktnum_t n_packet_total;	// number of packets sent

	databyte_t n_byte_attempt;	// number of bytes sent during attempt state
	pktnum_t n_packet_attempt;	// number of packets sent during attempt state

	pktnum_t n_patch_total;

	struct ethhdr *eth_hdr_p; /* data packet headers */
	struct iphdr ip_hdr;
	struct udphdr udp_hdr;

	RateType rate_type;		// rate limit type Gbps or Mpps
	double rate;			// rate limit value
	double rate_f_delta;	// number of u-sec for sending a byte
	double std_rate_f_delta;	// original rate_f_delta

	/* variable time stamp */
	struct timeval tv_sendStart; // the time the data transfer starts
	struct timeval tv_push; /* push received/next time to request if postponed */

	/* stats time stamp */
	struct timeval tv_path_request; /* multicast path requested */
	struct timeval tv_path_approve; /* multicast path approved */
	struct timeval tv_path_release; /* multicast path released */

	struct timeval tv_first_data; /* first data seq is sent */
	struct timeval tv_last_data; /* last data seq is sent */
	struct timeval tv_attempt_finish;

	struct timeval tv_first_first_seq; /* first FIRST_SEQ received by the sender */
	struct timeval tv_first_first_seq_distinct_rack; /* first FIRST_SEQ from distinct rack received by the sender */
	struct timeval tv_last_first_seq; /* last FIRST_SEQ received by the sender */

	struct timeval tv_first_all_done;
	struct timeval tv_first_all_done_distinct_rack; /* first FIRST_SEQ from distinct rack received by the sender */ // TODO
	struct timeval tv_last_all_done;

//	uint8_t is_path_requested; // if the path is requested
	uint8_t is_path_released; // if the path is released
	uint8_t is_stats_logged;

	gint32 controller_xid;

	uint16_t delete_counter;
} MulticastState;

extern MulticastState *activeMulticast_p; /* pointer to current active MulticastState */

extern RateType rate_type;
extern double rate_f;
extern int lst_move;

void processPatchTuple(SendSocketState *ssock_p);

SendSocketState *createSendSocketState(int sock, struct sockaddr_in addr);
SenderState *createSenderState(int id, const struct in_addr *sk_p, BcdID *bcd_id_p, SendSocketState *ssock_p);
MulticastState *createBcdState(BcdID *bcd_id_p, MsgIntQ * msgIntQ_p, MsgWrtQ * msgWrtQ_p, uint8_t rate_type, double rate_f, char *nic_name);
my_elem_t *createPatchTupleRingElement(BcdID *, PatchTuple *pt, struct timeval *tv_init);
databyte_t getFilePosition(seqnum_t cur, seqnum_t first, seqnum_t tail, databyte_t payload, databyte_t start);
databyte_t extendFile(FILE ** f_pp, databyte_t extend_db);

void trashFdState(FdState *fds_p);
void deleteFdState(FdState *fds_p);
void deleteSendSocketState(SendSocketState * ssock_p);
void deleteSenderState(SenderState * send_p);
void deleteMulticastState(MulticastState * multi_p);

void multicastState_stats(MulticastState * multi_p, FILE * fd_stats_p);
void multicastState_updateRate(MulticastState * multi_p, double new_rate_f_delta);
void multicastState_reset(MulticastState * multi_p);
void multicastState_updateSeqEnd(MulticastState * multi_p, SenderState * sender_p);
bytes_t createData(char *payload_p, MulticastState *multi_p, FILE* fd, seqnum_t seq); //, uint32_t f1, uint32_t f2, uint32_t f3, uint32_t f4, uint32_t f5, uint32_t f6);
int pushcmp(void *a, void *b);

#endif /* HERO_H_ */
