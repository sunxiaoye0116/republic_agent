/*
 * sys_util.h
 *
 *  Created on: Feb 11, 2016
 *      Author: xs6
 */

#ifndef SRC_SYS_UTIL_H_
#define SRC_SYS_UTIL_H_

#define _GNU_SOURCE		/* See feature_test_macros(7) */
#include <stdio.h> /* printf */
#include <stdlib.h> /* printf */
#include <string.h> /* printf */
#include <sched.h>		/* cpuset_t */
#include <pthread.h>	/* system_ncpus */
#include <unistd.h>		/* sysconf */
#include <errno.h>		/* errno */
#include <assert.h>		/* errno */

#include <sys/time.h>

/* get number of cpus */
int system_ncpus(void);

/* set thread affinity using pthread */
void setaffinity(pthread_t me, int i);

void setscheduler(pid_t pid, int policy, int priority);

/* get milli-second time */
void gettimeofmillisecond(char *buf_p, size_t len, struct timeval *tv_time);

#endif /* SRC_SYS_UTIL_H_ */
