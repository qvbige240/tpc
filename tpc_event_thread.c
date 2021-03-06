/**
 * History:
 * ================================================================
 * 2018-10-16 qing.zou created
 *
 */

#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#include "tpc_util.h"
#include "tpc_event_thread.h"

static pthread_mutexattr_t attr_recursive;

static void *event_thread_lock_alloc(unsigned locktype)
{
	pthread_mutexattr_t *attr = NULL;
	pthread_mutex_t *lock = TPC_MALLOC(sizeof(pthread_mutex_t));
	if (!lock) {
		TPC_LOGE("lock malloc error.");
		return NULL;
	}
	if (locktype & TPC_THREAD_LOCKTYPE_RECURSIVE)
		attr = &attr_recursive;
	if (pthread_mutex_init(lock, attr)) {
		TPC_LOGE("event_thread_lock init error.");
		TPC_FREE(lock);
		return NULL;
	}
	return lock;
}

static void event_thread_lock_free(void *_lock, unsigned locktype)
{
	pthread_mutex_t *lock = _lock;
	pthread_mutex_destroy(lock);
	TPC_FREE(lock);
}

static int event_thread_lock(unsigned mode, void *_lock)
{
	pthread_mutex_t *lock = _lock;
	if (mode & TPC_THREAD_TRY)
		return pthread_mutex_trylock(lock);
	else
		return pthread_mutex_lock(lock);
}

static int event_thread_unlock(unsigned mode, void *_lock)
{
	pthread_mutex_t *lock = _lock;
	return pthread_mutex_unlock(lock);
}

static unsigned long event_thread_get_id(void)
{
	union {
		pthread_t		thr;
		unsigned long	id;
	} r;
	memset(&r, 0, sizeof(r));
	r.thr = pthread_self();
	return (unsigned long)r.id;
}

/** cond **/
static void *event_thread_cond_alloc(unsigned condflags)
{
	pthread_cond_t *cond = TPC_MALLOC(sizeof(pthread_cond_t));
	if (!cond) {
		TPC_LOGE("cond malloc error.");
		return NULL;
	}
	if (pthread_cond_init(cond, NULL)) {
		TPC_LOGE("event_thread_cond init error.");
		TPC_FREE(cond);
		return NULL;
	}
	return cond;
}

static void event_thread_cond_free(void *_cond)
{
	pthread_cond_t *cond = _cond;
	pthread_cond_destroy(cond);
	TPC_FREE(cond);
}

static int event_thread_cond_signal(void *_cond, int broadcast)
{
	pthread_cond_t *cond = _cond;
	int r;
	if (broadcast)
		r = pthread_cond_broadcast(cond);
	else
		r = pthread_cond_signal(cond);
	return r ? -1 : 0;
}

static int event_thread_cond_wait(void *_cond, void *_lock, const struct timeval *tv)
{
	int r;
	pthread_cond_t *cond = _cond;
	pthread_mutex_t *lock = _lock;

	if (tv) {
		struct timeval now, abstime;
		struct timespec ts;
		tpc_gettimeofday(&now, NULL);
		tpc_timeradd(&now, tv, &abstime);
		ts.tv_sec = abstime.tv_sec;
		ts.tv_nsec = abstime.tv_usec * 1000;
		r = pthread_cond_timedwait(cond, lock, &ts);
		if (r == ETIMEDOUT)
			return 1;
		else if (r)
			return -1;
		else
			return 0;
	} else {
		r = pthread_cond_wait(cond, lock);
		return r ? -1 : 0;
	}
}


static void event_thread_set_id_callback(unsigned long (*id_fn)(void));
static int event_thread_set_lock_callbacks(const struct tpc_thread_lock_callbacks *cbs);
static int event_thread_set_condition_callbacks(const struct tpc_thread_condition_callbacks *cbs);

int tpc_event_use_pthreads(void)
{
	struct tpc_thread_lock_callbacks cbs = {
		TPC_THREAD_LOCK_API_VERSION,
		TPC_THREAD_LOCKTYPE_RECURSIVE,
		event_thread_lock_alloc,
		event_thread_lock_free,
		event_thread_lock,
		event_thread_unlock,
	};
	struct tpc_thread_condition_callbacks cond_cbs = {
		TPC_THREAD_CONDITION_API_VERSION,
		event_thread_cond_alloc,
		event_thread_cond_free,
		event_thread_cond_signal,
		event_thread_cond_wait,
	};
	
	if (pthread_mutexattr_init(&attr_recursive)) {
		TPC_LOGE("pthread_mutexattr_init error.");
		return -1;
	}
	if (pthread_mutexattr_settype(&attr_recursive, PTHREAD_MUTEX_RECURSIVE)) {
		TPC_LOGE("pthread_mutexattr_settype error.");
		return -1;
	}

	event_thread_set_lock_callbacks(&cbs);
	event_thread_set_condition_callbacks(&cond_cbs);
	event_thread_set_id_callback(event_thread_get_id);

	return 0;
}


static struct tpc_thread_lock_callbacks _thread_lock_fns = {
	0, 0, NULL, NULL, NULL, NULL
};
static unsigned long (*_thread_id_fn)(void) = NULL;
static struct tpc_thread_condition_callbacks _thread_cond_fns = {
	0, NULL, NULL, NULL, NULL
};

static void event_thread_set_id_callback(unsigned long (*id_fn)(void))
{
	_thread_id_fn = id_fn;
}

static int event_thread_set_lock_callbacks(const struct tpc_thread_lock_callbacks *cbs)
{
	struct tpc_thread_lock_callbacks *target = &_thread_lock_fns;
	if (!cbs) {
		if (target->alloc)
			TPC_LOGW("Trying to disable lock functions after they have been set up will probaby not work.");
		TPC_MEMSET(target, 0, sizeof(_thread_lock_fns));
		return 0;
	}
	if (target->alloc) {
		if (target->lock_api_version == cbs->lock_api_version &&
			target->supported_locktypes == cbs->supported_locktypes &&
			target->alloc == cbs->alloc && target->free == cbs->free &&
			target->lock == cbs->lock && target->unlock == cbs->unlock)
			return 0;
		TPC_LOGW("Can't change lock callbacks once they have been initialized.");
		return -1;
	}

	if (cbs->alloc && cbs->free && cbs->lock && cbs->unlock) {
		TPC_MEMCPY(target, cbs, sizeof(_thread_lock_fns));


		//...

		return 0;
	}

	return 0;
}

static int event_thread_set_condition_callbacks(const struct tpc_thread_condition_callbacks *cbs)
{
	struct tpc_thread_condition_callbacks *target = &_thread_cond_fns;
	if (!cbs) {
		if (target->alloc_condition)
			TPC_LOGW("Trying to disable condition functions after they have been set up will probaby not work.");
		TPC_MEMSET(target, 0, sizeof(_thread_cond_fns));
		return 0;
	}
	if (target->alloc_condition) {
		if (target->condition_api_version == cbs->condition_api_version &&
			target->alloc_condition == cbs->alloc_condition &&
			target->free_condition == cbs->free_condition &&
			target->signal_condition == cbs->signal_condition &&
			target->wait_condition == cbs->wait_condition)
			return 0;
		TPC_LOGW("Can't change condition callbacks once they have been initialized.");
		return -1;
	}

	if (cbs->alloc_condition && cbs->free_condition &&
		cbs->signal_condition && cbs->wait_condition) {
		TPC_MEMCPY(target, cbs, sizeof(_thread_cond_fns));
	}

	return 0;
}

void *tpc_evthread_setup_global_lock(void *lock, unsigned locktype, int enable_locks)
{
	if (enable_locks) {
		return_val_if_fail(lock == NULL, lock);
		return _thread_lock_fns.alloc(locktype);
	} else {
		return lock;
	}
}

unsigned long tpc_evthread_get_id()
{
	return _thread_id_fn ? _thread_id_fn() : 1;
}

void *tpc_evthread_lock_alloc(unsigned locktype)
{
	return _thread_lock_fns.alloc ? _thread_lock_fns.alloc(locktype) : NULL;
}

void tpc_evthread_lock_free(void *lock, unsigned locktype)
{
	if (_thread_lock_fns.free)
		_thread_lock_fns.free(lock, locktype);
}

int tpc_evthread_lock_lock(unsigned mode, void *lock)
{
	if (_thread_lock_fns.lock)
		return _thread_lock_fns.lock(mode, lock);
	else
		return 0;
}

int tpc_evthread_lock_unlock(unsigned mode, void *lock)
{
	if (_thread_lock_fns.unlock)
		return _thread_lock_fns.unlock(mode, lock);
	else
		return 0;
}

void *tpc_evthread_cond_alloc(unsigned condtype)
{
	if (_thread_cond_fns.alloc_condition)
		return _thread_cond_fns.alloc_condition(condtype);
	else
		return NULL;
}

void tpc_evthread_cond_free(void *cond)
{
	if (_thread_cond_fns.free_condition)
		_thread_cond_fns.free_condition(cond);
}

int tpc_evthread_cond_signal(void *cond, int broadcast)
{
	if (_thread_cond_fns.signal_condition)
		return _thread_cond_fns.signal_condition(cond, broadcast);
	else
		return 0;
}

int tpc_evthread_cond_wait(void *cond, void *lock, const struct timeval *tv)
{
	if (_thread_cond_fns.wait_condition)
		return _thread_cond_fns.wait_condition(cond, lock, tv);
	else
		return 0;
}

int tpc_evthread_locking_enabled(void)
{
	return _thread_lock_fns.lock != NULL;
}
