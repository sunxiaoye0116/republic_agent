/*
 * thread_recv.h
 *
 *  Created on: Aug 29, 2016
 *      Author: xs6
 */

#ifndef SRC_THREAD_RECV_H_
#define SRC_THREAD_RECV_H_
#include <assert.h>
#include <zlog.h>
#ifndef NETMAP_WITH_LIBS
#define NETMAP_WITH_LIBS
#endif
#include "net/netmap_user.h"
#include "global.h"
#include "hero.h"

void *thread_recvSocket(void *arg);

#endif /* SRC_THREAD_RECV_H_ */
