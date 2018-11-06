/**
 * History:
 * ================================================================
 * 2018-10-16 qing.zou created
 *
 */
#ifndef TPC_EVENT_THREAD_H
#define TPC_EVENT_THREAD_H

#include "tpc_typedef.h"

TPC_BEGIN_DELS

/** A flag passed to a locking callback when the lock was allocated as a
 * read-write lock, and we want to acquire or release the lock for writing. */
#define TPC_THREAD_WRITE	0x04
/** A flag passed to a locking callback when the lock was allocated as a
 * read-write lock, and we want to acquire or release the lock for reading. */
#define TPC_THREAD_READ		0x08
/** A flag passed to a locking callback when we don't want to block waiting
 * for the lock; if we can't get the lock immediately, we will instead
 * return nonzero from the locking callback. */
#define TPC_THREAD_TRY		0x10


#define TPC_THREAD_LOCK_API_VERSION		1
/** A recursive lock is one that can be acquired multiple times at once by the
 * same thread.  No other process can allocate the lock until the thread that
 * has been holding it has unlocked it as many times as it locked it. */
#define TPC_THREAD_LOCKTYPE_RECURSIVE	1
/* A read-write lock is one that allows multiple simultaneous readers, but
 * where any one writer excludes all other writers and readers. */
#define TPC_THREAD_LOCKTYPE_READWRITE	2

struct tpc_thread_lock_callbacks {
	/** The current version of the locking API.  Set this to
	 * TPC_THREAD_LOCK_API_VERSION */
	int lock_api_version;
	/** Which kinds of locks does this version of the locking API
	 * support?  A bitfield of TPC_THREAD_LOCKTYPE_RECURSIVE and
	 * TPC_THREAD_LOCKTYPE_READWRITE.
	 *
	 * (Note that RECURSIVE locks are currently mandatory, and
	 * READWRITE locks are not currently used.)
	 **/
	unsigned supported_locktypes;
	/** Function to allocate and initialize new lock of type 'locktype'.
	 * Returns NULL on failure. */
	void *(*alloc)(unsigned locktype);
	/** Funtion to release all storage held in 'lock', which was created
	 * with type 'locktype'. */
	void (*free)(void *lock, unsigned locktype);
	/** Acquire an already-allocated lock at 'lock' with mode 'mode'.
	 * Returns 0 on success, and nonzero on failure. */
	int (*lock)(unsigned mode, void *lock);
	/** Release a lock at 'lock' using mode 'mode'.  Returns 0 on success,
	 * and nonzero on failure. */
	int (*unlock)(unsigned mode, void *lock);
};

#define TPC_THREAD_CONDITION_API_VERSION 1

struct tpc_thread_condition_callbacks {
	/** The current version of the conditions API.  Set this to
	 * TPC_THREAD_CONDITION_API_VERSION */
	int condition_api_version;
	/** Function to allocate and initialize a new condition variable.
	 * Returns the condition variable on success, and NULL on failure.
	 * The 'condtype' argument will be 0 with this API version.
	 */
	void *(*alloc_condition)(unsigned condtype);
	/** Function to free a condition variable. */
	void (*free_condition)(void *cond);
	/** Function to signal a condition variable.  If 'broadcast' is 1, all
	 * threads waiting on 'cond' should be woken; otherwise, only on one
	 * thread is worken.  Should return 0 on success, -1 on failure.
	 * This function will only be called while holding the associated
	 * lock for the condition.
	 */
	int (*signal_condition)(void *cond, int broadcast);
	/** Function to wait for a condition variable.  The lock 'lock'
	 * will be held when this function is called; should be released
	 * while waiting for the condition to be come signalled, and
	 * should be held again when this function returns.
	 * If timeout is provided, it is interval of seconds to wait for
	 * the event to become signalled; if it is NULL, the function
	 * should wait indefinitely.
	 *
	 * The function should return -1 on error; 0 if the condition
	 * was signalled, or 1 on a timeout. */
	int (*wait_condition)(void *cond, void *lock, const struct timeval *timeout);
};

int tpc_event_use_pthreads(void);

unsigned long tpc_evthread_get_id();
void *tpc_evthread_lock_alloc(unsigned locktype);
void tpc_evthread_lock_free(void *lock, unsigned locktype);
int tpc_evthread_lock_lock(unsigned mode, void *lock);
int tpc_evthread_lock_unlock(unsigned mode, void *lock);

void *tpc_evthread_cond_alloc(unsigned condtype);
void tpc_evthread_cond_free(void *cond);
int tpc_evthread_cond_signal(void *cond, int broadcast);
int tpc_evthread_cond_wait(void *cond, void *lock, const struct timeval *tv);
int tpc_evthread_locking_enabled(void);

//extern unsigned long (*_thread_id_fn)(void);
//extern struct tpc_thread_lock_callbacks _thread_lock_fns;
//extern struct tpc_thread_condition_callbacks _thread_cond_fns;

#define TPC_EVTHREAD_GET_ID() tpc_evthread_get_id()
#define TPC_EVBASE_INTHREAD(base)				\
	((base)->th_owner_id == tpc_evthread_get_id())
#define TPC_EVBASE_NEED_NOTIFY(base)			 \
	((base)->running_loop &&			 \
	((base)->th_owner_id != tpc_evthread_get_id()))

#define TPC_EVTHREAD_ALLOC_LOCK(lockvar, locktype)		\
	((lockvar) = tpc_evthread_lock_alloc(locktype))

#define TPC_EVTHREAD_FREE_LOCK(lockvar, locktype)				\
	do {								\
		void *_lock_tmp_ = (lockvar);				\
		if (_lock_tmp_)						\
			tpc_evthread_lock_free(_lock_tmp_, (locktype)); \
	} while (0)

/** Acquire a lock. */
#define TPC_EVLOCK_LOCK(lockvar, mode)					\
	do {								\
		if (lockvar)						\
			tpc_evthread_lock_lock(mode, lockvar);		\
	} while (0)

/** Release a lock */
#define TPC_EVLOCK_UNLOCK(lockvar, mode)					\
	do {								\
		if (lockvar)						\
			tpc_evthread_lock_unlock(mode, lockvar);	\
	} while (0)

/** Lock an event base. */
#define TPC_EVACQUIRE_LOCK(base, lockvar) do {				\
		TPC_EVLOCK_LOCK((base)->lockvar, 0);			\
		TPC_LOGD(("%s[%d]: id[%lu]lock ====\n", __FILE__, __LINE__, tpc_evthread_get_id()));          \
	} while (0)

/** Unlock an event base. */
#define TPC_EVRELEASE_LOCK(base, lockvar) do {				\
		TPC_LOGD(("%s[%d]: id[%lu]unlock ====\n", __FILE__, __LINE__, tpc_evthread_get_id()));        \
		TPC_EVLOCK_UNLOCK((base)->lockvar, 0);			\
	} while (0)

/** Allocate a new condition variable and store it in the void *, condvar */
#define TPC_EVTHREAD_ALLOC_COND(condvar)					\
	do {								\
		(condvar) = tpc_evthread_cond_alloc(0);		\
	} while (0)
/** Deallocate and free a condition variable in condvar */
#define TPC_EVTHREAD_FREE_COND(cond)					\
	do {								\
		if (cond)						\
			tpc_evthread_cond_free((cond));		\
	} while (0)
/** Signal one thread waiting on cond */
#define TPC_EVTHREAD_COND_SIGNAL(cond)					\
	( (cond) ? tpc_evthread_cond_signal((cond), 0) : 0 )
/** Signal all threads waiting on cond */
#define TPC_EVTHREAD_COND_BROADCAST(cond)					\
	( (cond) ? tpc_evthread_cond_signal((cond), 1) : 0 )
/** Wait until the condition 'cond' is signalled.  Must be called while
 * holding 'lock'.  The lock will be released until the condition is
 * signalled, at which point it will be acquired again.  Returns 0 for
 * success, -1 for failure. */
#define TPC_EVTHREAD_COND_WAIT(cond, lock)					\
	( (cond) ? tpc_evthread_cond_wait((cond), (lock), NULL) : 0 )
/** As TPC_EVTHREAD_COND_WAIT, but gives up after 'tv' has elapsed.  Returns 1
 * on timeout. */
#define TPC_EVTHREAD_COND_WAIT_TIMED(cond, lock, tv)			\
	( (cond) ? tpc_evthread_cond_wait((cond), (lock), (tv)) : 0 )

#define TPC_EVTHREAD_LOCKING_ENABLED()		\
	(tpc_evthread_locking_enabled())


#if !defined(TPC_EVENT_DISABLE_THREAD_SUPPORT)

void *tpc_evthread_setup_global_lock(void *lock, unsigned locktype, int enable_locks);

#define TPC_EVTHREAD_SETUP_GLOBAL_LOCK(lockvar, locktype)			\
	do {								\
		lockvar = tpc_evthread_setup_global_lock(lockvar,		\
			(locktype), enable_locks);				\
		if (!lockvar) {						\
			TPC_LOGW("Couldn't allocate %s", #lockvar);	\
			return -1;					\
		}							\
	} while (0);

#endif

TPC_END_DELS

#endif // TPC_EVENT_THREAD_H
