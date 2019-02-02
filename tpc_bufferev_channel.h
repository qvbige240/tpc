/**
 * History:
 * ================================================================
 * 2019-1-23 qing.zou created
 *
 */
#ifndef TPC_BUFFEREV_CHANNEL_H
#define TPC_BUFFEREV_CHANNEL_H

#include "tpc_util.h"
#include "tpc_bufferev.h"
#include "tpc_event_thread.h"

TPC_BEGIN_DELS

#define TPC_CHANNEL_PACKAGE_SIZE		1400

#define TPC_BEV_UPCAST(b)	TPC_EVUTIL_UPCAST((b), struct tpc_bufferev_private, bev)

#ifdef TPC_EVENT_DISABLE_THREAD_SUPPORT
	#define TPC_BEV_LOCK(b)		((void)0)
	#define TPC_BEV_UNLOCK(b)	((void)0)
#else
	/** Internal: Grab the lock (if any) on a bufferev */
	#define TPC_BEV_LOCK(b) do {						\
		struct tpc_bufferev_private *locking =  TPC_BEV_UPCAST(b);	\
		TPC_EVLOCK_LOCK(locking->lock, 0);				\
	} while (0)

	/** Internal: Release the lock (if any) on a bufferev */
	#define TPC_BEV_UNLOCK(b) do {						\
		struct tpc_bufferev_private *locking =  TPC_BEV_UPCAST(b);	\
		TPC_EVLOCK_UNLOCK(locking->lock, 0);			\
	} while (0)
#endif


struct tpc_bufferev_private {
	tpc_bufferev_t		bev;

	unsigned			own_lock : 1;

	unsigned			readcb_pending : 1;
	unsigned			writecb_pending : 1;
	unsigned			connecting : 1;

	unsigned short		write_suspended;

	int					options;
	int					refcnt;

	void				*lock;
};

struct tpc_bufferev_ops {
	/** The name of the bufferevent's type. */
	const char *type;
	/** At what offset into the implementation type will we find a
	    bufferevent structure?

	    Example: if the type is implemented as
	    struct bufferevent_x {
	       int extra_data;
	       tpc_bufferev_t bev;
	    }
	    then mem_offset should be offsetof(struct bufferevent_x, bev)
	*/
	off_t mem_offset;

	/** Enables one or more of EV_READ|EV_WRITE on a bufferevent.  Does
	    not need to adjust the 'enabled' field.  Returns 0 on success, -1
	    on failure.
	 */
	int (*enable)(tpc_bufferev_t *, short);

	/** Disables one or more of EV_READ|EV_WRITE on a bufferevent.  Does
	    not need to adjust the 'enabled' field.  Returns 0 on success, -1
	    on failure.
	 */
	int (*disable)(tpc_bufferev_t *, short);

	/** Detatches the bufferevent from related data structures. Called as
	 * soon as its reference count reaches 0. */
	void (*unlink)(tpc_bufferev_t *);

	/** Free any storage and deallocate any extra data or structures used
	    in this implementation. Called when the bufferevent is
	    finalized.
	 */
	void (*destruct)(tpc_bufferev_t *);

	///** Called when the timeouts on the bufferevent have changed.*/
	//int (*adj_timeouts)(tpc_bufferev_t *);

	///** Called to flush data. */
	//int (*flush)(tpc_bufferev_t *, short, enum bufferevent_flush_mode);

	///** Called to access miscellaneous fields. */
	//int (*ctrl)(tpc_bufferev_t *, enum bufferevent_ctrl_op, union bufferevent_ctrl_data *);

};

int tpc_bufferev_init_common(struct tpc_bufferev_private *bufev_private,
							 tpc_evbase_t *base, const tpc_bufferev_ops *ops, int options);
void tpc_bufferev_run_writecb(tpc_bufferev_t *bufev);
void tpc_bufferev_run_eventcb(tpc_bufferev_t *bufev, short what);
int tpc_bufferev_add_event(tpc_events *ev, const struct timeval *tv);
void tpc_bufferev_incref_and_lock(tpc_bufferev_t *bufev);
int tpc_bufferev_decref_and_unlock(tpc_bufferev_t *bufev);

TPC_END_DELS

#endif // TPC_BUFFEREV_CHANNEL_H
