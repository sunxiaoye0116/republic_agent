/*
 * service_thread.h
 *
 *  Created on: Aug 29, 2016
 *      Author: xs6
 */

#ifndef SRC_THREAD_SERVICE_H_
#define SRC_THREAD_SERVICE_H_
#include <assert.h>
#include <zlog.h>
#ifndef NETMAP_WITH_LIBS
#define NETMAP_WITH_LIBS
#endif
#include "net/netmap_user.h"
#include "global.h"
#include "hero.h"

#define NETWORK_CONTROLLER_ADDR 	"[RESEARCH_NETWORK_PREFIX].152"
#define NETWORK_CONTROLLER_PORT 	(10880)

void *thread_domainSocket(void *arg);

#endif /* SRC_THREAD_SERVICE_H_ */
