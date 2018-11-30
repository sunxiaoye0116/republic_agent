/*
 * global.h
 *
 *  Created on: Nov 16, 2015
 *      Author: xs6
 */

#ifndef SRC_GLOBAL_H_
#define SRC_GLOBAL_H_

#include <apr_hash.h>
#include <apr_queue.h>
#include <apr_skiplist.h>
#include <zlog.h>

#ifndef NETMAP_WITH_LIBS
#define NETMAP_WITH_LIBS
#endif

#include <linux/if_ether.h>
#include <netinet/in.h>
#include "net/netmap_user.h"
#include "net/netmap.h"

#include <thrift/c_glib/protocol/thrift_binary_protocol.h>
#include <thrift/c_glib/transport/thrift_buffered_transport.h>
#include <thrift/c_glib/transport/thrift_socket.h>
#include "gen-c_glib/edu_rice_bold_service_bcd_service.h"

#include "dll.h"

/* primitive types */
typedef uint32_t seqnum_t;
typedef int64_t databyte_t;
typedef uint64_t app_id_sb_t;
typedef uint64_t data_id_t;
typedef uint32_t msg_type_t;
typedef uint32_t bytes_t;
typedef int32_t pktnum_t;

extern zlog_category_t *zc;

extern apr_pool_t *pool_NetmapS_p;
extern apr_pool_t *pool_HashMulticast_S_p;
// extern apr_pool_t *pool_HashSender_S_p;
extern apr_pool_t *pool_ssock_p;

/* recever pool */
extern apr_pool_t *pool_HashRecver_L_p; /* hashmap: key: [xid], value: */
extern apr_pool_t *pool_lsock_p;
extern apr_pool_t *pool_rsock_p;

/* recv & send shared */
extern pthread_t pthread_NetmapR;
extern pthread_t pthread_LocalSocket;

/* receiver threads */
extern pthread_t pthread_RecvSocket;

/* sender threads */
extern pthread_t pthread_SendSocket;

/* recv & send shared */
extern List *lsockList; /* state of an executor socket */
extern apr_hash_t *hashApp;

/* receiver */
extern apr_hash_t *hashRecver; /* HashMap: key: [BcdID], value: RecverState */
extern List *rsockList; /* state of recv socket */

/* sender */
extern apr_hash_t *hashMulticast; /* HashMap: key: [BcdID], value: SenderState */
extern List *ssockList; /* the state of send socket */
extern apr_skiplist *queuePush_p; /* priority of waiting push multicast */

/* thrift */
extern ThriftSocket *thrift_socket_p;
extern ThriftTransport *thrift_transport_p;
extern ThriftProtocol *thrift_protocol_p;
extern edu_rice_bold_serviceBcdServiceIf *thrift_client_p;
extern GError *error_p;

extern struct nmreq curr_nmr;
extern char nmr_name[64];
extern int do_abort;
extern int localSocketID;
extern uint8_t zeros[9216];

extern char n2c_buf[512];
extern char c2i_buf[512];
extern char c_send2i_buf[512];
extern char i2c_buf[512];

extern char *data_nic_name;
extern char *ctrl_nic_name;
extern uint8_t cpu_index;
extern uint8_t queue_index;
extern uint8_t seq_zero_notify;
extern uint8_t msg_padding;
extern double attempt_scale;
extern bytes_t data_payload_len;
extern uint8_t connect_controller;
extern uint8_t first_first_seq_fullrate;
extern pktnum_t nm_burst;

extern int pipe_fd_n2c[2];
extern int pipe_fd_c2i[2];
extern int pipe_fd_c_send2i[2];
extern int pipe_fd_i2c_recv[2];

extern int32_t data_channel_scheduling_policy;
extern int32_t data_channel_scheduling_priority;
extern uint32_t data_channel_affinity;
extern uint32_t ctrl_channel_affinity;
extern uint32_t api_channel_affinity;
extern uint32_t num_cores;

#define CTRL_DST_PORT		(0x6464)    /* Reverse Addr Res packet  */
#define DATA_SRC_PORT		(0x6363)
#define DATA_DST_PORT		(0x6262)

/* define for netmap only */
#define SEQNUM_MAX				(INT32_MAX-100)
#define BYTE_MAX				(INT32_MAX-1000)
#define RECVBUF_LEN				(16384)
#define DEFAULT_BURST			(256)
#define MULTICAST_IP_PREFIX		"238.238.238."
#define DEFAULT_CTRL_NIC_NAME	"eth2"
#define DEFAULT_DATA_NIC_NAME	"eth2"
#define DEFAULT_ATTEMPT_SCALE	(0.01)
#define DEFAULT_DATA_PAYLOAD_LEN	(8692) //(8820)
#define INTERFACE_MTU			(8946)
#define DEFAULT_DATA_CHANNEL_SCHEDULING_POLICY (SCHED_OTHER)
#define DEFAULT_DATA_CHANNEL_SCHEDULING_PRIORITY (0)
#define DEFAULT_DATA_CHANNEL_AFFINITY	(1)
#define DEFAULT_CTRL_CHANNEL_AFFINITY	(0)
#define DEFAULT_API_CHANNEL_AFFINITY	(0)
#define DEFAULT_NETMAP_QUEUE_IDX (1)
#define TIME_BUF_LEN			(32)
#define HOSTNAME_LEN			(512)
#define IP_OFFSET				(111)
#define NUM_SERVER_PER_RACK		(2)

#define FDSTATE_DELETE_TIMEOUT_MSEC	(10*1000)
#define SELECT_TIMEOUT_SEC		(0)
#define SELECT_TIMEOUT_USEC		(20*1000)
#define POLL_TIMEOUT_MSEC		(15)
#define FAST_DONE_TIMEOUT_MSEC	(1000)
#define LOG_DIRECTORY			"./log/%s.log"

#define MSG_NONTYPE				(44)

/* message between threads */
#define MSG_N2C_SEQ_FIRST		(60) // get the first packet of the data
#define MSG_N2C_SEQ_ZERO		(61) // get the seq zero of the data
#define MSG_N2C_SEQ_JUMP		(62) // get jump packet of the data
#define MSG_N2C_SEQ_ALL			(64) // get all the seq of the data

#define MSG_I2C_PULL			(53) // get jump packet of the data

#define MSG_C2I_SEQ_ZERO		(71) // get the seq zero of the data
#define MSG_C2I_PULL_REJECT		(72) // get all the seq of the data
#define MSG_C2I_SEQ_ALL			(77) // get all the seq of the data

#define MSG_C_SEND2I_SEQ_ALL	(47)

/* control messages between agents */
#define MSG_FIRST_SEQ 			(81)
#define MSG_PULL_REJECT			(82)
#define MSG_PULL				(83)
#define MSG_PATCH_REQ 			(84)
#define MSG_PATCH_DATA 			(85)
#define MSG_FAST_DONE 			(86)
#define MSG_ALL_DONE 			(87)

/* command define between netmap and spark interface */
#define DOMAIN_SOCKET_FILENAME	"/home/bold/domain_socket_file"
#define MASTER_EXECUTOR_ID		(-1)
#define BCD_DIR_PREFIX			"/mnt/ramdisk/"
#define FILENAME_LEN			(128)
#define IP_ADDR_LEN				(16)
#define APPID_LEN				(64)

/* message between UNIX domain socket and agent*/
#define MSG_INT_REQ				(90)
#define MSG_INT_RPL				(91)
#define MSG_WRT_REQ				(92)
#define MSG_WRT_RPL				(93)
#define MSG_PSH_REQ				(94)
#define MSG_PSH_RPL				(95)
#define MSG_READ_REQ 			(96)
#define MSG_READ_RPL 			(97)
#define MSG_DEL_REQ				(98)
#define MSG_DEL_RPL				(99)

/* reply code */
#define REPLY_SUCCESS			(50)
#define REPLY_FAIL				(51)
#define REPLY_ALTER				(52)

extern char hostname_ctrl[HOSTNAME_LEN];
extern char hostname_data[HOSTNAME_LEN];
extern struct in_addr hostinaddress_ctrl;
extern struct in_addr hostinaddress_data;
extern struct ethhdr hostethhdr_ctrl;
extern struct ethhdr hostethhdr_data;

#endif /* SRC_GLOBAL_H_ */
