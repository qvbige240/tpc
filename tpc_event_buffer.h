/**
 * History:
 * ================================================================
 * 2019-1-18 qing.zou created
 *
 */
#ifndef TPC_EVENT_BUFFER_H
#define TPC_EVENT_BUFFER_H

#include <sys/queue.h>
#include "tpc_typedef.h"
#include "tpc_bufferev.h"
#include "tpc_event_thread.h"

TPC_BEGIN_DELS

//#define BUFFER_CHAIN_MAX ((((int64_t) 0x7fffffffL) << 32) | 0xffffffffL)
#define TPC_EVBUFFER_CHAIN_MAX				(4 << 20)

#ifndef TPC_BUFFER_USR_DEF //use event define
    #define TPC_MIN_BUFFER_SIZE				512
    #define TPC_MAX_TO_COPY_IN_EXPAND			4096
    #define TPC_MAX_TO_REALIGN_IN_EXPAND		2048
    #define TPC_BUFFER_CHAIN_MAX_AUTO_SIZE		4096
#else //user customize
    #define TPC_MIN_BUFFER_SIZE 8
    #define MAX_TO_COPY_IN_EXPAND 64
    #define MAX_TO_REALIGN_IN_EXPAND 32
    #define BUFFER_CHAIN_MAX_AUTO_SIZE 64
#endif

#define TPC_EVBUFFER_CHAIN_SIZE				sizeof(tpc_evbuffer_chain)
/** Return a pointer to extra data allocated along with an evbuffer. */
#define TPC_EVBUFFER_CHAIN_EXTRA(t, c)		(t *)((tpc_evbuffer_chain *)(c) + 1)


struct tpc_evbuffer_cb_entry {
	/** Structures to implement a doubly-linked queue of callbacks */
	//LIST_ENTRY(evbuffer_cb_entry) next;
	/** The callback function to invoke when this callback is called. */
	tpc_evbuffer_cb_func	cb_func;
	/** Argument to pass to cb. */
	void					*cb_args;
	/** Currently set flags on this callback. */
	int						flags;
};


typedef struct tpc_evbuffer_chain {
	struct tpc_evbuffer_chain		*next;
	size_t						buffer_len;
	//uint64_t					misalign;
	size_t						misalign;
	size_t						off;
	/** number of references to this chain */
	int							refcnt;
	unsigned char				*buffer;
} tpc_evbuffer_chain;

struct tpc_evbuffer {
	tpc_evbuffer_chain	*first;
	tpc_evbuffer_chain	*last;
	tpc_evbuffer_chain	**last_with_datap;

	/** Total amount of bytes stored in all chains.*/
	size_t				total_len;

	/** Number of bytes we have added to the buffer since we last tried to
	 * invoke callbacks. */
	size_t				n_add_for_cb;
	/** Number of bytes we have removed from the buffer since we last
	 * tried to invoke callbacks. */
	size_t				n_del_for_cb;

//#ifndef TPC_EVENT_DISABLE_THREAD_SUPPORT
	/** A lock used to mediate access to this buffer. */
	void				*lock;
//#endif

	unsigned			own_lock : 1;
	unsigned			freeze_start : 1;
	unsigned			freeze_end : 1;

	uint32_t			flags;
	int					refcnt;

	/** A doubly-linked-list of callback functions */
	//LIST_HEAD(evbuffer_cb_queue, evbuffer_cb_entry) callbacks;
	struct tpc_evbuffer_cb_entry callback;

	/** 
	 * The parent bufferevent object this evbuffer belongs to.
	 * NULL if the evbuffer stands alone. 
	 */
	tpc_bufferev_t		*parent;
};


#define TPC_EVBUFFER_LOCK(buffer)						\
	do {								\
		TPC_EVLOCK_LOCK((buffer)->lock, 0);				\
	} while (0)
#define TPC_EVBUFFER_UNLOCK(buffer)						\
	do {								\
		TPC_EVLOCK_UNLOCK((buffer)->lock, 0);			\
	} while (0)


tpc_evbuffer *tpc_evbuffer_new(void);
void tpc_evbuffer_free(tpc_evbuffer *buf);
int tpc_evbuffer_enable_locking(tpc_evbuffer *buf, void *lock);
void tpc_evbuffer_set_parent(tpc_evbuffer *buf, tpc_bufferev_t *bev);
void tpc_evbuffer_lock(tpc_evbuffer *buf);
void tpc_evbuffer_unlock(tpc_evbuffer *buf);
int tpc_evbuffer_add(tpc_evbuffer *buf, const void *data, size_t datlen);
int tpc_evbuffer_drain(tpc_evbuffer *buf, size_t len);
int tpc_evbuffer_remove(tpc_evbuffer *buf, void *data, size_t datlen);
size_t tpc_evbuffer_get_length(const tpc_evbuffer *buf);

int tpc_evbuffer_prepend(tpc_evbuffer *buf, const void *data, size_t size);
int tpc_evbuffer_expand(tpc_evbuffer *buf, size_t datlen);
int tpc_evbuffer_write_atmost(tpc_evbuffer *buffer, evutil_channel_t ch, size_t howmuch);
int tpc_evbuffer_write(tpc_evbuffer *buffer, evutil_channel_t ch);
int tpc_evbuffer_freeze(tpc_evbuffer *buf, int at_front);
int tpc_evbuffer_unfreeze(tpc_evbuffer *buf, int at_front);

TPC_END_DELS

#endif // TPC_EVENT_BUFFER_H
