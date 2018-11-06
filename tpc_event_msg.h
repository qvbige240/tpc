/**
 * History:
 * ================================================================
 * 2018-10-16 qing.zou created
 *
 */

#ifndef TPC_EVENT_MSG_H
#define TPC_EVENT_MSG_H

//#include <sys/queue.h>		/* tailq */

#include "tpc_minheap.h"

TPC_BEGIN_DELS

// #define TPC_EVENT_NOTICE_MAX_NUM	TPC_KEY_EVENT_MAX_NUM
#define TPC_EVENT_NOTICE_MAX_NUM	10


struct tpc_notice_info {
	tpc_events	ev_notice;
	int			ev_notice_pair[2];		/* socketpair used to send notifications */
	int			ev_notice_added;
	int			ev_notice_added_cnt;

	void		*priv;
};

//int tpc_evmsg_notice(int key);
int tpc_evmsg_notice(tpc_evbase_t *base, int key, void *data, int size);

int tpc_evmsg_init(tpc_evbase_t *base);
void tpc_evmsg_dealloc(tpc_evbase_t *base);

#ifndef TPC_EVENT_DISABLE_THREAD_SUPPORT
int tpc_evmsg_global_setup_locks(tpc_evbase_t *base, const int enable_locks);
void tpc_evmsg_global_free_locks(tpc_evbase_t *base);
#endif

TPC_END_DELS

#endif // TPC_EVENT_MSG_H
