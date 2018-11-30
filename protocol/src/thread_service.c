/*
 * thread_service.c
 *
 *  Created on: Aug 29, 2016
 *      Author: xs6
 */

#include "thread_service.h"

void *thread_domainSocket(void *arg) {
	zlog_info(zc, "api thread started");
	setaffinity(pthread_LocalSocket, api_channel_affinity % num_cores);
	zlog_info(zc, "set api thread to core [%d]/[%d]", api_channel_affinity % num_cores, num_cores);

#if (!GLIB_CHECK_VERSION (2, 36, 0))
	g_type_init();
#endif

	/* socket and option variables */
	int sock, new_sock, max;
	int optval = 1;

	/* server socket address variables */
	struct sockaddr_un addr;

	/* socket address variables for a connected client */
	socklen_t addr_len = sizeof(struct sockaddr_un);

	/* maximum number of pending connection requests */
	int BACKLOG = 5;

	/* variables for select */
	fd_set read_set, write_set;
	struct timeval tv_select_timeout = { .tv_sec = SELECT_TIMEOUT_SEC, .tv_usec = SELECT_TIMEOUT_USEC };
	struct timeval tv_st, tv_cur, tv_diff;
	int ret;

	ssize_t count;

	struct sockaddr_un local_socket_addr;

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket error");
		exit(-1);
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		perror("setting TCP socket option");
		abort();
	}

	memset(&local_socket_addr, 0, sizeof(local_socket_addr));
	local_socket_addr.sun_family = AF_UNIX;
	strncpy(local_socket_addr.sun_path, DOMAIN_SOCKET_FILENAME, sizeof(local_socket_addr.sun_path) - 1);

	unlink(DOMAIN_SOCKET_FILENAME);

	if (bind(sock, (struct sockaddr*) &local_socket_addr, sizeof(local_socket_addr)) == -1) {
		perror("bind error");
		exit(-1);
	}

	if (listen(sock, BACKLOG) == -1) {
		perror("listen error");
		exit(-1);
	}

	ssize_t bytes_ret, bytes_msg;

	MsgPshQ msgPshQ;
	MsgWrtQ msgWrtQ;
	MsgIntQ msgIntQ;

	FdState *fds_p;
	FdState *fds_p2;
	LocalSocketState *lsock_p;
	LocalSocketState *lsock_p2;

	AppState *app_p;
	AppState *app_p2;
	RecverState *recv_p;
	RecverState *recv_p2;
	MulticastState *multi_p;

	apr_hash_index_t *hi;

	while (1) {

		// set file descriptor that select() listen to
		FD_ZERO(&read_set); /* clear everything */
		FD_ZERO(&write_set); /* clear everything */
		FD_SET(sock, &read_set); /* put the listening socket in */
		max = sock; /* initialize max */
		// set connected sockets
		{
			LIST_FOREACH_DOUBLYLINKEDLIST(lsockList, first, next, _ls_cur, _ls_next)
				fds_p = (FdState *) (_ls_cur->value);
				if (!(fds_p->is_trashed)) {
					FD_SET(fds_p->fd, &read_set);
					if (fds_p->sendQueue_p) {
						if (apr_queue_size(fds_p->sendQueue_p) > 0) { /* set write_set */
							FD_SET(fds_p->fd, &write_set);
						}
					}
					if (max < fds_p->fd) { // update max
						max = fds_p->fd;
					}
				}
			}
		}

		// request&release multicast path
		{
			// release
			if (activeMulticast_p && activeMulticast_p->bcd_state == BCDSTATE_DEAD) { /* check if there is a need to release */
				// if (activeMulticast_p->n_all_done == activeMulticast_p->n_init) {
				if (connect_controller) {
					gettimeofday(&(activeMulticast_p->tv_path_release), NULL);
					activeMulticast_p->is_path_released = 1;
					thrift_socket_p = g_object_new(THRIFT_TYPE_SOCKET, "hostname", NETWORK_CONTROLLER_ADDR, "port", NETWORK_CONTROLLER_PORT, NULL);
					thrift_transport_p = g_object_new(THRIFT_TYPE_BUFFERED_TRANSPORT, "transport", thrift_socket_p, NULL);
					thrift_protocol_p = g_object_new(THRIFT_TYPE_BINARY_PROTOCOL, "transport", thrift_transport_p, NULL);

					/* connect to server */
					gboolean tio = thrift_transport_is_open(thrift_transport_p);
					zlog_debug(zc, "thrift_transport_p is [%s] open", tio ? "" : "NOT");
					if (!tio) {
						if (!error_p && thrift_transport_open(thrift_transport_p, &error_p)) {
							zlog_debug(zc, "transport opened");
						} else {
							zlog_fatal(zc, "open failed. domain [%d] code [%d] msg [%s]", error_p->domain, error_p->code, error_p->message);
							g_clear_error(&error_p);
						}
						thrift_client_p = g_object_new(EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_CLIENT, "input_protocol", thrift_protocol_p, "output_protocol", thrift_protocol_p, NULL);
					}

					zlog_debug(zc, "RELEASE called. rid [%d]", activeMulticast_p->controller_xid);
					if (!error_p && edu_rice_bold_service_bcd_service_if_unpush(thrift_client_p, activeMulticast_p->controller_xid, &error_p)) {
						zlog_debug(zc, "RELEASE succeed.");
					} else {
						zlog_fatal(zc, "RELEASE failed. domain [%d] code [%d] msg [%s]", error_p->domain, error_p->code, error_p->message);
						g_clear_error(&error_p);
					}

					g_object_unref(thrift_client_p);
					g_object_unref(thrift_protocol_p);
					g_object_unref(thrift_transport_p);
					g_object_unref(thrift_socket_p);
				}
				activeMulticast_p = NULL;
				// }
			}

			// request
			MulticastState **multi_pp = NULL;/* check if there is a need to r */
			if ((!activeMulticast_p) && (multi_pp = (MulticastState **) apr_skiplist_peek(queuePush_p))) {
				MulticastState *multi_p2 = *multi_pp;
				zlog_info(zc, "top waiting request did[%ld]", multi_p2->bcd_id.data_id);
				struct timeval tv_cur;
				gettimeofday(&tv_cur, NULL);
				if (timercmp(&(multi_p2->tv_push), &tv_cur, <)) { /* it's time to try to request for the push */
					/* take out MulticastState from the queue */
					multi_pp = (MulticastState **) apr_skiplist_pop(queuePush_p, NULL);
					apr_skiplist_free(queuePush_p, (void *) multi_pp);

					/* master */
					char * master_p = (char *) malloc(sizeof(char) * IP_ADDR_LEN);
					strcpy(master_p, inet_ntoa(multi_p2->msgIntQ.masterinaddr));
					edu_rice_bold_serviceServerID master_si_p = master_p;

					/* slaves */
					GHashTable *slaves_ght_p = (GHashTable *) g_hash_table_new(g_str_hash, g_str_equal);
					char * slaves_p = (char *) malloc(sizeof(char) * IP_ADDR_LEN * multi_p2->msgPshQ.fan_out_effective);
					int i;
					for (i = 0; i < multi_p2->msgPshQ.fan_out_effective; i++) {
						if (multi_p2->msgPshQ.slaves[i].s_addr == hostinaddress_ctrl.s_addr) {
							zlog_debug(zc, "slave[%d] [%s] [skipped]", i, inet_ntoa(multi_p2->msgPshQ.slaves[i]));
						} else {
							strcpy(&(slaves_p[i * IP_ADDR_LEN]), inet_ntoa(multi_p2->msgPshQ.slaves[i]));
							g_hash_table_insert(slaves_ght_p, &(slaves_p[i * IP_ADDR_LEN]), NULL);
							zlog_debug(zc, "slave[%d] [%s] [added]", i, inet_ntoa(multi_p2->msgPshQ.slaves[i]));
						}
					}

					/* bcd info */
					edu_rice_bold_serviceBcdInfo * bcd_info_p = (edu_rice_bold_serviceBcdInfo *) g_object_new(EDU_RICE_BOLD_SERVICE_TYPE_BCD_INFO, NULL);
					g_object_set(bcd_info_p, "id", multi_p2->bcd_filename, "size", multi_p2->data_size, NULL);

					/* REPLY */
					edu_rice_bold_servicePushReply *push_reply_p = (edu_rice_bold_servicePushReply *) g_object_new(EDU_RICE_BOLD_SERVICE_TYPE_PUSH_REPLY, NULL);
					gint32 pushreply_xid;
					edu_rice_bold_servicePushReplyCmd pushreply_cmd;

					if (connect_controller) {
//						if (!multi_p2->is_path_requested) {
						gettimeofday(&(multi_p2->tv_path_request), NULL);
//							multi_p2->is_path_requested = 1;
//						}

						thrift_socket_p = g_object_new(THRIFT_TYPE_SOCKET, "hostname", NETWORK_CONTROLLER_ADDR, "port", NETWORK_CONTROLLER_PORT, NULL);
						thrift_transport_p = g_object_new(THRIFT_TYPE_BUFFERED_TRANSPORT, "transport", thrift_socket_p, NULL);
						thrift_protocol_p = g_object_new(THRIFT_TYPE_BINARY_PROTOCOL, "transport", thrift_transport_p, NULL);

						/* connect to server */
						gboolean tio = thrift_transport_is_open(thrift_transport_p);
						zlog_debug(zc, "thrift_transport_p is [%s] open", tio ? "" : "NOT");
						if (!tio) {
							if (!error_p && thrift_transport_open(thrift_transport_p, &error_p)) {
								zlog_debug(zc, "transport opened");
							} else {
								zlog_fatal(zc, "open failed. domain [%d] code [%d] msg [%s]", error_p->domain, error_p->code, error_p->message);
								g_clear_error(&error_p);
							}
							thrift_client_p = g_object_new(EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_CLIENT, "input_protocol", thrift_protocol_p, "output_protocol", thrift_protocol_p, NULL);
						}

						/* request */
						zlog_debug(zc, "REQUEST called. did[%ld]", multi_p2->bcd_id.data_id);
						if (!error_p && edu_rice_bold_service_bcd_service_if_push(thrift_client_p, &push_reply_p, master_si_p, slaves_ght_p, bcd_info_p, &error_p)) {
							zlog_debug(zc, "REQUEST succeed.");
						} else {
							zlog_fatal(zc, "REQUEST failed. domain [%d] code [%d] msg [%s]", error_p->domain, error_p->code, error_p->message);
							g_clear_error(&error_p);
						}

						if (!error_p && thrift_transport_close(thrift_transport_p, &error_p)) {
							zlog_debug(zc, "transport closed");
						} else {
							zlog_fatal(zc, "close failed. domain [%d] code [%d] msg [%s]", error_p->domain, error_p->code, error_p->message);
							g_clear_error(&error_p);
						}

						/* disconnect from server */
						g_object_get(push_reply_p, "xid", &pushreply_xid, "cmd", &pushreply_cmd, NULL);
						zlog_info(zc, "REPLY received. xid[%d], cmd[%d]", pushreply_xid, pushreply_cmd);

						g_object_unref(thrift_client_p);
						g_object_unref(thrift_protocol_p);
						g_object_unref(thrift_transport_p);
						g_object_unref(thrift_socket_p);

					} else {
						pushreply_cmd = EDU_RICE_BOLD_SERVICE_PUSH_REPLY_CMD_PUSH;
						pushreply_xid = 0;
					}

					free(slaves_p);
					free(master_p);
					g_object_unref(bcd_info_p);
					g_hash_table_remove_all(slaves_ght_p);
					g_hash_table_unref(slaves_ght_p);
					g_object_unref(push_reply_p);

					/* process push reply */
					multi_p2->controller_xid = pushreply_xid;
					switch (pushreply_cmd) {
					case EDU_RICE_BOLD_SERVICE_PUSH_REPLY_CMD_DENY:
						zlog_info(zc, "REQUEST did[%ld] is denied", multi_p2->bcd_id.data_id);
						//TODO
					case EDU_RICE_BOLD_SERVICE_PUSH_REPLY_CMD_POSTPONE:
						zlog_info(zc, "REQUEST did[%ld] is postponed", multi_p2->bcd_id.data_id);
						multi_p2->tv_push.tv_sec++;
						multi_pp = (MulticastState **) apr_skiplist_alloc(queuePush_p, sizeof(MulticastState *));
						*multi_pp = multi_p2;
						if (!apr_skiplist_insert(queuePush_p, multi_pp)) { /* insert MulticastState to the priority queue for push */
							zlog_fatal(zc, "insert MulticastState fails");
						}
						break;
					case EDU_RICE_BOLD_SERVICE_PUSH_REPLY_CMD_PUSH:
						zlog_info(zc, "REQUEST accepted");
						gettimeofday(&(multi_p2->tv_path_approve), NULL);
						multi_p2->bcd_state = BCDSTATE_INITIATED;
						activeMulticast_p = multi_p2;
						pthread_kill(pthread_NetmapR, SIGUSR1);
						break;
					default:
						break;
					}
				}
			}
		}

		tv_st = tv_select_timeout;
		/* invoke select, make sure to pass max+1 !!! */
		ret = select(max + 1, &read_set, &write_set, NULL, &tv_st);
		if (ret == 0) { /* timeout */
		} else if (ret > 0) {/* at least one file descriptor is ready */
			// new connection
			if (FD_ISSET(sock, &read_set)) {
				new_sock = accept(sock, (struct sockaddr *) &addr, &addr_len); /* there is an incoming connection, try to accept it */
				if (new_sock < 0) {
					perror("error accepting connection");
					abort();
				}

				/* make the socket non-blocking so send and recv will
				 return immediately if the socket is not ready.
				 this is important to ensure the server does not get
				 stuck when trying to send data to a socket that
				 has too much data to send already.
				 */
				if (fcntl(new_sock, F_SETFL, O_NONBLOCK) < 0) {
					perror("making socket non-blocking");
					abort();
				}

				/* the connection is made, everything is ready */
				/* let's see who's connecting to us */
				zlog_info(zc, "new connection accepted, address[%s]", addr.sun_path);

				fds_p2 = createLocalSocketState(new_sock, addr, localSocketID++); /* create the state for the local socket */
				zlog_info(zc, "fds created[%p]", (void * )fds_p2);

				List_push(lsockList, fds_p2); /* push the state to the list */
				zlog_info(zc, "fds added to the list[%p]", (void * )fds_p2);
			}

			// check connected sockets
			{
				gettimeofday(&tv_cur, NULL);
				LIST_FOREACH_DOUBLYLINKEDLIST(lsockList, first, next, Rcc, Rnn)
					fds_p = (FdState *) (Rcc->value);

					if (!(fds_p->is_trashed)) {
						// send
						if (FD_ISSET(fds_p->fd, &write_set)) { /* the socket is now ready to take more data */
							sendMsg(fds_p->fd, fds_p->sendQueue_p, &(fds_p->remainMsgToken_p));
						}

						// receive
						if (FD_ISSET(fds_p->fd, &read_set)) { /* we have data from a client */
							count = read(fds_p->fd, fds_p->recv_buf + fds_p->recv_ed_byte, RECVBUF_LEN - fds_p->recv_ed_byte);
							if (count > 0) {
								fds_p->recv_ed_byte += count;
								while (fds_p->recv_ed_byte > 0) { /* process the received bytes */
									if (fds_p->recv_ed_byte >= MSG_HDRLEN && fds_p->mh.type == MSG_NONTYPE) {
										parseMsgHdr(&(fds_p->mh), fds_p->recv_buf);
										fds_p->recv_expect_byte = fds_p->mh.len;
										zlog_debug(zc, "get MsgHdr from fd[%d]: msb[%016lx], lsb[%016lx], did[%ld], type[%d], len[%d]", fds_p->fd_type, fds_p->mh.bcd_id.app_id.msb, fds_p->mh.bcd_id.app_id.lsb, fds_p->mh.bcd_id.data_id,
											fds_p->mh.type, fds_p->mh.len);
									}
									if (fds_p->recv_ed_byte >= fds_p->recv_expect_byte) { /* got a complete message */
										app_p = (AppState *) apr_hash_get(hashApp, &(fds_p->mh.bcd_id.app_id), sizeof(AppID));
										// TODO: RecverState & MulticastState should be associated to AppState
										recv_p = (RecverState *) apr_hash_get(hashRecver, (&(fds_p->mh.bcd_id)), sizeof(BcdID));
										multi_p = (MulticastState *) apr_hash_get(hashMulticast, (&(fds_p->mh.bcd_id)), sizeof(BcdID));

										switch (fds_p->fd_type) {
										case (IFACE_UDS): {
											lsock_p = (LocalSocketState *) (fds_p->state_p);
											switch (fds_p->mh.type) {
											case MSG_READ_REQ:
												zlog_info(zc, "get MSG_READ_REQ, did[%ld], eid[%d]", fds_p->mh.bcd_id.data_id, lsock_p->msgIntQ.executor_id);
												assert(lsock_p->msgIntQ.executor_id != MASTER_EXECUTOR_ID);

												/* set the state of UDS that it is receiving the data */
												lsock_p->reading = 1;
												lsock_p->bcd_id = fds_p->mh.bcd_id;

												zlog_debug(zc, "master [%s] on the slave node", (hostinaddress_ctrl.s_addr == lsock_p->msgIntQ.masterinaddr.s_addr) ? "" : "not");
												if (hostinaddress_ctrl.s_addr != lsock_p->msgIntQ.masterinaddr.s_addr) { // the slave is not on a node running the master
													if (recv_p) {
														zlog_debug(zc, "RecverState did[%ld] has already exists", fds_p->mh.bcd_id.data_id);
														if (recv_p->is_finished || (recv_p->is_seq_zero && seq_zero_notify)) { // slave process should be notified
															zlog_info(zc, "did[%ld] has finished", recv_p->bcd_id.data_id);
															MsgReadRpl mr = { .code = htonl(REPLY_SUCCESS), .start = htobe64(recv_p->fileMetadata.start), .jump = htobe64(recv_p->fileMetadata.jump), .file_size = htobe64(recv_p->data_size) };
															zlog_debug(zc, "create MSG_READ_RPL did[%ld] start[%ld] jump[%ld]", recv_p->bcd_id.data_id, recv_p->fileMetadata.start, recv_p->fileMetadata.jump);
															if (!insertMsg2SendQ(fds_p->sendQueue_p, &(recv_p->bcd_id), MSG_READ_RPL, (void *) &mr, sizeof(MsgReadRpl), 0, 1)) {
																zlog_fatal(zc, "insert MSG_READ_RPL fails");
																abort();
															}
															lsock_p->reading = 0;
														}
													} else { // RecverState does not exist
														zlog_debug(zc, "RecverState does not exist, msb[%016lx], lsb[%016lx], did[%ld]", fds_p->mh.bcd_id.app_id.msb, fds_p->mh.bcd_id.app_id.lsb, fds_p->mh.bcd_id.data_id);
														//													fds_p2 = NULL;
														//													/* get the recvSocketState for master IP */
														//													{ /* TODO: get the recv socket from hashmap */
														//														LIST_FOREACH_DOUBLYLINKEDLIST(rsockList, first, next, Cc, Cn)
														//															FdState *_fds_p = (FdState *) (Cc->value);
														//															if (_fds_p->fd_type == CTRL_RECV_TCP) {
														//																if (!strcmp(lsock_p->msgIntQ.master_addr, ((RecvSocketState *) (_fds_p->state_p))->ip_address)) {
														//																	fds_p2 = _fds_p;
														//																	zlog_debug(zc, "found RecvSocketState having the same IP address: %s", ((RecvSocketState * ) (_fds_p->state_p))->ip_address);
														//																	break;
														//																}
														//															}
														//														}
														//													}
														//
														//													if (!fds_p2) {
														//														// TODO: ip address and nic name should comes from the local socket msg parameter
														//														zlog_warn(zc, "create RecvSocketState for ip[%s], nic[%s]", lsock_p->msgIntQ.master_addr, ctrl_nic_name);
														//														fds_p2 = createRecvSocketState(lsock_p->msgIntQ.master_addr, ctrl_nic_name);
														//														List_push(rsockList, fds_p2);
														//													}

														/* the above code just connects to the agent of the master */

														// send MSG_PULL to the master to let the master know this receiver
														zlog_debug(zc, "create MSG_I2C_PULL, did[%ld]", fds_p->mh.bcd_id.data_id);
														//													msgPullI2C.master_fds_p = htobe64(fds_p2);
														bytes_msg = createMsg(i2c_buf, &(fds_p->mh.bcd_id), MSG_I2C_PULL, (char *) &(lsock_p->msgIntQ), MSG_HDRLEN + sizeof(MsgIntQ), 0);
														bytes_ret = write(pipe_fd_i2c_recv[1], i2c_buf, bytes_msg);
														assert(bytes_ret == bytes_msg);

													}
												} else {
													zlog_info(zc, "did[%ld] is on local", multi_p->bcd_id.data_id);
													assert(multi_p);
													MsgReadRpl mr = { .code = htonl(REPLY_SUCCESS), .start = htobe64(0), .jump = htobe64(multi_p->data_size), .file_size = htobe64(multi_p->data_size) };
													zlog_debug(zc, "create MSG_READ_RPL did[%ld] start[%ld] jump[%ld]", multi_p->bcd_id.data_id, 0L, multi_p->data_size);
													if (!insertMsg2SendQ(fds_p->sendQueue_p, &(multi_p->bcd_id), MSG_READ_RPL, (void *) &mr, sizeof(MsgReadRpl), 0, 1)) {
														zlog_fatal(zc, "insert MSG_READ_RPL fails");
														abort();
													}
													lsock_p->reading = 0;
												}
												break;
											case MSG_DEL_REQ:
												zlog_info(zc, "get MSG_DEL_REQ, did[%ld], eid[%d] ", fds_p->mh.bcd_id.data_id, lsock_p->msgIntQ.executor_id);
												assert(app_p);
												if (multi_p || recv_p) {
													zlog_info(zc, "executor_id [%d], multi_p [%p], recv_p [%p], host [%d], master [%d]", lsock_p->msgIntQ.executor_id, (void * ) multi_p, (void * )recv_p, hostinaddress_ctrl.s_addr,
														lsock_p->msgIntQ.masterinaddr.s_addr);

													assert(
														((lsock_p->msgIntQ.executor_id == MASTER_EXECUTOR_ID) && multi_p) || ((lsock_p->msgIntQ.executor_id != MASTER_EXECUTOR_ID) && recv_p) || ((lsock_p->msgIntQ.executor_id != MASTER_EXECUTOR_ID) && (hostinaddress_ctrl.s_addr == lsock_p->msgIntQ.masterinaddr.s_addr)));

													zlog_debug(zc, "connected uds number [%d], delete number [%d]", apr_hash_count(app_p->uds_hash), multi_p ? multi_p->delete_counter : recv_p->delete_counter);
													if ((lsock_p->msgIntQ.executor_id == MASTER_EXECUTOR_ID) || ((lsock_p->msgIntQ.executor_id != MASTER_EXECUTOR_ID) && (hostinaddress_ctrl.s_addr == lsock_p->msgIntQ.masterinaddr.s_addr))) {
														if ((++multi_p->delete_counter) >= apr_hash_count(app_p->uds_hash)) {
															zlog_debug(zc, "remove() %s", multi_p->bcd_filename);
															closeFile(&(multi_p->fd));
															closeFile(&(multi_p->fd_patch));
															removeData(multi_p->bcd_filename);
														}
													} else {
														if ((++recv_p->delete_counter) >= apr_hash_count(app_p->uds_hash)) {
															zlog_debug(zc, "remove() %s", recv_p->bcd_filename);
															closeFile(&(recv_p->fd_p));
															closeFile(&(recv_p->fd_patch_p));
															removeData(recv_p->bcd_filename);
														}
													}
												}

												/* prepare reply message */
												MsgDelRpl mdr = { .code = htonl(REPLY_SUCCESS) };
												zlog_debug(zc, "create MSG_DEL_RPL, did[%ld], eid[%d]", fds_p->mh.bcd_id.data_id, lsock_p->msgIntQ.executor_id);
												/* send reply message */
												if (!insertMsg2SendQ(fds_p->sendQueue_p, &(fds_p->mh.bcd_id), MSG_DEL_RPL, (void *) &mdr, sizeof(MsgDelRpl), 0, 1)) {
													zlog_fatal(zc, "insert MSG_DEL_RPL fail, did[%ld]", fds_p->mh.bcd_id.data_id);
													abort();
												}
												break;
											case MSG_WRT_REQ: /* issued only from the master */
												parseMsgWrtQ(&msgWrtQ, fds_p->recv_buf + MSG_HDRLEN); /* parse push request message */
												zlog_info(zc, "get MSG_WRT_REQ, did[%ld], eid[%d], bsize[%ld]", fds_p->mh.bcd_id.data_id, lsock_p->msgIntQ.executor_id, msgWrtQ.bcd_size);
												assert(lsock_p->msgIntQ.executor_id == MASTER_EXECUTOR_ID);
												assert(!multi_p);
												assert(app_p);

												/* create MulticastState */
												multi_p = createBcdState(&(fds_p->mh.bcd_id), &(lsock_p->msgIntQ), &msgWrtQ, rate_type, rate_f, ctrl_nic_name);
												zlog_debug(zc, "create MulticastState, [%s]", multi_p->bcd_filename);
												apr_hash_set(hashMulticast, &(multi_p->bcd_id), sizeof(BcdID), multi_p);

												/* add to hashmap of appState */

												apr_hash_set(app_p->bcd_sender_hash, &(multi_p->bcd_id), sizeof(BcdID), multi_p);

												/* prepare reply message */
												MsgWrtRpl mwr = { .code = htonl(REPLY_SUCCESS) };
												zlog_debug(zc, "create MSG_WRT_RPL, did[%ld]", fds_p->mh.bcd_id.data_id);
												/* send reply message */
												if (!insertMsg2SendQ(fds_p->sendQueue_p, &(fds_p->mh.bcd_id), MSG_WRT_RPL, (void *) &mwr, sizeof(MsgWrtRpl), 0, 1)) {
													zlog_fatal(zc, "insert MSG_WRT_RPL fail, did[%ld]", fds_p->mh.bcd_id.data_id);
													abort();
												}
												break;
											case MSG_PSH_REQ: /* issued only from the master, ask this program to push the data to a specific set of slave nodes */
												parseMsgPshQ(&msgPshQ, fds_p->recv_buf + MSG_HDRLEN); /* parse push request message */
												zlog_info(zc, "get MSG_PSH_REQ, did[%ld], eid[%d]", fds_p->mh.bcd_id.data_id, lsock_p->msgIntQ.executor_id);

												assert(lsock_p->msgIntQ.executor_id == MASTER_EXECUTOR_ID);
												assert(multi_p);

												multi_p->msgPshQ = msgPshQ;

												zlog_debug(zc, "[%s] push bcd_id [%s]", multi_p->msgPshQ.fan_out_effective > 0 ? "" : "no", multi_p->bcd_filename);
												if (multi_p->msgPshQ.fan_out_effective) { // multicast path request
													multicastState_reset(multi_p);
													int i;
													for (i = 0; i < multi_p->msgPshQ.fan_out_effective; i++) { // create SenderState for each receiver
														SenderState *sender_p = createSenderState((multi_p->n_init)++, &(multi_p->msgPshQ.slaves[i]), &(multi_p->bcd_id), NULL);
														zlog_debug(zc, "create SenderState sender id[%d] ip[%s] key[0x%08x]", sender_p->id, inet_ntoa(sender_p->skey), sender_p->skey.s_addr);
														apr_hash_set(multi_p->sender_addr_hash_p, (const void *) (&(sender_p->skey)), (apr_ssize_t) (sizeof(struct in_addr)), (const void *) sender_p); /* put SenderState into hashSender */
														if (sender_p->is_distinct_rack) { // mark if this multicast push has receiver in distinct racks
															multi_p->is_distinct_rack = 1;
														}
													}
													multi_p->sender_end_seq_arr_p = (SenderState **) malloc(multi_p->msgPshQ.fan_out_effective * sizeof(SenderState *));

													gettimeofday(&(multi_p->tv_push), NULL);
													MulticastState ** multi_pp = (MulticastState **) apr_skiplist_alloc(queuePush_p, sizeof(MulticastState *));
													*multi_pp = multi_p;
													if (!apr_skiplist_insert(queuePush_p, multi_pp)) { /* insert MulticastState to the priority queue for push */
														zlog_error(zc, "insert MulticastState fail");
													}
													MulticastState **multi_pp2 = (MulticastState **) apr_skiplist_peek(queuePush_p);
													zlog_debug(zc, "check insertion alive[%d] did[%ld]", (*multi_pp2)->bcd_state, (*multi_pp2)->bcd_id.data_id);
												}

												/* prepare reply message */
												MsgPshRpl mpr = { .code = htonl(REPLY_SUCCESS) };
												zlog_debug(zc, "create MSG_PSH_RPL, did[%ld]", fds_p->mh.bcd_id.data_id);
												/* send reply message */
												if (!insertMsg2SendQ(fds_p->sendQueue_p, &(fds_p->mh.bcd_id), MSG_PSH_RPL, (void *) &mpr, sizeof(MsgPshRpl), 0, 1)) {
													zlog_fatal(zc, "insert MSG_PSH_RPL fail, did[%ld]", fds_p->mh.bcd_id.data_id);
													abort();
												}
												break;
											case MSG_INT_REQ:
												parseMsgIntQ(&msgIntQ, fds_p->recv_buf + MSG_HDRLEN);
												zlog_info(zc, "get MSG_INT_REQ, did[%ld], eid[%d], appid[%s], master[%s]", fds_p->mh.bcd_id.data_id, msgIntQ.executor_id, msgIntQ.app_id, msgIntQ.master_addr);

												/* assign the property of the executor*/
												lsock_p->msgIntQ = msgIntQ;

												/* connect to master if this is a slave */
												zlog_debug(zc, "[%s] executor [%d]", (lsock_p->msgIntQ.executor_id == MASTER_EXECUTOR_ID) ? "master" : "slave", lsock_p->msgIntQ.executor_id);
												if (lsock_p->msgIntQ.executor_id != MASTER_EXECUTOR_ID) {
													zlog_debug(zc, "master [%s] on the slave node", (hostinaddress_ctrl.s_addr == lsock_p->msgIntQ.masterinaddr.s_addr) ? "" : "not");
													if (hostinaddress_ctrl.s_addr != lsock_p->msgIntQ.masterinaddr.s_addr) { // slave not on master node
														/* get RecvSocketState given master ip */
														fds_p2 = NULL;
														{ /* TODO: get the recv socket from hashmap */
															LIST_FOREACH_DOUBLYLINKEDLIST(rsockList, first, next, Cc, Cn)
																FdState *_fds_p = (FdState *) (Cc->value);
																if (_fds_p->fd_type == CTRL_RECV_TCP) {
																	if (!strcmp(lsock_p->msgIntQ.master_addr, ((RecvSocketState *) (_fds_p->state_p))->ip_address)) {
																		fds_p2 = _fds_p;
																		zlog_debug(zc, "found RecvSocketState having the same IP address: %s", ((RecvSocketState * ) (_fds_p->state_p))->ip_address);
																		break;
																	}
																}
															}
														}

														if (!fds_p2) {
															zlog_debug(zc, "create RecvSocketState for ip[%s], nic[%s]", lsock_p->msgIntQ.master_addr, ctrl_nic_name);
															fds_p2 = createRecvSocketState(lsock_p->msgIntQ.master_addr, ctrl_nic_name);
															List_push(rsockList, fds_p2);
														}
													}
												}

												if (!app_p) {/* create AppState if does not exist */
													zlog_debug(zc, "AppState does [not] exist");
													app_p2 = createAppState(&(fds_p->mh.bcd_id.app_id), msgIntQ.app_id, msgIntQ.master_addr);
													zlog_debug(zc, "AppState created @ [%p]", (void * ) app_p2);
												} else {
													zlog_debug(zc, "AppState exist @ [%p]", (void * ) app_p);
												}
												/* connect AppState and UDSState */ //TODO: there may be a bug
												lsock_p->app_p = app_p2;
												apr_hash_set(app_p2->uds_hash, &(lsock_p->id), sizeof(int), fds_p);
												if (!app_p) { /* add AppState to HashMap */
													apr_hash_set(hashApp, &(app_p2->app_id), sizeof(AppID), app_p2);
												}

												/* prepare reply message */
												MsgIntRpl mir = { .code = htonl(REPLY_SUCCESS) };
												zlog_debug(zc, "create MSG_INT_RPL, did[%ld]", fds_p->mh.bcd_id.data_id);
												/* send reply message */
												zlog_debug(zc, "send queue length before [%d] @ [%p]", apr_queue_size(fds_p->sendQueue_p), (void * )(fds_p->sendQueue_p));
												if (!insertMsg2SendQ(fds_p->sendQueue_p, &(fds_p->mh.bcd_id), MSG_INT_RPL, (void *) &mir, sizeof(MsgIntRpl), 0, 1)) {
													zlog_fatal(zc, "insert MSG_INT_RPL fail, did[%ld]", fds_p->mh.bcd_id.data_id);
													abort();
												}
												zlog_debug(zc, "send queue length after [%d]", apr_queue_size(fds_p->sendQueue_p));
												break;
											default:
												zlog_warn(zc, "get non-supported message did[%ld] type [%d]", fds_p->mh.bcd_id.data_id, fds_p->mh.type);
												break;
											}
											break;
										}
										case (PIPE_C2I_UDS): {
											switch (fds_p->mh.type) {
											case MSG_C2I_PULL_REJECT:
												zlog_debug(zc, "get MSG_C2I_PULL_REJECT, did[%ld]", fds_p->mh.bcd_id.data_id);
												assert(app_p);

												for (hi = apr_hash_first(NULL, app_p->uds_hash); hi; hi = apr_hash_next(hi)) {
													apr_hash_this(hi, NULL, NULL, (void*) &fds_p2);
													assert(fds_p2->fd_type == IFACE_UDS);
													lsock_p2 = (LocalSocketState *) (fds_p2->state_p);
													if (lsock_p2->reading && (lsock_p2->bcd_id.app_id.lsb == fds_p->mh.bcd_id.app_id.lsb) && (lsock_p2->bcd_id.app_id.msb == fds_p->mh.bcd_id.app_id.msb)
														&& (lsock_p2->bcd_id.data_id == fds_p->mh.bcd_id.data_id)) { /* UDS is reading the finished bcd */
														zlog_info(zc, "notify about did[%ld]", fds_p->mh.bcd_id.data_id);
														MsgReadRpl mrr = { .code = htonl(REPLY_ALTER) };
														zlog_debug(zc, "create MSG_READ_RPL code[%d] did[%ld]", REPLY_ALTER, fds_p->mh.bcd_id.data_id);
														if (!insertMsg2SendQ(fds_p2->sendQueue_p, &(fds_p->mh.bcd_id), MSG_READ_RPL, (void *) &mrr, sizeof(MsgReadRpl), 0, 1)) {
															zlog_fatal(zc, "insert MSG_READ_RPL fails");
															abort();
														}
														lsock_p2->reading = 0;
													}
												}

												break;
											case MSG_C2I_SEQ_ALL:
												zlog_debug(zc, "get MSG_C2I_SEQ_ALL, did[%ld]", recv_p->bcd_id.data_id);
												struct timeval tv_recv_stats_1, tv_recv_stats_2, tv_recv_stats_3;
												gettimeofday(&tv_recv_stats_1, NULL);

												assert(app_p);
												assert(recv_p);
												recverState_stats(recv_p, app_p->fd_stats_p);

												gettimeofday(&tv_recv_stats_2, NULL);
												timersub(&tv_recv_stats_2, &tv_recv_stats_1, &tv_recv_stats_3);
												float recv_stats_diff_ms = tv_recv_stats_3.tv_sec * 1000.0 + tv_recv_stats_3.tv_usec / 1000.0;
												if (recv_stats_diff_ms > 0.1 || recv_stats_diff_ms < -1) {
													D("recv stats time [%.3f]ms", recv_stats_diff_ms);
												}
												/* note: no break; here */
											case MSG_C2I_SEQ_ZERO:
												zlog_debug(zc, "get MSG_C2I_SEQ_ZERO, did[%ld]", recv_p->bcd_id.data_id);

												assert(app_p);
												assert(recv_p);

												for (hi = apr_hash_first(NULL, app_p->uds_hash); hi; hi = apr_hash_next(hi)) {
													apr_hash_this(hi, NULL, NULL, (void*) &fds_p2);
													assert(fds_p2->fd_type == IFACE_UDS);
													lsock_p2 = (LocalSocketState *) (fds_p2->state_p);
													recv_p2 = (RecverState *) apr_hash_get(hashRecver, (void *) &(lsock_p2->bcd_id), sizeof(BcdID));
													if (lsock_p2->reading && (recv_p == recv_p2)) { /* UDS is reading the finished bcd */
														zlog_info(zc, "notify about did[%ld]", recv_p->bcd_id.data_id);
														MsgReadRpl mr = { .code = htonl(REPLY_SUCCESS), .start = htobe64(recv_p->fileMetadata.start), .jump = htobe64(recv_p->fileMetadata.jump), .file_size = htobe64(recv_p->data_size) };
														zlog_debug(zc, "create MSG_READ_RPL did[%ld] start[%ld] jump[%ld]", recv_p->bcd_id.data_id, recv_p->fileMetadata.start, recv_p->fileMetadata.jump);
														//							D("create MSG_READ_RPL did[%ld] start[%d] jump[%d]", recv_p2->bcd_id.data_id, recv_p2->fileMetadata.start,
														//								recv_p2->fileMetadata.jump);
														if (!insertMsg2SendQ(fds_p2->sendQueue_p, &(recv_p->bcd_id), MSG_READ_RPL, (void *) &mr, sizeof(MsgReadRpl), 0, 1)) {
															zlog_fatal(zc, "insert MSG_READ_RPL fails");
															abort();
														}
														lsock_p2->reading = 0;
													}
												}
												break;
											default:
												break;
											}
											break;
										}
										case (PIPE_C_SEND2I_UDS): {
											switch (fds_p->mh.type) {
											case MSG_C_SEND2I_SEQ_ALL:
												zlog_debug(zc, "get MSG_C_SEND2I_SEQ_ALL");
												struct timeval tv_multi_stats_1, tv_multi_stats_2, tv_multi_stats_3;
												gettimeofday(&tv_multi_stats_1, NULL);

												assert(app_p);
												assert(multi_p);
												if (!(multi_p->is_path_released)) {
													gettimeofday(&(multi_p->tv_path_release), NULL);
													multi_p->tv_last_data = multi_p->tv_path_release;
												}
												multicastState_stats(multi_p, app_p->fd_stats_p);

												gettimeofday(&tv_multi_stats_2, NULL);
												timersub(&tv_multi_stats_2, &tv_multi_stats_1, &tv_multi_stats_3);
												float multi_stats_diff_ms = tv_multi_stats_3.tv_sec * 1000.0 + tv_multi_stats_3.tv_usec / 1000.0;
												if (multi_stats_diff_ms > 0.1 || multi_stats_diff_ms < -1) {
													D("multi stats time [%.3f]ms", multi_stats_diff_ms);
												}
												break;
											default:
												break;
											}
											break;
										}
										default:
											break;
										}

										fds_p->recv_ed_byte -= fds_p->recv_expect_byte; /* reset the state*/

										if (fds_p->recv_ed_byte > 0) {
											memmove(fds_p->recv_buf, fds_p->recv_buf + fds_p->recv_expect_byte, fds_p->recv_ed_byte);
										}

										fds_p->recv_expect_byte = BYTE_MAX;
										fds_p->mh.type = MSG_NONTYPE;

									} else {
										break;
									}
								}
							} else if (count == 0) {
								zlog_info(zc, "Local Client closed connection. Client address is: %s, uds_id[%d]", ((LocalSocketState * )(fds_p->state_p))->client_addr.sun_path, ((LocalSocketState * )(fds_p->state_p))->id);
								trashFdState(fds_p);
							} else {
								if (errno == EINTR || errno == EAGAIN) {
									// continue;
								} else {
									perror("read() fail");
									zlog_error(zc, "errno[%d]", errno);
									trashFdState(fds_p);
								}
							}
						}
					} else { // delete trashed UDS after timeout
						timersub(&tv_cur, &(fds_p->tv_trashed), &tv_diff);
						if (tv_diff.tv_sec * 1000.0 + tv_diff.tv_usec / 1000.0 > FDSTATE_DELETE_TIMEOUT_MSEC) {
							assert(fds_p->fd_type == IFACE_UDS);
							zlog_debug(zc, "fds[%p] is being trashed", (void * ) fds_p);
							zlog_debug(zc, "fds_p->state_p[%p] is being trashed", fds_p->state_p);
							zlog_info(zc, "deleteFdState [%d]", ((LocalSocketState * )(fds_p->state_p))->id);
							deleteFdState((FdState *) List_remove(lsockList, Rcc));
						}
					}
				}
			}
		} else {
			if (errno == EINTR || errno == EINVAL) {
			} else {
				perror("select() fail");
				zlog_error(zc, "errno[%d]", errno);
			}
		}
	}
}
