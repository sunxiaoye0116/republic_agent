/*
 * thread_recv.c
 *
 *  Created on: Aug 29, 2016
 *      Author: xs6
 */

#include "thread_recv.h"

void *thread_recvSocket(void *arg) {
	zlog_info(zc, "ctrl channel (recv) thread started");
	setaffinity(pthread_RecvSocket, ctrl_channel_affinity % num_cores);
	zlog_info(zc, "set ctrl channel (recv) thread to core [%d]/[%d]", ctrl_channel_affinity % num_cores, num_cores);

	fd_set read_set, write_set; /* variables for select */
	int sockMax, ret;
	struct timeval tv_select_timeout = { .tv_sec = SELECT_TIMEOUT_SEC, .tv_usec = SELECT_TIMEOUT_USEC };
	struct timeval tv_st;

	ssize_t count;

	DataHdr dh;
	BcdMetadata bm;

	MsgSeqFirst msf;
	MsgIntQ miq;
	PatchTuple mpr;

	BcdID *bcd_id_t_p;
	char bcd_id_buf_t_p[100];

	ssize_t bytes_ret, bytes_msg;

	while (1) {
		/* set up the file descriptor bit map that select should be watching */
		FD_ZERO(&read_set); /* clear everything */
		FD_ZERO(&write_set); /* clear everything */
		sockMax = 0;

		{
			LIST_FOREACH_DOUBLYLINKEDLIST(rsockList, first, next, Cc, Cn)
				FdState * fds_p = (FdState *) (Cc->value);

				/* set read_set */
				FD_SET(fds_p->fd, &read_set);

				if (fds_p->sendQueue_p)
					/* set write_set */
					if (apr_queue_size(fds_p->sendQueue_p) > 0) {
						FD_SET(fds_p->fd, &write_set);
					}

				/* set max */
				if (sockMax < fds_p->fd) {
					sockMax = fds_p->fd;
				}
			}
		}

		tv_st = tv_select_timeout;
		/* invoke select, make sure to pass max+1 !!! */

		ret = select(sockMax + 1, &read_set, &write_set, NULL, &tv_st);
		if (ret == 0) { /* no descriptor ready, timeout happened */
		} else if (ret > 0) { /* at least one file descriptor is ready */
			{
				LIST_FOREACH_DOUBLYLINKEDLIST(rsockList, first, next, Ccc, Cn)
					FdState * fds_p = (FdState *) (Ccc->value);

					/* send message */
					if (FD_ISSET(fds_p->fd, &write_set)) {
						sendMsg(fds_p->fd, fds_p->sendQueue_p, &(fds_p->remainMsgToken_p));
					}

					/* read message */
					if (FD_ISSET(fds_p->fd, &read_set)) { /* we have data from a client */
						count = read(fds_p->fd, fds_p->recv_buf + fds_p->recv_ed_byte, RECVBUF_LEN - fds_p->recv_ed_byte);
						if (count > 0) { /* get bytes */
//							zlog_debug(zc, "recv returns [%ld]", count);
							fds_p->recv_ed_byte += count;
							while (fds_p->recv_ed_byte > 0) { /* process the received bytes */
								if (fds_p->recv_ed_byte >= MSG_HDRLEN && fds_p->mh.type == MSG_NONTYPE) { /* parse header */
									parseMsgHdr(&(fds_p->mh), fds_p->recv_buf);
									struct timeval tv_msgdelivertime_2, tv_msgdelivertime_3;
									gettimeofday(&tv_msgdelivertime_2, NULL);
									timersub(&tv_msgdelivertime_2, &(fds_p->mh.tv), &tv_msgdelivertime_3);
									float msgdelivertime_diff_ms = tv_msgdelivertime_3.tv_sec * 1000.0 + tv_msgdelivertime_3.tv_usec / 1000.0;
//									if (msgdelivertime_diff_ms > .1 || msgdelivertime_diff_ms < -1) {
//										D("message deliver time [%.3f]ms, type[%d]", msgdelivertime_diff_ms, fds_p->mh.type);
//									}
									fds_p->recv_expect_byte = fds_p->mh.len;
//									zlog_debug(zc, "get MsgHdr from fd[%d]: msb[%016lx], lsb[%016lx], did[%ld], type[%d], len[%d]", fds_p->fd_type, fds_p->mh.bcd_id.app_id.msb, fds_p->mh.bcd_id.app_id.lsb, fds_p->mh.bcd_id.data_id,
//										fds_p->mh.type, fds_p->mh.len);
								}

								if (fds_p->recv_ed_byte >= fds_p->recv_expect_byte) { /* got a complete message */
									RecverState *recv_p = (RecverState *) apr_hash_get(hashRecver, (const void *) (&(fds_p->mh.bcd_id)), sizeof(BcdID));

									/* log output id */
									bcd_id_t_p = &(fds_p->mh.bcd_id);
									BcdId2str(bcd_id_t_p, bcd_id_buf_t_p);

									switch (fds_p->fd_type) {
									case (CTRL_RECV_TCP): {
										switch (fds_p->mh.type) {
										case MSG_PULL_REJECT:
											zlog_info(zc, "get MSG_PULL_REJECT did[%ld]", fds_p->mh.bcd_id.data_id);

											zlog_debug(zc, "create MSG_C2I_PULL_REJECT, did[%ld]", fds_p->mh.bcd_id.data_id);
											bytes_msg = createMsg(c2i_buf, &(fds_p->mh.bcd_id), MSG_C2I_PULL_REJECT, NULL, MSG_HDRLEN, 0);
											bytes_ret = write(pipe_fd_c2i[1], c2i_buf, bytes_msg);
											assert(bytes_ret == bytes_msg);

											break;
										case MSG_PATCH_DATA:
											parseDataHdr(&dh, fds_p->recv_buf + MSG_HDRLEN);

											databyte_t offset;
											// zlog_debug(zc, "get MSG_PATCH_DATA did[%ld] seq[%d] fast[%d] slow[%d] max[%d]", fds_p->mh.bcd_id.data_id, dh.seq_num, recv_p->num_fast_recv_ed, recv_p->num_slow_recv_ed, recv_p->seq_max);
											databyte_t file_offset;
											if (recv_p->seq_tail > 0 && dh.seq_num > recv_p->seq_tail) { /* the data should be written to the tail buffer */
												offset = (dh.seq_num - recv_p->seq_tail - 1) * recv_p->byte_payload;
												file_offset = getFilePosition(dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload, recv_p->fileMetadata.start);

												if (offset + (recv_p->seq_tail + 1) * recv_p->byte_payload != file_offset) {
													zlog_error(zc, "[%ld]vs[%ld], cur[%d], first[%d], tail[%d], size[%d], start[%ld]", offset, file_offset, dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload,
														recv_p->fileMetadata.start);
												}

												zlog_debug(zc, "write to position in buffer[%ld]", offset);
												memcpy(recv_p->tail_patch_buffer + offset, fds_p->recv_buf + MSG_HDRLEN + DATA_HDRLEN, dh.len);

											} else {
												if (dh.seq_num == recv_p->seq_fst) {
													zlog_error(zc, "get MSG_PATCH_DATA out of range, did[%ld], seq[%d], len[%d], dh.seq[%d], seq_fst[%d], payload_size_max[%d]", dh.bcd_id.data_id, dh.seq_num, dh.len, dh.seq_num,
														recv_p->seq_fst, recv_p->byte_payload);
													break;
												}

												if (dh.seq_num == 0) { // update information
													zlog_info(zc, "get zero data from slowpath, seq_max position[%ld]", MSG_HDRLEN + DATA_HDRLEN + dh.len);
													parseBcdMetadata(&bm, fds_p->recv_buf + MSG_HDRLEN + DATA_HDRLEN + dh.len);
													recverState_updateBcdMetadata(recv_p, &bm);
													assert((recv_p->fileMetadata).start >= 0); // TODO should be removed
												}
												offset = getFilePosition(dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload, recv_p->fileMetadata.start);
//												if (dh.seq_num > recv_p->seq_fst) {
//													offset = (dh.seq_num - recv_p->seq_fst) * recv_p->byte_payload;
//												} else if (dh.seq_num < recv_p->seq_fst) {
//													offset = (dh.seq_num * recv_p->byte_payload) + (recv_p->fileMetadata).start;
//												} else {
//													zlog_error(zc, "get MSG_PATCH_DATA out of range, did[%ld], seq[%d], len[%d], dh.seq[%d], seq_fst[%d], payload_size_max[%d]", dh.bcd_id.data_id, dh.seq_num, dh.len, dh.seq_num,
//														recv_p->seq_fst, recv_p->byte_payload);
//												}
//												if (offset != file_offset) {
//													zlog_error(zc, "[%ld]vs[%ld], cur[%d], first[%d], tail[%d], size[%d], start[%ld]", offset, file_offset, dh.seq_num, recv_p->seq_fst, recv_p->seq_tail, recv_p->byte_payload,
//														recv_p->fileMetadata.start);
//												}

												int fp = fseek(recv_p->fd_patch_p, offset, SEEK_SET);
												if (fp < 0) {
													zlog_error(zc, "fseek returns %d", fp);
													perror("fseek() returns -1");
												}
												fwrite(fds_p->recv_buf + MSG_HDRLEN + DATA_HDRLEN, sizeof(char), dh.len, recv_p->fd_patch_p);
											}
											recv_p->num_slow_recv_ed++;

											if (recverState_hasGottenAllData(recv_p)) {
												recverState_finishRecv(recv_p);

												zlog_debug(zc, "create MSG_C2I_SEQ_ALL, did[%ld]", fds_p->mh.bcd_id.data_id);
												bytes_msg = createMsg(c2i_buf, &(recv_p->bcd_id), MSG_C2I_SEQ_ALL, NULL, MSG_HDRLEN, 0);
												bytes_ret = write(pipe_fd_c2i[1], c2i_buf, bytes_msg);
												if (bytes_ret != bytes_msg) {
													zlog_fatal(zc, "write() fails MSG_C2I_SEQ_ALL [%ld]/[%ld], did[%ld]", bytes_ret, bytes_msg, fds_p->mh.bcd_id.data_id);
												}

												if (!insertMsg2SendQ(recv_p->fds_p->sendQueue_p, &(fds_p->mh.bcd_id), MSG_ALL_DONE, NULL, 0, 0, 1)) {
													zlog_fatal(zc, "insert MSG_ALL_DONE fail, did[%ld]", fds_p->mh.bcd_id.data_id);
													abort();
												}
											}
											break;
										case MSG_FAST_DONE:
											zlog_info(zc, "get MSG_FAST_DONE did[%ld]", recv_p->bcd_id.data_id);
											while (recv_p->lock_processingFastData) { /* wait until last fast path data finishes */
												usleep(1);
											}
											if (recv_p->is_finished) {
												zlog_info(zc, "finished receiving, ignored");
												break;
											}
											recverState_stopFathPath(recv_p);

											while (recv_p->lock_processingFastData) { /* wait until last fast path data finishes */
												usleep(1);
											}
											PatchTuple *pt_p = NULL;
											if (recv_p->is_seq_zero) { /* recver knows seqEnd */
												if ((recv_p->seq_lst + 1) % (recv_p->seq_max) != recv_p->seq_fst) { /* have not recv the last data packet */
													/* create MSG_PATCH_DATA */
													pt_p = createPatchTuple(recv_p->seq_lst + 1, recv_p->seq_fst);
												} else {
													zlog_info(zc, "got last seq");
												}
											} else { /* recver does not know seqEnd */
												/* create MSG_PATCH_DATA */
												pt_p = createPatchTuple(recv_p->seq_lst + 1, SEQ2WARDTHEEND);
											}

											if (pt_p) {
												zlog_warn(zc, "FAST_DONE packet loss [%d, %d], did[%ld]", pt_p->stt_seq, pt_p->end_seq, recv_p->bcd_id.data_id);
												if (!insertMsg2SendQ(fds_p->sendQueue_p, &(recv_p->bcd_id), MSG_PATCH_REQ, (void *) pt_p, sizeof(PatchTuple), 0, 1)) {
													zlog_fatal(zc, "create MSG_PATCH_REQ fail");
													abort();
												}
												deletePatchTuple(pt_p);
											}
											break;
										default:
											zlog_warn(zc, "UNSOLICITED msg from fd[%d]: did[%ld], type[%d]", fds_p->fd_type, fds_p->mh.bcd_id.data_id, fds_p->mh.type);
											break;

										}
										break;
									}
									case (PIPE_I2C_RECV): {
										switch (fds_p->mh.type) {
										case MSG_I2C_PULL:
											zlog_debug(zc, "get MSG_I2C_PULL");
											miq = *(MsgIntQ *) (fds_p->recv_buf + MSG_HDRLEN);

											FdState * fds_p2 = NULL;
											/* get the recvSocketState for master IP */
											{ /* TODO: get the recv socket from hashmap */
												LIST_FOREACH_DOUBLYLINKEDLIST(rsockList, first, next, Cc, Cn)
													FdState *_fds_p = (FdState *) (Cc->value);
													if (_fds_p->fd_type == CTRL_RECV_TCP) {
														if (!strcmp(miq.master_addr, ((RecvSocketState *) (_fds_p->state_p))->ip_address)) {
															fds_p2 = _fds_p;
															zlog_debug(zc, "found RecvSocketState having the same IP address: %s", ((RecvSocketState * ) (_fds_p->state_p))->ip_address);
															break;
														}
													}
												}
											}

											if (!fds_p2) {
												// TODO: ip address and nic name should comes from the local socket msg parameter
												zlog_warn(zc, "create RecvSocketState for ip[%s], nic[%s]", miq.master_addr, ctrl_nic_name);
												fds_p2 = createRecvSocketState(miq.master_addr, ctrl_nic_name);
												List_push(rsockList, fds_p2);
											}

											/* tell sender agent MSG_PULL */
											if (!insertMsg2SendQ(fds_p2->sendQueue_p, &(fds_p->mh.bcd_id), MSG_PULL, NULL, 0, 0, 1)) {
												zlog_fatal(zc, "insert MSG_PULL fail, did[%ld]", fds_p->mh.bcd_id.data_id);
												abort();
											}
											break;
										}
										break;
									}
									case (PIPE_N2C_RECV): {
										switch (fds_p->mh.type) {
										case MSG_N2C_SEQ_FIRST:
											zlog_debug(zc, "get MSG_N2C_SEQ_FIRST");

											/* tell sender MSG_FIRST_SEQ */
											msf = *(MsgSeqFirst *) (fds_p->recv_buf + MSG_HDRLEN);
											if (!insertMsg2SendQ(recv_p->fds_p->sendQueue_p, &(fds_p->mh.bcd_id), MSG_FIRST_SEQ, (void *) &msf, sizeof(MsgSeqFirst), 0, 1)) {
												zlog_fatal(zc, "insert MSG_FIRST_SEQ fail, did[%ld]", fds_p->mh.bcd_id.data_id);
												abort();
											}

											/* tell service iface MSG_C2I_SEQ_ALL */
											if (recv_p->seq_max == 1) {
												zlog_debug(zc, "create MSG_C2I_SEQ_ALL, did[%ld]", fds_p->mh.bcd_id.data_id);
												bytes_msg = createMsg(c2i_buf, &(fds_p->mh.bcd_id), MSG_C2I_SEQ_ALL, NULL, MSG_HDRLEN, 0);
												bytes_ret = write(pipe_fd_c2i[1], c2i_buf, bytes_msg);
												if (bytes_ret != bytes_msg) {
													zlog_fatal(zc, "write() fails MSG_C2I_SEQ_ALL [%ld]/[%ld], did[%ld]", bytes_ret, bytes_msg, fds_p->mh.bcd_id.data_id);
												}
											}
											break;
										case MSG_N2C_SEQ_ZERO: /* single seq data won't has this */
											zlog_debug(zc, "get MSG_N2C_SEQ_ZERO");
											assert(recv_p->seq_max != 1);

											if (seq_zero_notify) {
												/* tell service iface MSG_C2I_SEQ_ZERO */
												zlog_debug(zc, "create MSG_C2I_SEQ_ZERO, did[%ld]", fds_p->mh.bcd_id.data_id);
												bytes_msg = createMsg(c2i_buf, &(fds_p->mh.bcd_id), MSG_C2I_SEQ_ZERO, NULL, MSG_HDRLEN, 0);
												bytes_ret = write(pipe_fd_c2i[1], c2i_buf, bytes_msg);
												if (bytes_ret != bytes_msg) {
													zlog_fatal(zc, "write() fails MSG_C2I_SEQ_ZERO [%ld]/[%ld], did[%ld]", bytes_ret, bytes_msg, fds_p->mh.bcd_id.data_id);
												}
											}
											break;
										case MSG_N2C_SEQ_ALL: /* single seq data won't has this */
											zlog_debug(zc, "get MSG_N2C_SEQ_ALL");
											assert(recv_p->seq_max != 1);

											/* tell service iface MSG_C2I_SEQ_ALL */
											zlog_debug(zc, "create MSG_C2I_SEQ_ALL, did[%ld]", fds_p->mh.bcd_id.data_id);
											bytes_msg = createMsg(c2i_buf, &(fds_p->mh.bcd_id), MSG_C2I_SEQ_ALL, NULL, MSG_HDRLEN, 0);
											bytes_ret = write(pipe_fd_c2i[1], c2i_buf, bytes_msg);
											if (bytes_ret != bytes_msg) {
												zlog_fatal(zc, "write() fails MSG_C2I_SEQ_ALL [%ld]/[%ld], did[%ld]", bytes_ret, bytes_msg, fds_p->mh.bcd_id.data_id);
											}

											/* tell sender MSG_ALL_DONE, if ... */
											if (!insertMsg2SendQ(recv_p->fds_p->sendQueue_p, &(fds_p->mh.bcd_id), MSG_ALL_DONE, NULL, 0, 0, 1)) {
												zlog_fatal(zc, "insert MSG_ALL_DONE fail, did[%ld]", fds_p->mh.bcd_id.data_id);
												abort();
											}
											break;
										case MSG_N2C_SEQ_JUMP:
											zlog_debug(zc, "get MSG_N2C_SEQ_JUMP");

											// TODO: tell iface if first_seq_notify
											mpr = *(PatchTuple *) (fds_p->recv_buf + MSG_HDRLEN);
											if (!insertMsg2SendQ(recv_p->fds_p->sendQueue_p, &(fds_p->mh.bcd_id), MSG_PATCH_REQ, (void *) &mpr, sizeof(PatchTuple), 0, 1)) {
												zlog_fatal(zc, "insert MSG_PATCH_REQ fail, did[%ld]", fds_p->mh.bcd_id.data_id);
												abort();
											}
											break;
										default:
											zlog_warn(zc, "UNSOLICITED msg from fd[%d]: did[%ld], type[%d]", fds_p->fd_type, fds_p->mh.bcd_id.data_id, fds_p->mh.type);
											break;
										}
										break;
									}
									default:
										zlog_warn(zc, "UNSOLICITED fd from fd[%d]: did[%ld], type[%d]", fds_p->fd_type, fds_p->mh.bcd_id.data_id, fds_p->mh.type);
										break;
									}

									/* reset buf state */
									fds_p->recv_ed_byte -= fds_p->recv_expect_byte;

									if (fds_p->recv_ed_byte > 0) {
										memmove(fds_p->recv_buf, fds_p->recv_buf + fds_p->recv_expect_byte, fds_p->recv_ed_byte);
									}
									fds_p->recv_expect_byte = BYTE_MAX;
									fds_p->mh.type = MSG_NONTYPE;

								} else { /* message incomplete */
									break;
								}
							}
						} else if (count == 0) { /* connection is closed, clean up */
							zlog_info(zc, "Connection closed: [%s]", inet_ntoa(((RecvSocketState * )(fds_p->state_p))->server_addr.sin_addr));
							deleteRecvSocketState(List_remove(rsockList, Ccc));
						} else {
							if (errno == EAGAIN || errno == EINTR) {
								break;
							} else {
								perror("read() fails");
								zlog_error(zc, "errno[%d] not ignoreable", errno);
								deleteRecvSocketState(List_remove(rsockList, Ccc));
								abort();
							}
						}
					}
				}
			}
		} else {
			if (errno == EINTR || errno == EINVAL) {
			} else {
				// TODO: gracefully process select fail
				perror("select() fails");
				zlog_error(zc, "errno[%d] not ignoreable", errno);
				abort();
			}
		}

	}
}
