/**
 * History:
 * ================================================================
 * 2018-10-09 qing.zou created
 *
 */
#ifndef TPC_UTIL_H
#define TPC_UTIL_H

#include "tpc_typedef.h"

TPC_BEGIN_DELS

/** Replacement for offsetof on platforms that don't define it. */
#ifdef offsetof
	#define tpc_offsetof(type, field) offsetof(type, field)
#else
	#define tpc_offsetof(type, field) ((off_t)(&((type *)0)->field))
#endif

#define TPC_EVUTIL_UPCAST(ptr, type, field)				\
	((type *)(((char*)(ptr)) - tpc_offsetof(type, field)))

#ifdef TPC_HAVE_TIMERADD
#define tpc_timeradd(tvp, uvp, vvp) timeradd((tvp), (uvp), (vvp))
#define tpc_timersub(tvp, uvp, vvp) timersub((tvp), (uvp), (vvp))
#define tpc_timerclear(tvp) timerclear(tvp)
#else
#define tpc_timeradd(tvp, uvp, vvp)					\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;       \
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (0)
#define	tpc_timersub(tvp, uvp, vvp)					\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)
#define	tpc_timerclear(tvp)	(tvp)->tv_sec = (tvp)->tv_usec = 0
#endif /* !TPC_HAVE_TIMERADD */


/** 
 * Return true if the tvp is related to uvp according to the relational
 * operator cmp.  Recognized values for cmp are ==, <=, <, >=, and >. 
 */
#define	tpc_timercmp(tvp, uvp, cmp)					\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?				\
	 ((tvp)->tv_usec cmp (uvp)->tv_usec) :				\
	 ((tvp)->tv_sec cmp (uvp)->tv_sec))

#define TPC_HAVE_GETTIMEOFDAY
#ifdef TPC_HAVE_GETTIMEOFDAY
  #define tpc_gettimeofday(tv, tz)  gettimeofday((tv), (tz))
#else
  struct timezone;
  TPCAPI int tpc_gettimeofday(struct timeval *tv, struct timezone *tz);
#endif


TPCAPI int tpc_socket_closeonexec(int fd);
TPCAPI int tpc_socket_nonblocking(int fd);

TPC_END_DELS

#endif //TPC_UTIL_H
