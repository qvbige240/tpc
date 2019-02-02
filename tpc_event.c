/**
 * History:
 * ================================================================
 * 2018-10-09 qing.zou created
 *
 */

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/eventfd.h>

#include "tpc_event.h"
#include "tpc_event_map.h"
#include "tpc_event_thread.h"

extern const struct tpc_eventop tpc_epoll_ops;

static const struct tpc_eventop *eventops[] = {
	&tpc_epoll_ops,
	NULL
};

static int event_thread_notify(tpc_evbase_t *thiz);

static int use_monotonic = 0;
static void detect_monotonic(void)
{
#if defined(TPC_HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
	struct timespec	ts;
	static int use_monotonic_initialized = 0;

	if (use_monotonic_initialized)
		return;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
		use_monotonic = 1;
	TPC_LOGD(("mono ts: %d %ld", ts.tv_sec, ts.tv_nsec));

	use_monotonic_initialized = 1;
#endif
}

#define CLOCK_SYNC_INTERVAL -1

static int tpc_gettime(tpc_evbase_t* base, struct timeval *tp)
{
#if defined(TPC_HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
	if (use_monotonic) {
		struct timespec	ts;

		if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
			return (-1);

		tp->tv_sec = ts.tv_sec;
		tp->tv_usec = ts.tv_nsec / 1000;
		if (base->last_updated_clock_diff + CLOCK_SYNC_INTERVAL < ts.tv_sec) {
			struct timeval tv;
			tpc_gettimeofday(&tv,NULL);
			tpc_timersub(&tv, tp, &base->tv_clock_diff);
			base->last_updated_clock_diff = ts.tv_sec;
			//TPC_LOGD(("real time: %d %d, clock mono: %d %ld, diff: %d %d",
			//	tv.tv_sec, tv.tv_usec, 
			//	ts.tv_sec, ts.tv_nsec, 
			//	base->tv_clock_diff.tv_sec, base->tv_clock_diff.tv_usec));
		}

		return (0);
	}
#endif

	return (tpc_gettimeofday(tp, NULL));
}

static void event_queue_insert(tpc_evbase_t* base, tpc_events* ev, int queue)
{
	if ((ev->ev_flags & queue)) {
		if (queue & TPC_EVLIST_ACTIVE)
			return;
		TPC_LOGE("event(%p) already on queue %x", ev, queue);
		return;
	}

	if (~ev->ev_flags & TPC_EVLIST_INTERNAL)
		base->event_count++;

	ev->ev_flags |= queue;
	switch (queue) {
		case TPC_EVLIST_INSERTED:
			TAILQ_INSERT_TAIL(&base->eventqueue, ev, ev_next);
			break;
		case TPC_EVLIST_ACTIVE:
			base->event_count_active++;
			TAILQ_INSERT_TAIL(&base->activequeues[ev->ev_priority], ev, ev_active_next);
			break;
		case TPC_EVLIST_TIMEOUT:
			tpc_minheap_push(&base->timeheap, ev);
			break;
		default:
			TPC_LOGE("unknown queue %x", queue);
			break;
	}
}

static void event_queue_remove(tpc_evbase_t* base, tpc_events* ev, int queue)
{
	if (!(ev->ev_flags & queue)) {
		TPC_LOGE("event(%p) not on queue %x", ev, queue);
		return;
	}

	if (~ev->ev_flags & TPC_EVLIST_INTERNAL)
		base->event_count--;

	ev->ev_flags &= ~queue;
	switch (queue) {
		case TPC_EVLIST_INSERTED:
			TAILQ_REMOVE(&base->eventqueue, ev, ev_next);
			break;
		case TPC_EVLIST_ACTIVE:
			base->event_count_active--;
			TAILQ_REMOVE(&base->activequeues[ev->ev_priority], ev, ev_active_next);
			break;
		case TPC_EVLIST_TIMEOUT:
			tpc_minheap_erase(&base->timeheap, ev);
			break;
		default:
			TPC_LOGE("unknown queue %x", queue);
			break;
	}
}

void tpc_event_active_nolock(tpc_events *ev, int flag, short ncalls)
{
	tpc_evbase_t *thiz;

	TPC_LOGD(("event_active: %p (fd %d), flag 0x%02x, callback %p", ev, (ev->ev_fd), (int)flag, ev->event_callback));

	if (ev->ev_flags & TPC_EVLIST_ACTIVE) {
		ev->ev_result |= flag;
		TPC_LOGW("event already be in active!");
		return;
	}

	thiz = ev->ev_base;
	if (thiz == NULL) {
		TPC_LOGW("ev->ev_base is NULL!");
		return;
	}
	ev->ev_result = flag;

	if (ev->ev_events & (TPC_EV_SIGNAL|TPC_EV_NOTICE)) {
		if (thiz->current_event == ev && !TPC_EVBASE_INTHREAD(thiz)) {
			++thiz->current_event_waiters;
			TPC_EVTHREAD_COND_WAIT(thiz->current_event_cond, thiz->th_base_lock);
		}
		ev->ev_ncalls = ncalls;
		//ev->ev_pncalls = NULL;	//...
	}

	event_queue_insert(thiz, ev, TPC_EVLIST_ACTIVE);

	if (TPC_EVBASE_NEED_NOTIFY(thiz))
		event_thread_notify(thiz);
}

static int event_priority_init(tpc_evbase_t* base, int npriorities)
{
	int i = 0;
	if (base->event_count_active || npriorities < 1 || npriorities >= 256)
		return -1;

	if (npriorities == base->nactivequeues)
		return 0;

	if (base->nactivequeues) {
		TPC_FREE(base->activequeues);
		base->nactivequeues = 0;
	}

	base->activequeues = (struct tpc_event_queue *)TPC_CALLOC(npriorities, sizeof(struct tpc_event_queue));
	if (base->activequeues == NULL) {
		TPC_LOGE("calloc failed!");
		return -1;
	}
	base->nactivequeues = npriorities;

	for (i = 0; i < base->nactivequeues; i++)	{
		TAILQ_INIT(&base->activequeues[i]);
	}

	return 0;
}

static int event_add_internal(tpc_events* ev, const struct timeval* tv, int tv_is_absolute)
{
	int notify = 0, ret = 0;
	tpc_evbase_t* thiz = ev->ev_base;

	TPC_LOGI("event_add: event: %p (fd %d), %s%s%s%s call %p",
		ev,
		ev->ev_fd,
		ev->ev_events & TPC_EV_READ ? "TPC_EV_READ " : "",
		ev->ev_events & TPC_EV_WRITE ? "TPC_EV_WRITE " : "",
		ev->ev_events & TPC_EV_NOTICE ? "TPC_EV_NOTICE " : "",
		tv ? "TPC_EV_TIMEOUT " : "",
		ev->event_callback);
	
	/*
	 * prepare for timeout insertion further below, if we get a
	 * failure on any step, we should not change any state.
	 */
	if (tv != NULL && !(ev->ev_flags & TPC_EVLIST_TIMEOUT)) {
		if (tpc_minheap_reserve(&thiz->timeheap, 1 + tpc_minheap_size(&thiz->timeheap)) == -1)
			return (-1);
	}

#ifndef TPC_EVENT_DISABLE_THREAD_SUPPORT
	if (thiz->current_event == ev && (ev->ev_events & (TPC_EV_NOTICE|TPC_EV_SIGNAL))
		&& !TPC_EVBASE_INTHREAD(thiz)) {
			++thiz->current_event_waiters;
			TPC_EVTHREAD_COND_WAIT(thiz->current_event_cond, thiz->th_base_lock);
	}
#endif

	if ((ev->ev_events & (TPC_EV_READ|TPC_EV_WRITE|TPC_EV_SIGNAL|TPC_EV_NOTICE))
		&& !(ev->ev_flags & (TPC_EVLIST_INSERTED|TPC_EVLIST_ACTIVE))) 
	{
		if (ev->ev_events & (TPC_EV_READ|TPC_EV_WRITE))
			ret = tpc_evio_add(thiz, ev->ev_fd, ev);
		else if (ev->ev_events & (TPC_EV_SIGNAL))
			;	//...
		else if (ev->ev_events & (TPC_EV_NOTICE))
			ret = tpc_evnotice_add(thiz, ev->ev_fd, ev);

		if (ret != -1)		//... ret = 0 ?
			event_queue_insert(thiz, ev, TPC_EVLIST_INSERTED);	//...
		if (ret == 1) {
			notify = 1;
			ret = 0;
		}
	}

	if (ret != -1 && tv != NULL) {
		struct timeval now;
		
		/*
		 * for persistent timeout events, we remember the
		 * timeout value and re-add the event.
		 *
		 * If tv_is_absolute, this was already set.
		 */
		//if (ev->ev_closure == EV_CLOSURE_PERSIST && !tv_is_absolute)
		if (ev->ev_closure == TPC_EV_CLOSURE_PERSIST && !tv_is_absolute)
			ev->ev_io_timeout = *tv;

		//...  timeout without callback; active list, need remove and add


		tpc_gettime(thiz, &now);

		if (tv_is_absolute)
			ev->ev_timeout = *tv;
		else
			tpc_timeradd(&now, tv, &ev->ev_timeout);

		TPC_LOGD(("event_add: timeout in %d\"%d(%d\"%d) seconds, (now: %d\"%d)call %p",
			(int)tv->tv_sec, tv->tv_usec, ev->ev_timeout.tv_sec, ev->ev_timeout.tv_usec, now.tv_sec, now.tv_usec, ev->event_callback));

		event_queue_insert(thiz, ev, TPC_EVLIST_TIMEOUT);

		if (tpc_minheap_elt_is_top(ev))
			notify = 1;

	}

	if (ret != -1 && notify && TPC_EVBASE_NEED_NOTIFY(thiz))
		event_thread_notify(thiz);

	return ret;
}

static int event_del_internal(tpc_events* ev)
{
	tpc_evbase_t* thiz;
	int ret = 0, notify = 0;

	TPC_LOGI("event_del: %p (fd %d), callback %p",
		ev, ev->ev_fd, ev->event_callback);

	if (ev->ev_base == NULL)
		return -1;

	thiz = ev->ev_base;

#ifndef TPC_EVENT_DISABLE_THREAD_SUPPORT
	if (thiz->current_event == ev && !TPC_EVBASE_INTHREAD(thiz)) {
		++thiz->current_event_waiters;
		TPC_EVTHREAD_COND_WAIT(thiz->current_event_cond, thiz->th_base_lock);
	}
#endif

	if (ev->ev_flags & TPC_EVLIST_TIMEOUT) {
		event_queue_remove(thiz, ev, TPC_EVLIST_TIMEOUT);
	}

	if (ev->ev_flags & TPC_EVLIST_ACTIVE) {
		event_queue_remove(thiz, ev, TPC_EVLIST_ACTIVE);
	}

	if (ev->ev_flags & TPC_EVLIST_INSERTED) {
		event_queue_remove(thiz, ev, TPC_EVLIST_INSERTED);
		if (ev->ev_events & (TPC_EV_READ|TPC_EV_WRITE))
			ret = tpc_evio_del(thiz, ev->ev_fd, ev);
		else if (ev->ev_events & (TPC_EV_SIGNAL))
			;
		else if (ev->ev_events & (TPC_EV_NOTICE))
			tpc_evnotice_del(thiz, ev->ev_fd, ev);

		if (ret == 1) {
			notify = 1;
			ret = 0;
		}

	}

	if (ret != -1 && notify && TPC_EVBASE_NEED_NOTIFY(thiz))
		event_thread_notify(thiz);

	return ret;
}

static int timeout_next(tpc_evbase_t* thiz, struct timeval **tv_p)
{
	struct timeval now;
	tpc_events *ev;
	struct timeval *tv = *tv_p;

	ev = tpc_minheap_top(&thiz->timeheap);
	if (ev == NULL) {
		TPC_LOGD(("min heap don't have events"));
		*tv_p = NULL;
		return 0;
	}

	if (tpc_gettime(thiz, &now) == -1)
		return -1;

	if (tpc_timercmp(&ev->ev_timeout, &now, <=)) {
		TPC_LOGW("ev_timeout <= now!");
		tpc_timerclear(tv);
		return 0;
	}

	tpc_timersub(&ev->ev_timeout, &now, tv);
	if (tv->tv_sec < 0) {
		TPC_LOGW("tv->tv_sec < 0, it's negative!");
		tv->tv_sec = 0;
	}

	TPC_LOGD(("timeout_next: in %d\"%d(%d\"%d) seconds", 
		(int)tv->tv_sec, tv->tv_usec, ev->ev_timeout.tv_sec, ev->ev_timeout.tv_usec));

	return 0;
}

static void timeout_process(tpc_evbase_t *thiz)
{
	struct timeval now;
	tpc_events *ev;

	if (tpc_minheap_empty(&thiz->timeheap)) {
		return;
	}

	tpc_gettime(thiz, &now);

	while((ev = tpc_minheap_top(&thiz->timeheap))) {

		TPC_LOGD(("ev_timeout: %d %d, now: %d %d",
			ev->ev_timeout.tv_sec, ev->ev_timeout.tv_usec,
			now.tv_sec, now.tv_usec));

		if (tpc_timercmp(&ev->ev_timeout, &now, >))
			break;

		/* delete this event from the queues */
		event_del_internal(ev);
		tpc_event_active_nolock(ev, TPC_EV_TIMEOUT, 1);
		//TPC_LOGD(("timeout_process: call %p", ev->event_callback));
	}
}

static INLINE void event_notice_closure(tpc_evbase_t *thiz, tpc_events *ev)
{
	short ncalls = ev->ev_ncalls;

	TPC_EVRELEASE_LOCK(thiz, th_base_lock);
	while (ncalls) {
		ncalls--;
		ev->ev_ncalls = ncalls;

		(*ev->event_callback)(ev->ev_fd, ev->ev_result, ev->ev_arg);

		//TPC_EVACQUIRE_LOCK(base, th_base_lock);
		//should_break = base->event_break;
		//TPC_EVRELEASE_LOCK(base, th_base_lock);
	}
}

static void event_persist_closure(tpc_evbase_t *thiz, tpc_events *ev)
{
	void (*timer_event_cb)(int, short, void *);
	int cb_fd;
	short cb_result;
	void *cb_arg;

	if (ev->ev_io_timeout.tv_sec || ev->ev_io_timeout.tv_usec) {
		struct timeval run_at, relative_to, delay, now;

		tpc_gettime(thiz, &now);
		delay = ev->ev_io_timeout;
		if (ev->ev_result & TPC_EV_TIMEOUT) {
			relative_to = ev->ev_timeout;
		} else {
			relative_to = now;
		}
		tpc_timeradd(&relative_to, &delay, &run_at);
		TPC_LOGD(("ev_timeout: %d %d, delay: %d %d, run_at: %d %d, now: %d %d",
			ev->ev_timeout.tv_sec, ev->ev_timeout.tv_usec,
			delay.tv_sec, delay.tv_usec,
			run_at.tv_sec, run_at.tv_usec,
			now.tv_sec, now.tv_usec));
		
		if (tpc_timercmp(&run_at, &now, <)) {
			/* Looks like we missed at least one invocation due to
			 * a clock jump, not running the event loop for a
			 * while, really slow callbacks, or
			 * something. Reschedule relative to now.
			 */
			tpc_timeradd(&now, &delay, &run_at);
		}

		event_add_internal(ev, &run_at, 1);
	}

	timer_event_cb	= ev->event_callback;
	cb_fd			= ev->ev_fd;
	cb_result		= ev->ev_result;
	cb_arg			= ev->ev_arg;

	TPC_EVRELEASE_LOCK(thiz, th_base_lock);

	/** exec the callback **/
	timer_event_cb(cb_fd, cb_result, cb_arg);
}

static int event_process_active_single_queue(tpc_evbase_t *thiz, struct tpc_event_queue *activeq)
{
	int count = 0;
	tpc_events *ev;
	return_val_if_fail(thiz && activeq, -1);

	for (ev = TAILQ_FIRST(activeq); ev; ev = TAILQ_FIRST(activeq)) {
		if (ev->ev_events & TPC_EV_PERSIST)
			event_queue_remove(thiz, ev, TPC_EVLIST_ACTIVE);	//...
		else
			event_del_internal(ev);

		if (!(ev->ev_flags & TPC_EVLIST_INTERNAL))
			++count;

		TPC_LOGD((
			"event_process_active event: %p, %s%s%s%scall %p",
			ev,
			ev->ev_result & TPC_EV_READ ? "TPC_EV_READ " : "",
			ev->ev_result & TPC_EV_WRITE ? "TPC_EV_WRITE " : "",
			ev->ev_result & TPC_EV_TIMEOUT ? "TPC_EV_TIMEOUT " : "",
			ev->ev_result & TPC_EV_NOTICE ? "TPC_EV_NOTICE " : "",
			ev->event_callback));

#ifndef TPC_EVENT_DISABLE_THREAD_SUPPORT
		thiz->current_event = ev;
		thiz->current_event_waiters = 0;
#endif

		switch (ev->ev_closure) {
			case TPC_EV_CLOSURE_SIGNAL:
				break;
			case TPC_EV_CLOSURE_NOTICE:
				event_notice_closure(thiz, ev);
				break;
			case TPC_EV_CLOSURE_PERSIST:
				event_persist_closure(thiz, ev);
				break;
			case TPC_EV_CLOSURE_NONE:
			default:
				TPC_EVRELEASE_LOCK(thiz, th_base_lock);
				(*ev->event_callback)(ev->ev_fd, ev->ev_result, ev->ev_arg);
				break;
		}
		TPC_EVACQUIRE_LOCK(thiz, th_base_lock);
#ifndef TPC_EVENT_DISABLE_THREAD_SUPPORT
		thiz->current_event = NULL;
		if (thiz->current_event_waiters) {
			thiz->current_event_waiters = 0;
			TPC_EVTHREAD_COND_BROADCAST(thiz->current_event_cond);
		}
#endif
	}

	return count;
}

static int event_process_active(tpc_evbase_t *thiz)
{
	int i = 0;
	struct tpc_event_queue *activeq = NULL;

	for (i = 0; i < thiz->nactivequeues; i++) {
		if (TAILQ_FIRST(&thiz->activequeues[i]) != NULL) {
			activeq = &thiz->activequeues[i];
			event_process_active_single_queue(thiz, activeq);
		}
	}

	return 0;
}

int tpc_event_pending(const tpc_events *ev, short event, struct timeval *tv)
{
	int flags = 0;

	if (!ev->ev_base) {
		TPC_LOGE("%s: event has no event_base set.", __func__);
		return -1;
	}

	TPC_EVACQUIRE_LOCK(ev->ev_base, th_base_lock);

	if (ev->ev_flags & TPC_EVLIST_INSERTED)
		flags |= (ev->ev_events & (TPC_EV_READ|TPC_EV_WRITE|TPC_EV_SIGNAL|TPC_EV_NOTICE));
	if (ev->ev_flags & TPC_EVLIST_ACTIVE)
		flags |= ev->ev_result;
	if (ev->ev_flags & TPC_EVLIST_TIMEOUT)
		flags |= TPC_EV_TIMEOUT;

	event &= (TPC_EV_TIMEOUT|TPC_EV_READ|TPC_EV_WRITE|TPC_EV_SIGNAL|TPC_EV_NOTICE);

	/* See if there is a timeout that we should report */
	if (tv != NULL && (flags & event & TPC_EV_TIMEOUT)) {
		struct timeval tmp = ev->ev_timeout;
		tmp.tv_usec &=  0x000fffff;
#if defined(TPC_HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
		/* correctly remamp to real time */
		tpc_timeradd(&ev->ev_base->tv_clock_diff, &tmp, tv);
#else
		*tv = tmp;
#endif
	}

	TPC_EVRELEASE_LOCK(ev->ev_base, th_base_lock);

	return (flags & event);
}

int tpc_event_assign(tpc_events *ev, tpc_evbase_t *base, int fd, short events, tpc_event_callback callback, void *arg)
{
	if (!base) {
		TPC_LOGE("base is null!");
		return -1;
	}

	ev->ev_base = base;

	ev->event_callback	= callback;
	ev->ev_arg			= arg;
	ev->ev_fd			= fd;
	ev->ev_events		= events;
	ev->ev_result		= 0;
	ev->ev_flags		= TPC_EVLIST_INIT;
	ev->ev_ncalls		= 0;
	/* add for notice event to pass param data */
	memset(ev->ev_pdata, 0x00, sizeof(ev->ev_pdata));

	if (events & TPC_EV_SIGNAL) {
		if (events & (TPC_EV_READ | TPC_EV_WRITE)) {
			TPC_LOGW("TPC_EV_SIGNAL is not compatible with read/write");
			return -1;
		}
		ev->ev_closure = TPC_EV_CLOSURE_SIGNAL;
	} else if (events & TPC_EV_NOTICE) {
		if (events & (TPC_EV_READ | TPC_EV_WRITE)) {
			TPC_LOGW("TPC_EV_NOTICE is not compatible with read/write");
			return -1;
		}
		ev->ev_closure = TPC_EV_CLOSURE_NOTICE;
	} else {
		if (events & TPC_EV_PERSIST) {
			tpc_timerclear(&ev->ev_io_timeout);
			ev->ev_closure = TPC_EV_CLOSURE_PERSIST;
		} else {
			ev->ev_closure = TPC_EV_CLOSURE_NONE;
		}
	}

	tpc_minheap_elem_init(ev);

	ev->ev_priority = base->nactivequeues / 2;

	return 0;
}

int tpc_event_base_set(tpc_evbase_t *base, tpc_events *ev)
{
	/* Only innocent events may be assigned to a different base */
	if (ev->ev_flags != TPC_EVLIST_INIT)
		return (-1);

	//_event_debug_assert_is_setup(ev);

	ev->ev_base = base;
	ev->ev_priority = base->nactivequeues/2;

	return (0);
}

tpc_events *tpc_event_new(tpc_evbase_t *base, int fd, short events, tpc_event_callback callback, void *arg)
{
	tpc_events *ev = TPC_MALLOC(sizeof(tpc_events));
	if (ev) {
		if (tpc_event_assign(ev, base, fd, events, callback, arg) < 0) {
			TPC_FREE(ev);
			return NULL;
		}
	}
	return ev;
}

void tpc_event_free(tpc_events *ev)
{
	if (ev)
	{
		tpc_event_del(ev);
		TPC_FREE(ev);
	}
}

int tpc_event_add(tpc_events *ev, const struct timeval *tv)
{
	int ret = 0;
	if (!ev->ev_base) {
		TPC_LOGE("event has no evbase set.");
		return -1;
	}

	TPC_EVACQUIRE_LOCK(ev->ev_base, th_base_lock);

	ret = event_add_internal(ev, tv, 0);

	TPC_EVRELEASE_LOCK(ev->ev_base, th_base_lock);

	return ret;
}

int tpc_event_del(tpc_events *ev)
{
	int ret = 0;
	if (!ev->ev_base) {
		TPC_LOGE("event has no evbase set.");
		return -1;
	}

	TPC_EVACQUIRE_LOCK(ev->ev_base, th_base_lock);

	ret = event_del_internal(ev);

	TPC_EVRELEASE_LOCK(ev->ev_base, th_base_lock);

	return ret;
}

int tpc_event_data_set(tpc_events *ev, void *data, int len)
{
	if (data && len > 0) {
		int size = len > EVENT_PARAM_DATA_SIZE_MAX - 1 ? EVENT_PARAM_DATA_SIZE_MAX - 1 : len;
		if (size != len)
			TPC_LOGE("event param data len is out of the scope");
		memset(ev->ev_pdata, 0x00, EVENT_PARAM_DATA_SIZE_MAX);
		memcpy(ev->ev_pdata, data, size);
	}
	return 0;
}

char *tpc_event_data_get(tpc_events *ev)
{
	return ev->ev_pdata;
}

//#ifndef TPC_EVENT_DISABLE_THREAD_SUPPORT
//int event_global_setup_locks(const int enable_locks)
//{
//	if (tpc_evmsg_global_setup_locks(enable_locks) < 0)
//		return -1;
//	return 0;
//}
//#endif

tpc_evbase_t* tpc_evbase_create(void)
{
	tpc_evbase_t *base;

	if ((base = (tpc_evbase_t*)TPC_CALLOC(1, sizeof(tpc_evbase_t))) == NULL) {
		TPC_LOGE("calloc error!");
		return NULL;
	}
	tpc_event_use_pthreads();

	detect_monotonic();
	tpc_gettime(base, &base->timer_tv);
	
	tpc_minheap_ctor(&base->timeheap);
	TAILQ_INIT(&base->eventqueue);
	base->th_notify_fd[0] = -1;
	base->th_notify_fd[1] = -1;

	tpc_iomap_init(&base->iomap);
	tpc_noticemap_init(&base->noticemap);

	base->evsel = eventops[0];
	base->priv = base->evsel->init(base);
	if (base->priv == NULL) {
		TPC_LOGW("no event io mechanism available.");
		base->evsel = NULL;
		tpc_evbase_destroy(base);
		return NULL;
	}

	if (event_priority_init(base, 1) < 0) {
		tpc_evbase_destroy(base);
		return NULL;
	}

	// thread
#ifndef TPC_EVENT_DISABLE_THREAD_SUPPORT
	int r = 0;
	TPC_EVTHREAD_ALLOC_LOCK(base->th_base_lock, TPC_THREAD_LOCKTYPE_RECURSIVE);
	TPC_EVTHREAD_ALLOC_COND(base->current_event_cond);
	r = tpc_evthread_make_notifiable(base);
	if (r < 0) {
		TPC_LOGW("unable to make notifiable.");
		tpc_evbase_destroy(base);
		return NULL;
	}

	//event_global_setup_locks(1);
#endif

	return base;
}

int tpc_evbase_loopbreak(tpc_evbase_t *event_base)
{
	int r = 0;
	if (event_base == NULL)
		return (-1);

	TPC_EVACQUIRE_LOCK(event_base, th_base_lock);
	event_base->event_break = 1;

	if (TPC_EVBASE_NEED_NOTIFY(event_base)) {
		r = event_thread_notify(event_base);
	} else {
		r = (0);
	}
	TPC_EVRELEASE_LOCK(event_base, th_base_lock);
	return r;
}

int tpc_evbase_loop(tpc_evbase_t* thiz, int flags)
{
	struct timeval tv = {0};
	struct timeval *tv_p;
	int res, done, ret = 0;

	TPC_EVACQUIRE_LOCK(thiz, th_base_lock);
	if (thiz->running_loop) {
		TPC_LOGW("already running loop, only one can run on evbase at once.");
		TPC_EVRELEASE_LOCK(thiz, th_base_lock);
		return -1;
	}

	thiz->running_loop = 1;
	done = 0;

	thiz->th_owner_id = TPC_EVTHREAD_GET_ID();

	while (!done) {
		tv_p = &tv;

		if (thiz->event_break)
			break;

		TPC_LOGD(("=========111event_count_active %d event cnt %d", thiz->event_count_active, thiz->event_count));
		if (!thiz->event_count_active) {
			timeout_next(thiz, &tv_p);
		} else {
			tpc_timerclear(&tv);
		}
		TPC_LOGD(("=========222event_count_active %d event cnt %d, tv: %d/%d", 
			thiz->event_count_active, thiz->event_count, tv.tv_sec, tv.tv_usec));

		if (!thiz->event_count_active && !(thiz->event_count > 0)) {
			TPC_LOGI("no events registered.");
			//sleep(5);
		}

		tpc_gettime(thiz, &thiz->timer_tv);
#if 0
		sleep((int)(tv.tv_sec));	// dispatch epoll ...
#else
		//struct timeval temp;
		//temp.tv_sec = tv.tv_sec;
		//temp.tv_usec = tv.tv_usec;
		//select(0, NULL, NULL, NULL, &tv);
		
		res = thiz->evsel->dispatch(thiz, tv_p);
		if (res == -1) {
			TPC_LOGD(("dispatch returned failed."));
			ret = 0;
			//goto done;
		}
#endif

		timeout_process(thiz);

		TPC_LOGD(("=========333event_count_active %d event cnt %d", thiz->event_count_active, thiz->event_count));
		if (thiz->event_count_active) {
			event_process_active(thiz);
		}
		//TPC_LOGD(("=========444event_count_active %d, %d seconds", thiz->event_count_active, (int)tv.tv_sec));
	}

	TPC_EVRELEASE_LOCK(thiz, th_base_lock);

	return ret;
}

void tpc_evbase_destroy(tpc_evbase_t* thiz)
{
	int i, n_deleted = 0;
	tpc_events* ev;
	if (thiz == NULL) {
		TPC_LOGW("no evbase to free");
		return;
	}

	if (thiz->th_notify_fd[0] != -1) {
		tpc_event_del(&thiz->th_notify);
		close(thiz->th_notify_fd[0]);
		if (thiz->th_notify_fd[1] != -1)
			close(thiz->th_notify_fd[1]);
		thiz->th_notify_fd[0] = -1;
		thiz->th_notify_fd[1] = -1;
	}

	while((ev = tpc_minheap_top(&thiz->timeheap))) {
		tpc_event_del(ev);
		n_deleted++;
	}
	tpc_minheap_dtor(&thiz->timeheap);

	for (i = 0; i < thiz->nactivequeues; ++i) {
		for (ev = TAILQ_FIRST(&thiz->activequeues[i]); ev; ) {
			tpc_events *next = TAILQ_NEXT(ev, ev_active_next);
			if (!(ev->ev_flags & TPC_EVLIST_INTERNAL)) {
				tpc_event_del(ev);
				++n_deleted;
			}
			ev = next;
		}
	}

	if (n_deleted) {
		TPC_LOGD(("%d events were still set in evbase."));
	}

	if (thiz->evsel && thiz->evsel->dealloc)
		thiz->evsel->dealloc(thiz);

	tpc_iomap_clear(&thiz->iomap);
	tpc_noticemap_clear(&thiz->noticemap);

	TPC_EVTHREAD_FREE_LOCK(thiz->th_base_lock, TPC_THREAD_LOCKTYPE_RECURSIVE);
	TPC_EVTHREAD_FREE_COND(thiz->current_event_cond);

	TPC_FREE(thiz->activequeues);
	TPC_FREE(thiz);
}

static int event_thread_notify(tpc_evbase_t *thiz)
{
	if (!thiz->th_notify_func) {
		TPC_LOGE("th_notify_func is null.");
		return -1;
	}
	if (thiz->is_notify_pending)
		return 0;
	thiz->is_notify_pending = 1;
	return thiz->th_notify_func(thiz);
}

static int event_thread_notify_default(tpc_evbase_t *thiz)
{
	uint64_t msg = 1;
	int r;
	do {
		r = write(thiz->th_notify_fd[0], (void*) &msg, sizeof(msg));
	} while (r < 0 && errno == EAGAIN);

	TPC_LOGD(("notify write msg: %d, ret = %d", msg, r));
	return (r < 0) ? -1 : 0;
}

static void event_thread_drain_default(int fd, short what, void *arg)
{
	uint64_t msg;
	ssize_t r;
	tpc_evbase_t *thiz = arg;

	r = read(fd, (void*) &msg, sizeof(msg));
	if ( r < 0 && errno != EAGAIN) {
		TPC_LOGW("Error reading from eventfd");
	}
	TPC_LOGD(("notify read msg: %d", msg));
	TPC_EVACQUIRE_LOCK(thiz, th_base_lock);
	thiz->is_notify_pending = 0;
	TPC_EVRELEASE_LOCK(thiz, th_base_lock);
}

int tpc_evthread_make_notifiable(tpc_evbase_t *thiz)
{
	void (*rcallback)(int, short, void *) = event_thread_drain_default;
	int (*notify)(tpc_evbase_t *) = event_thread_notify_default;

	if (!thiz) {
		TPC_LOGE("null pointer error.");
		return -1;
	}

	if (thiz->th_notify_fd[0] >= 0)
		return 0;

	thiz->th_notify_fd[0] = eventfd(0, EFD_CLOEXEC);
	if (thiz->th_notify_fd[0] >= 0) {
		tpc_socket_closeonexec(thiz->th_notify_fd[0]);
		rcallback = event_thread_drain_default;
		notify = event_thread_notify_default;
	}

	TPC_LOGI("eventfd = %d", thiz->th_notify_fd[0]);
	if (thiz->th_notify_fd[0] < 0) {
		TPC_LOGW("fd errrrrr..");
		//... use socketpair
	}

	tpc_socket_nonblocking(thiz->th_notify_fd[0]);
	if (thiz->th_notify_fd[1] > 0)
		tpc_socket_nonblocking(thiz->th_notify_fd[1]);

	thiz->th_notify_func = notify;

	tpc_event_assign(&thiz->th_notify, thiz, thiz->th_notify_fd[0],
						TPC_EV_READ|TPC_EV_PERSIST, rcallback, thiz);

	thiz->th_notify.ev_flags |= TPC_EVLIST_INTERNAL;
	thiz->th_notify.ev_priority = 0;	

	return tpc_event_add(&thiz->th_notify, NULL);
}
