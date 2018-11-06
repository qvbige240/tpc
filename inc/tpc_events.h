/**
 * History:
 * ================================================================
 * 2018-10-09 qing.zou created
 *
 */
#ifndef TPC_EVENTS_H
#define TPC_EVENTS_H

#include <sys/queue.h>		/* tailq */

#include "tpc_minheap.h"

TPC_BEGIN_DELS

#define TPC_HAVE_CLOCK_GETTIME
//#define CLOCK_MONOTONIC

/** Indicates that a timeout has occurred. */
#define TPC_EV_TIMEOUT			0x01
/** Wait for FD to become readable */
#define TPC_EV_READ				0x02
/** Wait for FD to become writable */
#define TPC_EV_WRITE			0x04
/** Wait for a POSIX signal to be raised */
#define TPC_EV_SIGNAL			0x08
/** Wait for a custom notice to be raised */
#define TPC_EV_NOTICE			0x40
/**
 * Persistent event: won't get removed automatically when activated.
 *
 * When a persistent event with a timeout becomes activated, its timeout
 * is reset to 0.
 */
#define TPC_EV_PERSIST			0x10
/** Select edge-triggered behavior, if supported by the backend. */
#define TPC_EV_ET				0x20

typedef void (*tpc_event_callback)(int fd, short events, void *args);

struct tpc_evbase_t;
typedef struct tpc_evbase_t tpc_evbase_t;

TPCAPI int tpc_event_assign(tpc_events *ev, tpc_evbase_t *base, int fd, short events, tpc_event_callback callback, void *arg);
TPCAPI int tpc_event_add(tpc_events *ev, const struct timeval *tv);
TPCAPI int tpc_event_del(tpc_events *ev);
TPCAPI tpc_events *tpc_event_new(tpc_evbase_t *base, int fd, short events, tpc_event_callback callback, void *arg);
TPCAPI void tpc_event_free(tpc_events *ev);
TPCAPI char *tpc_event_data_get(tpc_events *ev);
TPCAPI tpc_evbase_t* tpc_evbase_create(void);
TPCAPI int tpc_evbase_loop(tpc_evbase_t* thiz, int flags);
TPCAPI void tpc_evbase_destroy(tpc_evbase_t* thiz);

//TPCAPI int tpc_evmsg_notice(int key);
TPCAPI int tpc_evmsg_notice(tpc_evbase_t *base, int key, void *data, int size);

TPC_END_DELS

#endif // TPC_EVENTS_H
