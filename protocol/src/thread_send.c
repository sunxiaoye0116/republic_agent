/*
 * thread_send.c
 *
 *  Created on: Aug 29, 2016
 *      Author: xs6
 */

#include "thread_send.h"

/* from sender */

void *thread_sendSocket(void *arg) {
	zlog_info(zc, "ctrl channel (send) thread started");
	setaffinity(pthread_SendSocket, ctrl_channel_affinity % num_cores);
	zlog_info(zc, "set ctrl channel (send) thread to core [%d]/[%d]", ctrl_channel_affinity % num_cores, num_cores);

	ssize_t bytes_ret, bytes_msg;
	/* create command queue for this thread */
	cmd_ring_t * ringCmd = (cmd_ring_t *) apr_palloc(pool_ssock_p, sizeof(cmd_ring_t));
	APR_RING_INIT(ringCmd, _cmd_elem_t, link);

//	struct send_socket_arg *cs_arg = (struct send_socket_arg *) arg;

	/* socket and option variables */
	int sockListen, sockListenNew, sockMax;
	int optval = 1;

	my_elem_t *ptre_p;

	/* server socket address variables */
	struct sockaddr_in sin, addr;

	/* socket address variables for a connected client */
	socklen_t addr_len = sizeof(struct sockaddr_in);

	/* maximum number of pending connection requests */
	int BACKLOG = 5;

	/* variables for select */
	fd_set read_set, write_set;
	struct timeval tv_select_timeout = { .tv_sec = SELECT_TIMEOUT_SEC, .tv_usec = SELECT_TIMEOUT_USEC };
	struct timeval tv_st;
	struct timeval tv_tmp;
	struct timeval tv_diff;
	int ret;
	BcdID *bcd_id_t_p;
	char bcd_id_buf_t_p[100];

	int64_t last_patch_start = 0;
	int64_t last_patch_counter = 0;

	/* a silly message */
	/* number of bytes sent/received */
	int count;

	/* create a server socket to listen for TCP connection requests */
	if ((sockListen = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("opening TCP socket");
		abort();
	}

	/* set option so we can reuse the port number quickly after a restart */
	if (setsockopt(sockListen, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		perror("setting TCP socket option");
		abort();
	}

	/* fill in the address of the server socket */
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(CTRL_DST_PORT);

	/* bind server socket to the address */
	if (bind(sockListen, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		perror("binding socket to address");
		abort();
	}

	/* put the server socket in listen mode */
	if (listen(sockListen, BACKLOG) < 0) {
		perror("listen on socket failed");
		abort();
	}

	/* now we keep waiting for incoming connections, check for incoming data to receive, check for ready socket to send more data */
	while (1) {

		/* check pQueue for seqEnd and create MSG_FAST_DONE */
		void *val = NULL;
		apr_hash_index_t *hi;
		for (hi = apr_hash_first(pool_HashMulticast_S_p, hashMulticast); hi; hi = apr_hash_next(hi)) { /* iterate over multicastState */
			apr_hash_this(hi, NULL, NULL, &val);
			if (val) {
				MulticastState * multi_p = (MulticastState *) val;
				SenderState ** sender_pp = NULL;
				SenderState * sender_p = NULL;
				while ((sender_pp = (SenderState **) (apr_skiplist_peek(multi_p->sender_end_seq_pq_p)))) {
					sender_p = *sender_pp;
					if (multi_p->seq_cur >= sender_p->seqEnd) { /* check the seq_cur against the peek of pQueue */
						gettimeofday(&(sender_p->tv_fast_done), NULL);

						SenderState ** s_pp = (SenderState **) apr_skiplist_pop(multi_p->sender_end_seq_pq_p, NULL);
						apr_skiplist_free(multi_p->sender_end_seq_pq_p, (void *) s_pp);
						multi_p->sender_end_seq_arr_p[multi_p->sender_end_seq_arr_idx_end] = sender_p;
						multi_p->sender_end_seq_arr_idx_end++;

					} else {
						break;
					}
				}
				int i;
				if (multi_p->bcd_state == BCDSTATE_DEAD) {
					gettimeofday(&tv_tmp, NULL);
					for (i = multi_p->sender_end_seq_arr_idx_start; i < multi_p->sender_end_seq_arr_idx_end; i++) { // send FAST_DONE after timeout
						sender_p = multi_p->sender_end_seq_arr_p[i];
						timersub(&tv_tmp, &(sender_p->tv_fast_done), &tv_diff);
						if (tv_diff.tv_sec * 1000.0 + tv_diff.tv_usec / 1000.0 > FAST_DONE_TIMEOUT_MSEC) {
							if (sender_p->has_all_done == 0) {
								zlog_info(zc, "sent MSG_FAST_DONE did[%ld] addr[%s] seq_cur[seq: %d(rn: %d)] seq_end[seq: %d(rn: %d)]", sender_p->bcd_id.data_id, inet_ntoa(sender_p->skey), multi_p->seq_cur % multi_p->seq_max,
									multi_p->seq_cur / multi_p->seq_max, sender_p->seqEnd % multi_p->seq_max, sender_p->seqEnd / multi_p->seq_max);
								cmd_elem_t * cmd_p = createCmd(sender_p->ssock_p->sendQueue_p, &(sender_p->bcd_id), MSG_FAST_DONE, NULL, 0, 0); /* create cmd element */
								APR_RING_INSERT_HEAD(ringCmd, cmd_p, _cmd_elem_t, link); /* insert cmd element */
								zlog_debug(zc, "push MSG_FAST_DONE did[%ld] to command ring", sender_p->bcd_id.data_id);
							}
							multi_p->sender_end_seq_arr_idx_start++;
						} else {
							break;
						}
					}
				}
			}
		}

		/* process command buffer */
		while (!APR_RING_EMPTY(ringCmd, _cmd_elem_t, link)) {
			cmd_elem_t * cmd_p = APR_RING_LAST(ringCmd);
			if (cmd_p) {
				apr_status_t s = apr_queue_trypush(cmd_p->sendQueue_p, (void *) (cmd_p->mt_p));
				if (s == APR_SUCCESS) {
					zlog_debug(zc, "pop a command push to sendQueue did[%ld]", cmd_p->bcd_id.data_id);
					APR_RING_REMOVE(cmd_p, link);
					free(cmd_p);
				} else {
					break;
				}
			} else {
				zlog_fatal(zc, "apr_queue_trypush fails");
			}
		}

		/* set up the file descriptor bit map that select should be watching */
		FD_ZERO(&read_set); /* clear everything */
		FD_ZERO(&write_set); /* clear everything */

		FD_SET(sockListen, &read_set); /* put the listening socket in */
		sockMax = sockListen; /* initialize max */

		/* put connected sockets into the read and write sets to monitor them */

		{
			LIST_FOREACH_DOUBLYLINKEDLIST(ssockList, first, next, Vs, Ns)
				SendSocketState *ssock_p = (SendSocketState *) (Vs->value);
				FD_SET(ssock_p->socket, &read_set); /* set read_set */

				processPatchTuple(ssock_p);
				if (apr_queue_size(ssock_p->sendQueue_p) > 0) {
					FD_SET(ssock_p->socket, &write_set); /* set write_set */
				}

				if (ssock_p->socket > sockMax) { /* update max if necessary */
					sockMax = ssock_p->socket;
				}
			}
		}

		tv_st = tv_select_timeout;

		ret = select(sockMax + 1, &read_set, &write_set, NULL, &tv_st); /* invoke select, make sure to pass max+1 !!! */
		if (ret == 0) { /* no descriptor ready, timeout happened */
			continue;
		} else if (ret > 0) { /* at least one file descriptor is ready */
			if (FD_ISSET(sockListen, &read_set)) { /* check the server socket */
				sockListenNew = accept(sockListen, (struct sockaddr *) &addr, &addr_len);/* there is an incoming connection, try to accept it */
				if (sockListenNew < 0) {
					perror("error accepting connection");
					abort();
				}

				/* make the socket non-blocking so send and recv will return immediately if the socket is not ready. this is important to ensure the server does not get stuck when trying to send data to a socket that has too much data to send already. */
				if (fcntl(sockListenNew, F_SETFL, O_NONBLOCK) < 0) {
					perror("making socket non-blocking");
					abort();
				}
				/* the connection is made, everything is ready */
				/* let's see who's connecting to us */
				zlog_debug(zc, "Accepted connection. Client IP address is: %s", inet_ntoa(addr.sin_addr));

				SendSocketState * ssock_new_p = createSendSocketState(sockListenNew, addr);
				List_push(ssockList, ssock_new_p);
//				struct send_state *sstate_new_p = send_state_create(sockListenNew, addr, multicaster_list->first, recver_id++, pool_p);
//				List_push(((struct multicast_state *) (multicaster_list->first->value))->sender_list, sstate_new_p);
				// continue;
				zlog_debug(zc, "add SendSocketState to list");
			}

			/* check other connected sockets, see if there is anything to read or some socket is ready to send more pending data */
			{
				LIST_FOREACH_DOUBLYLINKEDLIST(ssockList, first, next, Vs, Ns)
					SendSocketState *ssock_p = (SendSocketState *) (Vs->value);
					if (FD_ISSET(ssock_p->socket, &write_set)) {/* see if we can now do some previously unsuccessful writes */
						sendMsg(ssock_p->socket, ssock_p->sendQueue_p, &(ssock_p->remainMsgToken_p));
					}

					if (FD_ISSET(ssock_p->socket, &read_set)) {
						/* we have data from a client */
						count = recv(ssock_p->socket, ssock_p->recv_buf + ssock_p->recv_ed_byte, RECVBUF_LEN - ssock_p->recv_ed_byte, 0);
						if (count > 0) {
							ssock_p->recv_ed_byte += count;
							while (ssock_p->recv_ed_byte > 0) {
								// get the header
								if (ssock_p->recv_ed_byte >= MSG_HDRLEN && ssock_p->mh.type == MSG_NONTYPE) {
									parseMsgHdr(&(ssock_p->mh), ssock_p->recv_buf);
									struct timeval tv_msgdelivertime_2, tv_msgdelivertime_3;
									gettimeofday(&tv_msgdelivertime_2, NULL);
									timersub(&tv_msgdelivertime_2, &(ssock_p->mh.tv), &tv_msgdelivertime_3);
									float msgdelivertime_diff_ms = tv_msgdelivertime_3.tv_sec * 1000.0 + tv_msgdelivertime_3.tv_usec / 1000.0;
//									if (msgdelivertime_diff_ms > .1 || msgdelivertime_diff_ms < -1) {
//										D("message deliver time [%.3f]ms, type[%d]", msgdelivertime_diff_ms, ssock_p->mh.type);
//									}
									ssock_p->recv_expect_byte = ssock_p->mh.len;
									zlog_debug(zc, "get MsgHdr: addr[%s], msb[%016lx], lsb[%016lx], did[%ld], type[%d], len[%d]", inet_ntoa(ssock_p->client_addr.sin_addr), ssock_p->mh.bcd_id.app_id.msb, ssock_p->mh.bcd_id.app_id.lsb,
										ssock_p->mh.bcd_id.data_id, ssock_p->mh.type, ssock_p->mh.len);
								}
								if (ssock_p->recv_ed_byte >= ssock_p->recv_expect_byte) {
									// D("sstate_p->recv_ed_byte [%d] >= sstate_p->recv_expect_byte [%d]", sstate_p->recv_ed_byte, sstate_p->recv_expect_byte);
									MulticastState *multi_p = (MulticastState *) apr_hash_get(hashMulticast, &(ssock_p->mh.bcd_id), sizeof(BcdID)); /* get the multicast state */
									struct in_addr skey = ssock_p->client_addr.sin_addr;
									SenderState *sender_p = NULL;

									/* log output id */
									bcd_id_t_p = &(ssock_p->mh.bcd_id);
									BcdId2str(bcd_id_t_p, bcd_id_buf_t_p);

									if (multi_p) {
										sender_p = (SenderState *) apr_hash_get(multi_p->sender_addr_hash_p, (const void *) (&skey), (apr_ssize_t) (sizeof(struct in_addr))); /* get the sender state */
									} else {
										zlog_error(zc, "MulticastState does not exist, ignored. did[%ld]", ssock_p->mh.bcd_id.data_id);
										continue;
									}
									switch (ssock_p->mh.type) { /* process packet body */
									case MSG_PULL:
										assert(multi_p);
										zlog_debug(zc, "{%s} get MSG_PULL", bcd_id_buf_t_p);

										/* check if this is an existing push */
										zlog_info(zc, "{%s} is [%s] pushed", bcd_id_buf_t_p, sender_p ? "" : "NOT");
										if (!sender_p) { // receiver is not in the push
											zlog_debug(zc, "{%s} create MSG_PULL_REJECT", bcd_id_buf_t_p);
											if (!insertMsg2SendQ(ssock_p->sendQueue_p, &(ssock_p->mh.bcd_id), MSG_PULL_REJECT, NULL, 0, 0, 1)) {
												zlog_fatal(zc, "{%s} insert MSG_PULL_REJECT failed", bcd_id_buf_t_p);
												abort();
											} else {

											}
										}

//										multicastState_reset(multi_p);

//										if (!sender_p) { /* get sender state */
//											sender_p = createSenderState(multi_p->n_init++, &skey, &(multi_p->bcd_id), ssock_p);
//											zlog_debug(zc, "create SenderState did[%ld] ip[%s]", sender_p->bcd_id.data_id, inet_ntoa(sender_p->skey));
//											apr_hash_set(multi_p->hashSender, (const void *) (&(sender_p->skey)), (apr_ssize_t) (sizeof(struct in_addr)), (const void *) sender_p);
////											D("[new state] .xid[%d] .sin_port[%d] .s_addr[%d] .pad[%] sender_p[%p]", sender_p->key.xid, sender_p->key.sin_port,
////												sender_p->key.sin_addr.s_addr, sender_p->key.pad, sender_p);
//										} else {
//											zlog_error(zc, "SenderState has already existed. (receiver asks same data more than once)");
//											continue;
//										}
////										multi_p->alive = 1;
//										multi_p->bcd_state = INITIATED;
//										zlog_info(zc, "get MSG_INIT: did[%ld], (did[%ld], addr[%s], id[%d]), did[%ld]", multi_p->bcd_id.data_id, sender_p->bcd_id.data_id, inet_ntoa(sender_p->skey), sender_p->id, ssock_p->mh.bcd_id.data_id);
//										pthread_kill(pthread_NetmapR, SIGUSR1);
										break;

									case MSG_FIRST_SEQ:
										assert(multi_p);
										assert(sender_p);

										parseMsgSeqFirst(&(sender_p->msf), ssock_p->recv_buf + MSG_HDRLEN);

										if (!(sender_p->ssock_p)) {
											zlog_debug(zc, "assign SendSocketState to SenderState");
											sender_p->ssock_p = ssock_p;
										}

										if (multi_p->n_first_seq == 0) { // first FIRST_SEQ
											gettimeofday(&(multi_p->tv_first_first_seq), NULL);
										}
										if (multi_p->is_first_seq_distinct_rack == 0 && sender_p->is_distinct_rack) { // first FIRST_SEQ from distinct rack
											multi_p->is_first_seq_distinct_rack = 1;
											gettimeofday(&(multi_p->tv_first_first_seq_distinct_rack), NULL);
											pthread_kill(pthread_NetmapR, SIGUSR1); // notify data channel to start FULLRATE
										}
										if (multi_p->n_first_seq + 1 == multi_p->n_init) { // last FIRST_SEQ
											gettimeofday(&(multi_p->tv_last_first_seq), NULL);
											pthread_kill(pthread_NetmapR, SIGUSR1); // notify data channel to start FULLRATE
										}

										if (multi_p->seq_max == 1) { // for data having only 1 sequence
											if (multi_p->n_all_done == 0) {
												gettimeofday(&(multi_p->tv_first_all_done), NULL);
											}
											if (multi_p->is_all_done_distinct_rack == 0 && sender_p->is_distinct_rack) { // first FIRST_SEQ from distinct rack
												multi_p->is_all_done_distinct_rack = 1;
												gettimeofday(&(multi_p->tv_first_all_done_distinct_rack), NULL);
											}
											if (multi_p->n_all_done + 1 == multi_p->n_init) {
												gettimeofday(&(multi_p->tv_last_all_done), NULL);
												zlog_debug(zc, "{%s} create MSG_C_SEND2I_SEQ_ALL", bcd_id_buf_t_p);
												bytes_msg = createMsg(c_send2i_buf, &(multi_p->bcd_id), MSG_C_SEND2I_SEQ_ALL, NULL, MSG_HDRLEN, 0);
												bytes_ret = write(pipe_fd_c_send2i[1], c_send2i_buf, bytes_msg);
												if (bytes_ret != bytes_msg) {
													zlog_fatal(zc, "write() fails MSG_C_SEND2I_SEQ_ALL [%ld]/[%ld], did[%ld]", bytes_ret, bytes_msg, multi_p->bcd_id.data_id);
												}
											}
											multi_p->n_all_done++;
											// TODO: clean the sender state
//											apr_hash_set(multi_p->hashSender, (const void *) (&(sender_p->skey)), (apr_ssize_t) (sizeof(struct in_addr)), (const void *) (NULL));
										}
										multi_p->n_first_seq++; /* increase the number of initiated senders */
										multicastState_updateSeqEnd(multi_p, sender_p);

										zlog_info(zc, "{%s} get MSG_FIRST_SEQ[%d]/[%d] [%d]/[%d] from addr[%s] senderID[%d]", bcd_id_buf_t_p, sender_p->msf.seq_first, multi_p->seq_max, multi_p->n_first_seq, multi_p->n_init,
											inet_ntoa(sender_p->skey), sender_p->id);
										break;

									case MSG_ALL_DONE:
										assert(multi_p);
										assert(sender_p);
										assert(sender_p->has_all_done == 0);

										multi_p->n_all_done++; // increase MSG_ALL_DONE counter
										sender_p->has_all_done = 1; // set ALL_DONE flag of the sender
										zlog_debug(zc, "{%s} get MSG_ALL_DONE [%d]/[%d] addr[%s] senderID[%d]", bcd_id_buf_t_p, multi_p->n_all_done, multi_p->n_init, inet_ntoa(sender_p->skey), sender_p->id);

										//TODO: from the server out of the same rack of the sender
										if (multi_p->n_all_done == 1) {
											gettimeofday(&(multi_p->tv_first_all_done), NULL); // timestamp of the first MSG_ALL_DONE
										}
										if (multi_p->is_all_done_distinct_rack == 0 && sender_p->is_distinct_rack) { // first FIRST_SEQ from distinct rack
											multi_p->is_all_done_distinct_rack = 1;
											gettimeofday(&(multi_p->tv_first_all_done_distinct_rack), NULL);
										}
										if (multi_p->n_all_done == multi_p->n_init) { // last MSG_ALL_DONE
											gettimeofday(&(multi_p->tv_last_all_done), NULL);
											zlog_debug(zc, "{%s} create MSG_C_SEND2I_SEQ_ALL", bcd_id_buf_t_p);
											bytes_msg = createMsg(c_send2i_buf, &(multi_p->bcd_id), MSG_C_SEND2I_SEQ_ALL, NULL, MSG_HDRLEN, 0);
											bytes_ret = write(pipe_fd_c_send2i[1], c_send2i_buf, bytes_msg);
											if (bytes_ret != bytes_msg) {
												zlog_fatal(zc, "{%s} write() MSG_C_SEND2I_SEQ_ALL fails [%ld]/[%ld]", bcd_id_buf_t_p, bytes_ret, bytes_msg);
												abort();
											}
										}

										// TODO: clean the sender state
//										apr_hash_set(multi_p->hashSender, (const void *) (&(sender_p->skey)), (apr_ssize_t) (sizeof(struct in_addr)), (const void *) (NULL));
										//	deleteSenderState(sender_p);
//										printSenderHashmap();
//										D("[delete state] .xid[%d] .sin_port[%d] .s_addr[%d] .pad[%] sender_p[%p]", sender_p->key.xid, sender_p->key.sin_port,
//											sender_p->key.sin_addr, sender_p->key.pad, sender_p);
										break;

									case MSG_PATCH_REQ:
										assert(multi_p);
										assert(sender_p);

										PatchTuple pt, pt1, pt2;
										parseMsgPatchReq(ssock_p->recv_buf + MSG_HDRLEN, &pt);
										pt.stt_seq = pt.stt_seq % multi_p->seq_max; /* update the start seq */

										// if (pt.stt_seq == last_patch_start) {
										// 	last_patch_counter++;
										// 	zlog_warn(zc, "duplicate lose start[%d] counter[%d]", pt.stt_seq, last_patch_counter);
										// 	if (last_patch_counter >= 4) {
										// 		zlog_warn(zc, "more than 4");
										// 	}
										// } else {
										// 	last_patch_start = pt.stt_seq;
										// 	last_patch_counter = 0;
										// }

										if (pt.end_seq == SEQ2WARDTHEEND) { /* the receiver has problem, the sender should send the data packet until the end */
											pt.end_seq = sender_p->seqEnd % multi_p->seq_max; /* update end_seq */
//											zlog_info(zc, "get MSG_PATCH_REQ with SEQ2WARDTHEEND did[%ld] addr[%s], end_seq updated to[%d]", multi_p->bcd_id.data_id, inet_ntoa(sender_p->skey), pt.end_seq);
//											if (sender_p->msf.seq_first) { /* update the end seq, first packet is NOT 0 packet */
//												pt.end_seq = sender_p->msf.seq_first;
//											} else {
//												pt.end_seq = multi_p->seq_max;
//											}
										}
										zlog_debug(zc, "{%s} get MSG_PATCH_REQ from addr[%s] stt[%d]->end[%d] [%s]", bcd_id_buf_t_p, inet_ntoa(sender_p->skey), pt.stt_seq, pt.end_seq, pt.stt_seq < pt.end_seq ? "" : "larger than");
										if (pt.stt_seq < pt.end_seq) {
											ptre_p = createPatchTupleRingElement(&(ssock_p->mh.bcd_id), &pt, &(sender_p->tv_init));
											APR_RING_INSERT_HEAD(ssock_p->ringPatchTuple, ptre_p, _my_elem_t, link);
											multi_p->n_patch_total += (pt.end_seq - pt.stt_seq);
											zlog_debug(zc, "{%s} create PatchTuple stt[%d]->end[%d] [%p]", bcd_id_buf_t_p, ptre_p->pt.stt_seq, ptre_p->pt.end_seq, ptre_p);
										} else if (pt.stt_seq > pt.end_seq) { /* create the patchTupleElements when the seqs are wraped around */
											pt1 = pt2 = pt;
											pt1.stt_seq = 0;
											pt2.end_seq = multi_p->seq_max;

											ptre_p = createPatchTupleRingElement(&(ssock_p->mh.bcd_id), &pt1, &(sender_p->tv_init));
											APR_RING_INSERT_HEAD(ssock_p->ringPatchTuple, ptre_p, _my_elem_t, link);
											zlog_debug(zc, "{%s} create PatchTuple stt[%d]->end[%d] [%p]", bcd_id_buf_t_p, ptre_p->pt.stt_seq, ptre_p->pt.end_seq, ptre_p);

											ptre_p = createPatchTupleRingElement(&(ssock_p->mh.bcd_id), &pt2, &(sender_p->tv_init));
											APR_RING_INSERT_HEAD(ssock_p->ringPatchTuple, ptre_p, _my_elem_t, link);
											zlog_debug(zc, "{%s} create PatchTuple stt[%d]->end[%d] [%p]", bcd_id_buf_t_p, ptre_p->pt.stt_seq, ptre_p->pt.end_seq, ptre_p);

											multi_p->n_patch_total += ((pt1.end_seq - pt1.stt_seq) + (pt2.end_seq - pt2.stt_seq));
										} else {
											zlog_error(zc, "{%s} stt[%d] == end[%d]", bcd_id_buf_t_p, pt.stt_seq, pt.end_seq);
										}

										break;
									default:
										zlog_error(zc, "{%s} get UNSOLICITED[%d] from addr[%s] senderID[%d]", bcd_id_buf_t_p, ssock_p->mh.type, inet_ntoa(sender_p->skey), sender_p->id);
										break;
									}

									// reset the state
									ssock_p->recv_ed_byte -= ssock_p->recv_expect_byte;
									if (ssock_p->recv_ed_byte > 0) {
										memmove(ssock_p->recv_buf, ssock_p->recv_buf + ssock_p->recv_expect_byte, ssock_p->recv_ed_byte);
									}
									ssock_p->recv_expect_byte = BYTE_MAX;
									ssock_p->mh.type = MSG_NONTYPE;
								} else {
									break;
								}
							}
						} else if (count == 0) {
							zlog_info(zc, "Client closed connection. Client IP address is: %s", inet_ntoa(ssock_p->client_addr.sin_addr));
							close(ssock_p->socket);
							// TODO: delete ssock
							List_remove(ssockList, Vs);
						} else {
							if (errno == EAGAIN || errno == EINTR) {
								break;
							} else {
								/* something else is wrong */
								zlog_error(zc, "errno[%d] not ignoreable", errno);
								perror("error receiving from a client");
								close(ssock_p->socket);
								// TODO:
								List_remove(ssockList, Vs);

							}
						}
					}
				}
			}
		} else {
			if (errno == EINTR || errno == EINVAL) {
			} else {
				// TODO: gracefully process select fail
				zlog_error(zc, "errno[%d] not ignoreable", errno);
				perror("select failed");
				abort();
			}
		}

	}
}
