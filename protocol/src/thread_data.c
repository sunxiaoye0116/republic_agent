/*
 * thread_data.c
 *
 *  Created on: Aug 29, 2016
 *      Author: xs6
 */

#include "thread_data.h"

/*
 * move up to 'limit' pkts from rxring to txring swapping buffers.
 */
static pktnum_t process_rings(struct netmap_ring *rxring, struct netmap_ring *txring, uint32_t limit, uint8_t dirc) {
	pktnum_t rx_pktnum, tx_pktnum, mi, max_pktnum, nonmulti_pktnum = 0; /* number of packet forward to/from kernel stack */

	u_int j, k;

	j = rxring->cur; /* RX */
	k = txring->cur; /* TX */

	/* get m, which is the number of packet actually processed in this function */
	max_pktnum = limit;
	rx_pktnum = nm_ring_space(rxring); /* # new packets in the rx ring */
	if (rx_pktnum < max_pktnum) {
		max_pktnum = rx_pktnum;
	}
	tx_pktnum = nm_ring_space(txring); /* # empty slots in the tx ring */
	if (tx_pktnum < max_pktnum) {
		max_pktnum = tx_pktnum;
	}
	mi = max_pktnum;

	static uint8_t seq_error_last_iter = 0;

	ssize_t bytes_ret, bytes_msg;
	MsgSeqFirst msf;
	BcdMetadata bm;
	DataHdr dh;
	RecverState *recv_p;
	AppState *app_p;
	FdState *fds_p;

	while (mi-- > 0) {
		struct netmap_slot *rs = &rxring->slot[j];
		char *p = NETMAP_BUF(rxring, rs->buf_idx); /* the buffer associate to the slot */

		struct ethhdr *eth_p = (struct ethhdr *) (p); /* ethernet header */
		struct iphdr *ip_p = (struct iphdr *) (((char *) eth_p) + ETH_HLEN); /* ip header */
		struct udphdr *udp_p = (struct udphdr *) (((char *) ip_p) + IP4_HDRLEN); /* udp header */

//		u_short ip_sum_ori = ntohs(ip_p->ip_sum);
//		ip_p->ip_sum = 0;
//		ip_p->ip_sum = computeIPchecksum((uint16_t *) ip_p, IP4_HDRLEN);
//		if (ip_p->ip_sum != ip_sum_ori) {
//			D("[warn] packet corrupted...[%x] vs [%x]", ip_sum_ori, ip_p->ip_sum);
//		}

//		if (msg == NET2HOST) {
//			u_short ip_sum_ori = ip_p->ip_sum;
//			ip_p->ip_sum = 0;
//			ip_p->ip_sum = computeIPchecksum((uint16_t *) ip_p, IP4_HDRLEN);
//			if (ip_p->ip_sum != ip_sum_ori) {
//				D("[warn] packet corrupted...");
//			}
//		}

		if (ip_p->protocol == IPPROTO_UDP && // udp packet, ip_p
			ntohs(udp_p->dest) == DATA_DST_PORT && // fastpath packet
			ntohs(eth_p->h_proto) == ETH_P_IP && // ip packet
			dirc == NM_N2H // recv packet
		) { /* packets from the fast path */
			parseDataHdr(&dh, ((char *) udp_p) + UDP_HDRLEN);
			recv_p = (RecverState *) apr_hash_get(hashRecver, &(dh.bcd_id), sizeof(BcdID)); /* msgIntQ has received, master has connected */
			app_p = (AppState *) apr_hash_get(hashApp, &(dh.bcd_id.app_id), sizeof(AppID)); /* msgIntQ has received */

//			if (ip_p->check && validate_ip_checksum((struct iphdr *) ip_p) != 0) { /* ip checksum */
//				zlog_warn(zc, "get incorrect ip checksum");
//			}
//
//			uint16_t udpCheck = udp_p->check;
//			compute_udp_checksum(ip_p, (unsigned short*) udp_p);
//			if (udp_p->check != udpCheck) {
//				zlog_warn(zc, "get incorrect udp checksum");
//				abort();
//			}

			/* TODO the correct logic here should be like this. this requires the lsock are in a hashmap with BcdID as the key */
//			if RecverStat does not exist && any of the lsock for this data has been created
//				create RecverState
//				assign the RecvSocketState to RecverState
//			if RecverState exists
//				process packet
			/* create RecverState if does not exist */
			if (!recv_p && app_p) { /* RecverState has not been created for the incoming data, i.e., this is the first data packet, but AppState has created */

				/* get RecvSocketState for the master IP */
				struct in_addr ia = { .s_addr = ip_p->saddr };
				char source_ip[24];
				strcpy(source_ip, inet_ntoa(ia));

//				D("looking for RecvSocketState for IP %s", inet_ntoa(ia));
				fds_p = NULL;
				{ /* TODO: get the recv socket from hashmap */
					LIST_FOREACH_DOUBLYLINKEDLIST(rsockList, first, next, Cc, Cn)
						FdState *_fds_p = (FdState *) (Cc->value);
						if (_fds_p->fd_type == CTRL_RECV_TCP) {
							if (!strcmp(source_ip, ((RecvSocketState *) (_fds_p->state_p))->ip_address)) {
								fds_p = _fds_p;
								zlog_debug(zc, "found RecvSocketState having the same IP address: %s", ((RecvSocketState * ) (_fds_p->state_p))->ip_address);
								break;
							}
						}
					}
				}

				if (fds_p) {
					/* create RecverState for the bcd */
					// zlog_info(zc, "get data packet having no RecverState did[%ld]", dh.bcd_id.data_id);
					recv_p = createRecverState(&(dh.bcd_id));
					if (recv_p) {
						apr_hash_set(hashRecver, (const void *) (&(recv_p->bcd_id)), sizeof(BcdID), (const void *) recv_p);

						/* assign the RecvSocketState to RecverState */
						recv_p->fds_p = fds_p;

						/* enable fast path receiving */
						recverState_startFathPath(recv_p);
					} else {
					}

				} else {
				}
			}

			if (recv_p) { /* RecverState exists */
				if (recv_p->fds_p) { /* RecverState has RecvSocketState, i.e., can talk to master about first message and package loss */
					recv_p->lock_processingFastData = 1;
					if (recv_p->is_fastpath) { /* RecverState is active, i.e., should accept data from fast path, i.e., should process data from fast path */
						uint8_t seq_first = 0;
						uint8_t seq_zero = 0;
						uint8_t seq_all = 0;
						uint8_t seq_jump = 0;
						uint8_t seq_error = 0;
//						int64_t lseek_ret;
						databyte_t extend_db;
						PatchTuple * pt_p = NULL;

						if (!recv_p->is_seq_fst) { /* get the first seq of the data, init/set the corresponding states */
							recv_p->seq_lst = (dh.seq_num) ? (dh.seq_num - 1) : SEQNUM_MAX; /* last seq */
							recv_p->seq_fst = dh.seq_num; /* first seq */
							zlog_info(zc, "got the first data seq [%d]", recv_p->seq_fst);
							seq_first = 1; /* mark the state */
							recv_p->is_seq_fst = 1; /* update the state */
						}

						if (dh.seq_num != 0 && dh.seq_num - (recv_p->seq_lst) == 1) { /* [no-loss] got a subsequent seq (not 0 packet) */
							fwrite(p + DATA_PAYLOAD_OFFSET, sizeof(char), dh.len, recv_p->fd_p);
							recv_p->num_fast_recv_ed++;
						} else if (dh.seq_num == 0) { /* got 0 packet */
							zlog_info(zc, "get seq zero from fastpath, seq_max position [%ld]", DATA_PAYLOAD_OFFSET + dh.len);
							parseBcdMetadata(&bm, p + DATA_PAYLOAD_OFFSET + dh.len);
							if (recverState_updateBcdMetadata(recv_p, &bm) >= 0) {
								if (recv_p->seq_lst == recv_p->seq_max - 1 || recv_p->seq_lst == SEQNUM_MAX) { /* [no-loss] got seq zero */
									/* write data */
									fwrite(p + DATA_PAYLOAD_OFFSET, sizeof(char), dh.len, recv_p->fd_p);
									recv_p->num_fast_recv_ed++;
									seq_zero = 1; /* mark the state */
								} else if (recv_p->seq_lst > recv_p->seq_fst && recv_p->seq_fst != 0) { /* [loss] got seq zero, loss from seq_lst to seq_max */
									/* put place holder */
									extend_db = (databyte_t) (recv_p->seq_max - recv_p->seq_lst - 2) * recv_p->byte_payload + recv_p->byte_payload_min;
									assert(extendFile(&(recv_p->fd_p), extend_db) == extend_db);
									//								for (seq_i = recv_p->seq_lst + 1; seq_i < recv_p->seq_max - 1; seq_i++) {
									//									fwrite(zeros, sizeof(char), recv_p->byte_payload, recv_p->fd_p);
									//								}
									//								fwrite(zeros, sizeof(char), recv_p->byte_payload_min, recv_p->fd_p);
									//								if (ftell(recv_p->fd_p) != getFilePosition(dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload, recv_p->fileMetadata.start)) {
									//									zlog_error(zc, "[%ld]vs[%ld], cur[%d], first[%d], tail[%d], size[%d], start[%ld], lst[%d], lseek[%ld]", ftell(recv_p->fd_p),
									//										getFilePosition(dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload, recv_p->fileMetadata.start), dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload,
									//										recv_p->fileMetadata.start, recv_p->seq_lst, lseek_ret);
									//								}
									/* write data */
									fwrite(p + DATA_PAYLOAD_OFFSET, sizeof(char), dh.len, recv_p->fd_p);
									recv_p->num_fast_recv_ed++;
									pt_p = createPatchTuple(recv_p->seq_lst + 1, recv_p->seq_max);
									seq_zero = 1; /* mark the state */
									seq_jump = 1;
								} else { /* other situation, turn off fast path */
									seq_error = 1;
								}
							} else {
								seq_error = 1;
							}
						} else if (dh.seq_num > recv_p->seq_lst) { /* [loss] not wrap around */
							if (recv_p->seq_fst > recv_p->seq_lst && recv_p->seq_fst > dh.seq_num) { /* ([lst -> cur] -> fst) [loss] first half packet loss */
								/* put place holder */
								extend_db = (databyte_t) (dh.seq_num - recv_p->seq_lst - 1) * recv_p->byte_payload;
								assert(extendFile(&(recv_p->fd_p), extend_db) == extend_db);
//								assert(lseek_ret = lseek(fileno(recv_p->fd_p), (databyte_t) (dh.seq_num - recv_p->seq_lst - 1) * recv_p->byte_payload, SEEK_CUR));
//								for (seq_i = recv_p->seq_lst + 1; seq_i < dh.seq_num; seq_i++) {
//									fwrite(zeros, sizeof(char), recv_p->byte_payload, recv_p->fd_p);
//								}
//								if (ftell(recv_p->fd_p) != getFilePosition(dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload, recv_p->fileMetadata.start)) {
//									zlog_error(zc, "[%ld]vs[%ld], cur[%d], first[%d], tail[%d], size[%d], start[%ld], lst[%d], lseek[%ld]", ftell(recv_p->fd_p),
//										getFilePosition(dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload, recv_p->fileMetadata.start), dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload,
//										recv_p->fileMetadata.start, recv_p->seq_lst, lseek_ret);
//								}
								/* write data */
								fwrite((p + DATA_PAYLOAD_OFFSET), sizeof(char), dh.len, recv_p->fd_p);
								recv_p->num_fast_recv_ed++;
								pt_p = createPatchTuple(recv_p->seq_lst + 1, dh.seq_num);
								seq_jump = 1;
							} else if (recv_p->seq_lst >= recv_p->seq_fst) { /* (fst -> [lst -> cur]) [loss] second half packet loss */
								/* put place holder */
								extend_db = (databyte_t) (dh.seq_num - recv_p->seq_lst - 1) * recv_p->byte_payload;
								assert(extendFile(&(recv_p->fd_p), extend_db) == extend_db);
//								assert(lseek_ret = lseek(fileno(recv_p->fd_p), (databyte_t) (dh.seq_num - recv_p->seq_lst - 1) * recv_p->byte_payload, SEEK_CUR));
//								for (seq_i = recv_p->seq_lst + 1; seq_i < dh.seq_num; seq_i++) {
//									fwrite(zeros, sizeof(char), recv_p->byte_payload, recv_p->fd_p);
//								}
//								if (ftell(recv_p->fd_p) != getFilePosition(dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload, recv_p->fileMetadata.start)) {
//									zlog_error(zc, "[%ld]vs[%ld], cur[%d], first[%d], tail[%d], size[%d], start[%ld], lst[%d], lseek[%ld]", ftell(recv_p->fd_p),
//										getFilePosition(dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload, recv_p->fileMetadata.start), dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload,
//										recv_p->fileMetadata.start, recv_p->seq_lst, lseek_ret);
//								}
								/* write data */
								fwrite(p + DATA_PAYLOAD_OFFSET, sizeof(char), dh.len, recv_p->fd_p);
								recv_p->num_fast_recv_ed++;
								pt_p = createPatchTuple(recv_p->seq_lst + 1, dh.seq_num);
								seq_jump = 1;
							} else {
								seq_error = 1;
							}
						} else if (dh.seq_num < recv_p->seq_fst && recv_p->seq_fst < recv_p->seq_lst) { /* (cur] -> fst -> [lst) [loss] packet loss happens at the end of the second half and the beginning of the first half */
							assert(recv_p->seq_tail == 0);
							assert(recv_p->fileMetadata.start == -1);
							recv_p->seq_tail = recv_p->seq_lst; /* update tail seq */
							recv_p->fileMetadata.start = ftell(recv_p->fd_p);
							zlog_info(zc, "set seq_tail[%d], set file_start[%ld]", recv_p->seq_tail, recv_p->fileMetadata.start);

							/* put place holder */
							extend_db = (databyte_t) (dh.seq_num) * recv_p->byte_payload;
							assert(extendFile(&(recv_p->fd_p), extend_db) == extend_db);
//							assert(lseek_ret = lseek(fileno(recv_p->fd_p), (databyte_t) (dh.seq_num) * recv_p->byte_payload, SEEK_CUR));
//							for (seq_i = 0; seq_i < dh.seq_num; seq_i++) {
//								fwrite(zeros, sizeof(char), recv_p->byte_payload, recv_p->fd_p);
//							}
//							if (ftell(recv_p->fd_p) != getFilePosition(dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload, recv_p->fileMetadata.start)) {
//								zlog_error(zc, "[%ld]vs[%ld], cur[%d], first[%d], tail[%d], size[%d], start[%ld], lst[%d], lseek[%ld]", ftell(recv_p->fd_p),
//									getFilePosition(dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload, recv_p->fileMetadata.start), dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload,
//									recv_p->fileMetadata.start, recv_p->seq_lst, lseek_ret);
//							}
							/* write data */
							fwrite(p + DATA_PAYLOAD_OFFSET, sizeof(char), dh.len, recv_p->fd_p);
							recv_p->num_fast_recv_ed++;
							pt_p = createPatchTuple(recv_p->seq_lst + 1, dh.seq_num);
							seq_jump = 1;
						} else {
							seq_error = 1;
						}

						if (seq_error_last_iter) {
							zlog_warn(zc, "check previous error, seq_fst[%d],seq_lst[%d],seq_cur[%d]", recv_p->seq_fst, recv_p->seq_lst, dh.seq_num);
//							zlog_warn(zc,
//								"check previous error, seq_fst[%d],seq_lst[%d],lst[%d](sidx[%d],bidx[%d],mi[%d],lmove[%d],pre_bidx[%d],nxt_bidx[%d]),cur[%d](sidx[%d],bidx[%d],mi[%d],lmove[%d],pre_bidx[%d],nxt_bidx[%d])",
//								recv_p->seq_fst, recv_p->seq_lst, recv_p->dh_lst.seq_num, recv_p->dh_lst.f1, recv_p->dh_lst.f2, recv_p->dh_lst.f3, recv_p->dh_lst.f4, recv_p->dh_lst.f5, recv_p->dh_lst.f6, dh.seq_num, dh.f1, dh.f2, dh.f3,
//								dh.f4, dh.f5, dh.f6);
							seq_error_last_iter = 0;
						}
						if (seq_error) {
							// recverState_stopFathPath(recv_p);
							zlog_warn(zc, "check current error, seq_fst[%d],seq_lst[%d],seq_cur[%d]", recv_p->seq_fst, recv_p->seq_lst, dh.seq_num);
//							zlog_warn(zc,
//								"check current error, seq_fst[%d],seq_lst[%d],lst[%d](sidx[%d],bidx[%d],mi[%d],lmove[%d],pre_bidx[%d],nxt_bidx[%d]),cur[%d](sidx[%d],bidx[%d],mi[%d],lmove[%d],pre_bidx[%d],nxt_bidx[%d])",
//								recv_p->seq_fst, recv_p->seq_lst, recv_p->dh_lst.seq_num, recv_p->dh_lst.f1, recv_p->dh_lst.f2, recv_p->dh_lst.f3, recv_p->dh_lst.f4, recv_p->dh_lst.f5, recv_p->dh_lst.f6, dh.seq_num, dh.f1, dh.f2, dh.f3,
//								dh.f4, dh.f5, dh.f6);
							seq_error_last_iter = 1;
						} else {
							recv_p->seq_lst = dh.seq_num;
//							recv_p->dh_lst = dh;
						}

						if (((recv_p->seq_fst != 0) && ((dh.seq_num + 1) == recv_p->seq_fst)) || ((recv_p->seq_fst == 0) && ((dh.seq_num + 1) == recv_p->seq_max))) { /* get the last optical data */
							zlog_info(zc, "get last seq from fastpath [%d]", dh.seq_num);
							recverState_stopFathPath(recv_p);

							/* record the jump position */
							recv_p->fileMetadata.jump = ftell(recv_p->fd_p);
							zlog_info(zc, "jump [%ld]", recv_p->fileMetadata.jump);

							if (recverState_hasGottenAllData(recv_p)) {
								seq_all = 1;
								recverState_finishRecv(recv_p);
							}
						}

						/* send command to ctrl thread */
						bytes_t buf_bytes = 0;
						if (seq_first) { /* send seq first */
							msf.seq_first = htonl(dh.seq_num);
							zlog_debug(zc, "create MSG_N2C_SEQ_FIRST, seq[%d], did[%ld]", dh.seq_num, dh.bcd_id.data_id);
							bytes_msg = createMsg(n2c_buf + buf_bytes, &(recv_p->bcd_id), MSG_N2C_SEQ_FIRST, (char *) &msf, MSG_HDRLEN + sizeof(MsgSeqFirst), 0);
							buf_bytes += bytes_msg;
						}

						if (seq_zero && recv_p->seq_max > 1) {
							zlog_debug(zc, "create MSG_N2C_SEQ_ZERO, seq[%d], did[%ld]", dh.seq_num, dh.bcd_id.data_id);
							bytes_msg = createMsg(n2c_buf + buf_bytes, &(recv_p->bcd_id), MSG_N2C_SEQ_ZERO, NULL, MSG_HDRLEN, 0);
							buf_bytes += bytes_msg;
						}

						if (seq_all && recv_p->seq_max > 1) {
							zlog_debug(zc, "create MSG_N2C_SEQ_ALL, did[%ld]", recv_p->bcd_id.data_id);
							bytes_msg = createMsg(n2c_buf + buf_bytes, &(recv_p->bcd_id), MSG_N2C_SEQ_ALL, NULL, MSG_HDRLEN, 0);
							buf_bytes += bytes_msg;
						}

						if (seq_jump) {
							zlog_warn(zc, "create MSG_N2C_SEQ_JUMP, packet loss [%d, %d], did[%ld]", pt_p->stt_seq, pt_p->end_seq, recv_p->bcd_id.data_id);
							bytes_msg = createMsg(n2c_buf + buf_bytes, &(recv_p->bcd_id), MSG_N2C_SEQ_JUMP, (char *) pt_p, MSG_HDRLEN + sizeof(PatchTuple), 0);
							buf_bytes += bytes_msg;
							deletePatchTuple(pt_p);
						}
						if (buf_bytes) {
							bytes_ret = write(pipe_fd_n2c[1], n2c_buf, buf_bytes);
							if (bytes_ret != buf_bytes) {
								zlog_fatal(zc, "write() fails, did[%ld]", dh.bcd_id.data_id);
							}
						}
					} else { /* fast path turned off */
					}
					recv_p->lock_processingFastData = 0;
				} else { /* master's RecvSocketState does not exist */
				}
			} else { /* recverState does not exist, discarded */
			}
		} else {
			struct netmap_slot *ts = &txring->slot[k];

			ts->len = rs->len;
			uint32_t buf_idx = ts->buf_idx; /* zero copy */
			ts->buf_idx = rs->buf_idx;
			rs->buf_idx = buf_idx;
			ts->flags |= NS_BUF_CHANGED; /* report the buffer change. */
			rs->flags |= NS_BUF_CHANGED;

			k = nm_ring_next(txring, k);
			nonmulti_pktnum++;
		}
		j = nm_ring_next(rxring, j);
	}
	rxring->head = rxring->cur = j;
	txring->head = txring->cur = k;
	return nonmulti_pktnum;
}

uint8_t mx_ring_empty(MulticastState *multi_p) { // if there is anything to sent in the current active multicast
	return multi_p == NULL || multi_p->bcd_state == BCDSTATE_DEAD;
}

//int am_ring_empty(MulticastState *multi_p) { /* return if the multicast is alive */
//	if (!multi_p)
//		return 0x0fffffff;
//	return multi_p->bcd_state == BCDSTATE_DEAD;
//}

pktnum_t am_n_pkt_queued(MulticastState *multi_p) {
	if (mx_ring_empty(multi_p))
		return 0;

	/* update multicast state */
	if (multi_p->bcd_state == BCDSTATE_INITIATED && multi_p->n_packet_total == 0 && multi_p->n_byte_total == 0) {
		multicastState_updateRate(multi_p, multi_p->std_rate_f_delta / attempt_scale);
		multi_p->bcd_state = BCDSTATE_ATTEMPT;
	}

	if (multi_p->bcd_state == BCDSTATE_ATTEMPT
		&& ((first_first_seq_fullrate && ((multi_p->is_distinct_rack && multi_p->is_first_seq_distinct_rack) || (!(multi_p->is_distinct_rack) && multi_p->n_first_seq)))
			|| ((!first_first_seq_fullrate) && (multi_p->n_init == multi_p->n_first_seq)))) { /* upgrade to fullrate */
		multicastState_updateRate(multi_p, multi_p->std_rate_f_delta);
		gettimeofday(&(multi_p->tv_attempt_finish), NULL);
		multi_p->bcd_state = BCDSTATE_FULLRATE;
	}

	if (multi_p->bcd_state == BCDSTATE_FULLRATE && multi_p->seq_cur >= multi_p->seq_end && multi_p->n_init == multi_p->n_first_seq) {/* mark and will upgrade to dead after netmap send the last packet to NIC */
		if (!(multi_p->is_tv_last_data & 0x10)) { // not terminated
			// zlog_debug(zc, "multicast transfer terminate in am_n_pkt_queued(), did[%ld] head[%d] tail[%d]", multi_p->bcd_id.data_id, am_txring_p->head, am_txring_p->tail);
			am_txring_head = unlikely(am_txring_p->head == 0) ? am_txring_p->num_slots - 1 : am_txring_p->head - 1;
			am_txring_tail_last = am_txring_p->tail;
			multi_p->is_tv_last_data |= 0x14;
			need_release_path = 1;
		}
		return 0;
	}

	if (multi_p->is_tv_last_data & 0x10) { /* fullrate has terminated, but still in fullrate, last packet has been put in the netmap ring */
		return 0;
	}

	struct timeval tv_cur, tv_diff;
	gettimeofday(&tv_cur, NULL);
	timersub(&tv_cur, &(multi_p->tv_sendStart), &tv_diff);
	long int diff_us = tv_diff.tv_sec * 1000 * 1000 + tv_diff.tv_usec; // diff_us micro-seconds has passed since the most recent rate rectification

	/* how many packets can be sent by applying the rate limit (this is just a estimation since payload size varies)*/
	// pktnum_t ret = (pktnum_t) ((((diff_us * 1.0) / (multi_p->rate_f_delta)) - 1.0 * multi_p->n_byte) / (multi_p->byte_payload + DATA_PAYLOAD_OFFSET));
	pktnum_t ret = (pktnum_t) ceil(((((diff_us * 1.0) / (multi_p->rate_f_delta))) / (multi_p->byte_payload + DATA_PAYLOAD_OFFSET)) - multi_p->n_packet);
	if (ret < 0) {
		return 0;
	}

	/* how many packets need to be sent */
	if (multi_p->seq_end != SEQNUM_MAX) {
		pktnum_t n_max = multi_p->seq_end - multi_p->seq_cur;
		ret = ret > n_max ? n_max : ret;
	}

//	if (ret == 0 && multi_p->n_packet_total == 0 && multi_p->bcd_state == BCDSTATE_ATTEMPT) {
//		ret = 1;
//	}

	return ret;
}

/*
 * move up to 'limit' pkts from multicast machine[si] to txring swapping buffers.
 */
static pktnum_t am_process_rings(MulticastState *multi_p, struct netmap_ring *txring, pktnum_t max_pktnum) {
	pktnum_t mi, rx_pktnum, tx_pktnum, actual_pktnum; /* number of packet limits, final limit, receive limit, transmit limit, updated limit */
	databyte_t byte_i = 0; /* total bytes sent through the function call */

	/* print a warning if any of the ring flags is set (e.g. NM_REINIT) */
//	if (txring->flags)
//		D("txflags %x", txring->flags);
// get the pointers in the rings
//	j = multi_p->seq_cur; /* RX */
//	k = txring->cur; /* TX */
// set the number of packet to send
	/* record the time when first data is sent */
	if (multi_p->n_packet_total == 0 && multi_p->n_byte_total == 0) {
		gettimeofday(&(multi_p->tv_first_data), NULL);
	}

	actual_pktnum = max_pktnum;
	// rx_pktnum = am_n_pkt_queued(multi_p);
	// if (rx_pktnum < actual_pktnum) {
	// 	actual_pktnum = rx_pktnum;
	// }
	tx_pktnum = nm_ring_space(txring);
	if (tx_pktnum < actual_pktnum) {
		actual_pktnum = tx_pktnum;
	}

	mi = actual_pktnum;
	while (mi-- > 0) {
		struct netmap_slot *ts = &txring->slot[txring->cur];

		char *txbuf = NETMAP_BUF(txring, ts->buf_idx);
		struct iphdr *ip_p = (struct iphdr *) (txbuf + ETH_HLEN); /* set the pointer for the ethernet, ip, udp headers */
		struct udphdr *udp_p = (struct udphdr *) (((char *) ip_p) + IP4_HDRLEN);
		char *payload_p = ((char *) udp_p) + UDP_HDRLEN; /* udp payload */

//		uint32_t pre_slot_idx = unlikely(txring->cur == 0) ? txring->num_slots - 1 : txring->cur - 1;

		bytes_t payloadBytes = createData(payload_p, multi_p, multi_p->fd, (multi_p->seq_cur % multi_p->seq_max));		//, txring->cur, ts->buf_idx, mi, lst_move, (txring->slot[pre_slot_idx]).ptr,
//			(txring->slot[nm_ring_next(txring, txring->cur)]).ptr);

		/* fill the body of the packets */
		memcpy(txbuf, multi_p->eth_hdr_p, ETH_HLEN); /* ethernet header */

		memcpy(ip_p, &(multi_p->ip_hdr), sizeof(struct iphdr)); /* ip header */
		ip_p->tot_len = htons(IP4_HDRLEN + UDP_HDRLEN + payloadBytes); //ip_len
		compute_ip_checksum((struct iphdr *) ip_p);
//		ip_p->ip_sum = 0;
//		ip_p->ip_sum = htons(computeIPchecksum((uint16_t *) ip_p, IP4_HDRLEN));

		memcpy(udp_p, &(multi_p->udp_hdr), sizeof(struct udphdr)); /* udp header */
		udp_p->len = htons(UDP_HDRLEN + payloadBytes);
//		compute_udp_checksum((struct iphdr *) ip_p, (unsigned short *) udp_p);
		udp_p->check = 0;

		if (!(multi_p->seq_cur % multi_p->seq_max)) {
			if (multi_p->data_size > 1024 * 1024)
				zlog_info(zc, "metadata seq_cur[%d], datasize[%ld], payloadBytes[%d] seq_end[%d] n_init[%d] n_first_seq[%d]", multi_p->seq_cur, be64toh(((BcdMetadata *) (payload_p + payloadBytes - BCDMETADATA_LEN))->data_size),
					payloadBytes, multi_p->seq_end, multi_p->n_init, multi_p->n_first_seq);
		}

		bytes_t bytes = ETH_HLEN + IP4_HDRLEN + UDP_HDRLEN + payloadBytes; /* # bytes to send */

		ts->len = bytes; /* set the packet length */
		ts->flags = 0;

		byte_i += bytes;
		txring->head = txring->cur = nm_ring_next(txring, txring->cur);
		multi_p->seq_cur += 1;
	}

	if (multi_p->seq_cur >= multi_p->seq_end_max) { // in case the sender will found that the transfer should terminate after it receives a FIRST_SEQ
		multi_p->is_tv_last_data |= 0x01;
		am_txring_head = unlikely(txring->head == 0) ? txring->num_slots - 1 : txring->head - 1;
		am_txring_tail_last = txring->tail;
	}
	if ((multi_p->seq_cur >= multi_p->seq_end) && (multi_p->n_init == multi_p->n_first_seq)) { /* known seq end has been transmitted AND seq end has been updated for all the receivers. */
		if (!(multi_p->is_tv_last_data & 0x10)) { // not terminated
			// zlog_debug(zc, "multicast transfer terminate in am_process_rings(), did[%ld] head[%d] tail[%d]", multi_p->bcd_id.data_id, txring->head, txring->tail);
			am_txring_head = unlikely(txring->head == 0) ? txring->num_slots - 1 : txring->head - 1;
			am_txring_tail_last = txring->tail;
			multi_p->is_tv_last_data |= 0x12;
			need_release_path = 1;
		}
	}

	/* update statistics */
	multi_p->n_byte += byte_i;
	multi_p->n_packet += actual_pktnum;
	multi_p->n_byte_total += byte_i;
	multi_p->n_packet_total += actual_pktnum;
	if (multi_p->bcd_state == BCDSTATE_ATTEMPT) {
		multi_p->n_byte_attempt += byte_i;
		multi_p->n_packet_attempt += actual_pktnum;
	}
	return actual_pktnum;
}

static pktnum_t am_move(MulticastState *multi_p, struct nm_desc *dst, pktnum_t limit) {
	if (mx_ring_empty(multi_p))
		return 0;
	struct netmap_ring *txring = NETMAP_TXRING(dst->nifp, dst->last_tx_ring);
	if (nm_ring_empty(txring))
		return 0;
	return am_process_rings(multi_p, txring, limit);
}

static u_int move(struct nm_desc *src, struct nm_desc *dst, uint32_t limit, uint8_t dirc) {
	struct netmap_ring *txring, *rxring;
	u_int m = 0, si = src->first_rx_ring, di = dst->first_tx_ring;
	int i;
	i = 0;
	while (si <= src->last_rx_ring && di <= dst->last_tx_ring) {
		assert(i == 0); // only one pair of nic/host ring
		i++;
		rxring = NETMAP_RXRING(src->nifp, si);
		txring = NETMAP_TXRING(dst->nifp, di);
		if (nm_ring_empty(rxring)) {
			si++;
			continue;
		}
		if (nm_ring_empty(txring)) {
			di++;
			continue;
		}
		m += process_rings(rxring, txring, limit, dirc);
		si++;
		di++;
	}

	return (m);
}

void *thread_netmap(void *arg) {
	zlog_info(zc, "data channel thread started");
	setaffinity(pthread_NetmapR, data_channel_affinity % num_cores);
	zlog_info(zc, "set data channel thread to core[%d]/[%d]", data_channel_affinity % num_cores, num_cores);
	setscheduler(0, data_channel_scheduling_policy, data_channel_scheduling_priority);
	zlog_info(zc, "set data channel thread to policy[%d] priority[%d]", data_channel_scheduling_policy, data_channel_scheduling_priority);

	NetmapArg *n_arg = (NetmapArg *) arg;
	struct targ * targ = &(n_arg->t);

	/* poll fd */
	struct pollfd pfd_nic_host[2];
	memset(pfd_nic_host, 0, sizeof(pfd_nic_host));
	pfd_nic_host[0].fd = targ->fd;  // nic pfd
	pfd_nic_host[1].fd = targ->fd_host; // host stack pfd
	struct nm_desc *nmd_nic = targ->nmd;  // nic nmd
	struct nm_desc *nmd_host = targ->nmd_host;  // host stack nmd
	uint8_t move_flip = 0;

	am_txring_p = NETMAP_TXRING(nmd_nic->nifp, nmd_nic->last_tx_ring);

	while (!do_abort) { /* move packet between host and nic */
//		int n_nic, n_host;
		int ret;
		pfd_nic_host[0].events = pfd_nic_host[1].events = 0;
		pfd_nic_host[0].revents = pfd_nic_host[1].revents = 0;

		int n_nic_rx = pkt_queued(nmd_nic, 0);
//		int n_host_tx = pkt_queued(nmd_host, 1);
		int n_host_rx = pkt_queued(nmd_host, 0);
//		int n_nic_tx = pkt_queued(nmd_nic, 1);

		/* from sender */
//		int n_multi_rx = !am_ring_empty(activeMulticast_p);
		pktnum_t n_multi_rx = am_n_pkt_queued(activeMulticast_p);
		uint8_t is_mx_ring_empty = mx_ring_empty(activeMulticast_p);
//		if (n_host_tx) {
//			ioctl(pfd_nic_host[1].fd, NIOCTXSYNC, NULL);
//		}
//		if (n_nic_tx) {
//			ioctl(pfd_nic_host[0].fd, NIOCTXSYNC, NULL);
//		}

		if (n_nic_rx) {
			pfd_nic_host[1].events |= POLLOUT;
		} else {
			pfd_nic_host[0].events |= POLLIN;
		}
		// pfd_nic_host[1].events |= POLLOUT;

		if (n_host_rx) {
			pfd_nic_host[0].events |= POLLOUT;
		} else {
			pfd_nic_host[1].events |= POLLIN;
		}
		// pfd_nic_host[0].events |= POLLOUT;

		/* from sender */
		if (!is_mx_ring_empty) {
			pfd_nic_host[0].events |= POLLOUT;
		} else {

		}

		int timeout = (!is_mx_ring_empty || need_release_path) ? 1 : POLL_TIMEOUT_MSEC;
		ret = poll(pfd_nic_host, 2, timeout);
		if (ret > 0) {
			if (pfd_nic_host[0].revents & POLLOUT) { // host -> NIC

				if (n_multi_rx && (move_flip || (!n_host_rx))) {
					am_move(activeMulticast_p, nmd_nic, n_multi_rx > nm_burst ? nm_burst : n_multi_rx); // when busy looping, the program always goes to here
				} else if (n_host_rx) {
					lst_move = move(nmd_host, nmd_nic, nm_burst, NM_H2N);
				} else {
				}
				move_flip = ~move_flip;
			}
			if (pfd_nic_host[1].revents & POLLOUT) { // NIC -> host
				if (n_nic_rx) {
					move(nmd_nic, nmd_host, nm_burst, NM_N2H);
				} else {
				}
			}
		} else if (ret == 0) { // poll timeout
		} else {
			if (errno == EINTR || errno == EINVAL) { // received interrupt
			} else { // other errors
				zlog_error(zc, "errno [%d]", errno);
			}
		}

		ioctl(pfd_nic_host[0].fd, NIOCTXSYNC, NULL); // release the nic tx slots. this is required since the poll() does not move tail to head unless the ring is full.

		if (need_release_path) {
			zlog_debug(zc, "tail_last[%d], head_last[%d], head[%d], tail[%d], num_slot[%d], limit[%d], space[%d]", am_txring_tail_last, am_txring_head, am_txring_p->head, am_txring_p->tail, am_txring_p->num_slots, nm_burst,
				nm_ring_space(am_txring_p));
			uint32_t dist_ht = am_txring_p->tail >= am_txring_head ? am_txring_p->tail - am_txring_head : am_txring_p->tail + am_txring_p->num_slots - am_txring_head;
			uint32_t dist_tt = am_txring_tail_last > am_txring_p->tail ? am_txring_tail_last - am_txring_p->tail : am_txring_tail_last + am_txring_p->num_slots - am_txring_p->tail;
			if (dist_ht + dist_tt <= am_txring_p->num_slots) {
				assert(activeMulticast_p);
				activeMulticast_p->bcd_state = BCDSTATE_DEAD;
				gettimeofday(&(activeMulticast_p->tv_last_data), NULL);
				zlog_debug(zc, "upgrade did[%ld] BCDSTATE_DEAD", activeMulticast_p->bcd_id.data_id);
				need_release_path = 0;
				pthread_kill(pthread_LocalSocket, SIGUSR1);
			} else {
				am_txring_tail_last = am_txring_p->tail;
			}
		}
	}

	pthread_exit(&n_arg->retval);
	return NULL;
}
