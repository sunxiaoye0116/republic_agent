/*
 * global.c
 *
 *  Created on: Nov 16, 2015
 *      Author: xs6
 */

#include "global.h"

zlog_category_t *zc;

/* sender pool */
apr_pool_t *pool_NetmapS_p;
apr_pool_t *pool_HashMulticast_S_p;
apr_pool_t *pool_ssock_p;

/* receiver pool */
apr_pool_t *pool_HashRecver_L_p; /* hashmap: key: [xid], value: */
apr_pool_t *pool_lsock_p;
apr_pool_t *pool_rsock_p;

/* recv & send shared */
pthread_t pthread_NetmapR;
pthread_t pthread_LocalSocket;

/* receiver threads */
pthread_t pthread_RecvSocket;

/* sender threads */
pthread_t pthread_SendSocket;

/* recv & send shared */
List *lsockList; /* state of an executor socket */
apr_hash_t *hashApp;

/* receiver */
apr_hash_t *hashRecver; /* HashMap: key: [BcdID], value: RecverState */
List *rsockList; /* state of recv socket */

/* sender */
apr_hash_t *hashMulticast; /* HashMap: key: [BcdID], value: RecverState */
List *ssockList; /* the state of send socket */
apr_skiplist *queuePush_p; /* priority of waiting push multicast */

/* thrift */
ThriftSocket *thrift_socket_p;
ThriftTransport *thrift_transport_p;
ThriftProtocol *thrift_protocol_p;
edu_rice_bold_serviceBcdServiceIf *thrift_client_p;
GError *error_p;

struct nmreq curr_nmr;
char nmr_name[64];
int do_abort = 0;
int localSocketID = 0;
char n2c_buf[512];
char c2i_buf[512];
char i2c_buf[512];
char c_send2i_buf[512];
char i2c_buf[512];

uint8_t zeros[9216];

// TODO: read from config file
char *data_nic_name = DEFAULT_DATA_NIC_NAME;
char *ctrl_nic_name = DEFAULT_CTRL_NIC_NAME;
uint8_t queue_index = DEFAULT_NETMAP_QUEUE_IDX;
uint8_t seq_zero_notify = 0;
uint8_t msg_padding = 0;
double attempt_scale = DEFAULT_ATTEMPT_SCALE;
pktnum_t nm_burst = DEFAULT_BURST;
bytes_t data_payload_len = DEFAULT_DATA_PAYLOAD_LEN;
uint8_t connect_controller = 0;
uint8_t first_first_seq_fullrate = 0;

int pipe_fd_n2c[2];
int pipe_fd_c2i[2];
int pipe_fd_c_send2i[2];
int pipe_fd_i2c_recv[2];

int32_t data_channel_scheduling_policy = DEFAULT_DATA_CHANNEL_SCHEDULING_POLICY;
int32_t data_channel_scheduling_priority = DEFAULT_DATA_CHANNEL_SCHEDULING_PRIORITY;
uint32_t data_channel_affinity = DEFAULT_DATA_CHANNEL_AFFINITY;
uint32_t ctrl_channel_affinity = DEFAULT_CTRL_CHANNEL_AFFINITY;
uint32_t api_channel_affinity = DEFAULT_API_CHANNEL_AFFINITY;
uint32_t num_cores;

char hostname_ctrl[HOSTNAME_LEN];
char hostname_data[HOSTNAME_LEN];
struct in_addr hostinaddress_ctrl;
struct in_addr hostinaddress_data;
struct ethhdr hostethhdr_ctrl;
struct ethhdr hostethhdr_data;
