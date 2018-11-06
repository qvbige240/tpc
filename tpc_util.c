/**
 * History:
 * ================================================================
 * 2018-10-09 qing.zou created
 *
 */

#include <fcntl.h>

#include "tpc_util.h"

#ifndef TPC_HAVE_GETTIMEOFDAY
int tpc_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	struct _timeb tb;

	if (tv == NULL)
		return -1;

	_ftime(&tb);
	tv->tv_sec  = (long) tb.time;
	tv->tv_usec = ((int) tb.millitm) * 1000;
	return 0;
}
#endif

int tpc_socket_closeonexec(int fd)
{
	int flags;
	if ((flags = fcntl(fd, F_GETFD, NULL)) < 0) {
		printf("fcntl(%d, F_GETFD)", fd);
		return -1;
	}
	if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
		printf("fcntl(%d, F_SETFD)", fd);
		return -1;
	}

	return 0;
}

int tpc_socket_nonblocking(int fd)
{
	int flags;
	if ((flags = fcntl(fd, F_GETFL, NULL)) < 0) {
		printf("fcntl(%d, F_GETFL)", fd);
		return -1;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		printf("fcntl(%d, F_GETFL)", fd);
		return -1;
	}

	return 0;
}
