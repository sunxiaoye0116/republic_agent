/*
 * sys_util.c
 *
 *  Created on: Feb 11, 2016
 *      Author: xs6
 */

#include "sys_util.h"

/* sysctl wrapper to return the number of active CPUs */
int system_ncpus(void) {
	int ncpus;
#if defined (__FreeBSD__)
	int mib[2] = {CTL_HW, HW_NCPU};
	size_t len = sizeof(mib);
	sysctl(mib, 2, &ncpus, &len, NULL, 0);
#elif defined(linux)
	ncpus = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_WIN32)
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		ncpus = sysinfo.dwNumberOfProcessors;
	}
#else /* others */
	ncpus = 1;
#endif /* others */
	return (ncpus);
}

/* set the thread affinity. */
void setaffinity(pthread_t thread, int core_idx) {
	assert(core_idx >= 0);

	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(core_idx, &cpu_set);
	if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpu_set)) {
		perror("pthread_setaffinity_np");
		exit(EXIT_FAILURE);
	}
}

void setscheduler(pid_t pid, int policy, int priority) {
	struct sched_param sp;
	sp.__sched_priority = priority;

	if (sched_setscheduler(pid, policy, &sp)) {
		perror("sched_setscheduler");
		exit(EXIT_FAILURE);
	}
}

void gettimeofmillisecond(char *buf_p, size_t len, struct timeval *tv_time) {
	time_t t_time;
	size_t s_len;
	struct tm *tm_time_p;

	t_time = tv_time->tv_sec;
	tm_time_p = localtime(&t_time);
	s_len = strftime(buf_p, len, "%Y/%m/%d %H:%M:%S", tm_time_p);
	snprintf(buf_p + s_len, len - s_len, ".%03ld (%03ld)", tv_time->tv_usec / 1000, tv_time->tv_usec % 1000);

	return;
}
