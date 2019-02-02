/**
 * History:
 * ================================================================
 * 2019-1-23 qing.zou created
 *
 */

#include "tpc_bufferev.h"
#include "tpc_event_buffer.h"
#include "tpc_bufferev_channel.h"

void tpc_bufferev_run_writecb(tpc_bufferev_t *bufev)
{
	if (bufev->writecb)
		bufev->writecb(bufev, bufev->cbarg);
}
void tpc_bufferev_run_eventcb(tpc_bufferev_t *bufev, short what)
{
	if (bufev->errorcb)
		bufev->errorcb(bufev, what, bufev->cbarg);
}

int tpc_bufferev_init_common(struct tpc_bufferev_private *bufev_private,
						tpc_evbase_t *base, const tpc_bufferev_ops *ops, int options)
{
	tpc_bufferev_t *bufev = &bufev_private->bev;

	//if (!bufev->input) {
	//	if ((bufev->input = tpc_evbuffer_new()) == NULL)
	//		return -1;
	//}

	if (!bufev->output) {
		if ((bufev->output = tpc_evbuffer_new()) == NULL) {
			//tpc_evbuffer_free(bufev->input);
			return -1;
		}
	}

	bufev_private->refcnt = 1;
	bufev->ev_base = base;

	/* Disable timeouts. */
	//tpc_timerclear(&bufev->timeout_read);
	tpc_timerclear(&bufev->timeout_write);

	bufev->be_ops = ops;

	/*
	 * Set to EV_WRITE so that using tpc_bufferev_write is going to
	 * trigger a callback.  Reading needs to be explicitly enabled
	 * because otherwise no data will be available.
	 */
	bufev->enabled = TPC_EV_WRITE;

#ifndef TPC_EVENT_DISABLE_THREAD_SUPPORT
	//if (options & BEV_OPT_THREADSAFE) {
		if (tpc_bufferev_enable_locking(bufev, NULL) < 0) {
			/* cleanup */
			//tpc_evbuffer_free(bufev->input);
			tpc_evbuffer_free(bufev->output);
			//bufev->input = NULL;
			bufev->output = NULL;
			return -1;
		}
	//}
#endif

	bufev_private->options = options;

	//tpc_evbuffer_set_parent(bufev->input, bufev);
	tpc_evbuffer_set_parent(bufev->output, bufev);

	return 0;
}

void tpc_bufferev_incref_and_lock(tpc_bufferev_t *bufev)
{
	struct tpc_bufferev_private *bufev_private = TPC_BEV_UPCAST(bufev);
	TPC_BEV_LOCK(bufev);
	++bufev_private->refcnt;
}
int tpc_bufferev_decref_and_unlock(tpc_bufferev_t *bufev)
{
	struct tpc_bufferev_private *bufev_private =
	    TPC_EVUTIL_UPCAST(bufev, struct tpc_bufferev_private, bev);
	//tpc_bufferev_t *underlying;

	TPC_EVUTIL_ASSERT(bufev_private->refcnt > 0);

	if (--bufev_private->refcnt) {
		TPC_BEV_UNLOCK(bufev);
		return 0;
	}

	//underlying = tpc_bufferev_get_underlying(bufev);

	/* Clean up the shared info */
	if (bufev->be_ops->destruct)
		bufev->be_ops->destruct(bufev);

	/* XXX what happens if refcnt for these buffers is > 1?
	 * The buffers can share a lock with this bufferevent object,
	 * but the lock might be destroyed below. */
	/* evbuffer will free the callbacks */
	//tpc_evbuffer_free(bufev->input);
	tpc_evbuffer_free(bufev->output);

	//event_debug_unassign(&bufev->ev_read);
	//event_debug_unassign(&bufev->ev_write);

	TPC_BEV_UNLOCK(bufev);
	if (bufev_private->own_lock)
		TPC_EVTHREAD_FREE_LOCK(bufev_private->lock, TPC_THREAD_LOCKTYPE_RECURSIVE);

	/* Free the actual allocated memory. */
	TPC_FREE(((char*)bufev) - bufev->be_ops->mem_offset);
	TPC_LOGI("bufferev is free!");

	return 1;
}

void tpc_bufferev_free(tpc_bufferev_t *bufev)
{
	TPC_BEV_LOCK(bufev);
	tpc_bufferev_setcb(bufev, NULL, NULL, NULL, NULL);
	//_tpc_bufferev_cancel_all(bufev);
	tpc_bufferev_decref_and_unlock(bufev);
}

void tpc_bufferev_setcb(tpc_bufferev_t *bufev,
						tpc_bufferev_data_cb readcb, tpc_bufferev_data_cb writecb,
						tpc_bufferev_event_cb eventcb, void *cbarg)
{
	TPC_BEV_LOCK(bufev);

	//bufev->readcb = readcb;
	bufev->writecb = writecb;
	bufev->errorcb = eventcb;

	bufev->cbarg = cbarg;
	TPC_BEV_UNLOCK(bufev);
}

int tpc_bufferev_write(tpc_bufferev_t *bufev, const void *data, size_t size)
{
	if (tpc_evbuffer_add(bufev->output, data, size) == -1)
		return (-1);

	return 0;
}
//size_t tpc_bufferev_read(tpc_bufferev_t *bufev, void *data, size_t size);
//tpc_evbuffer *tpc_bufferev_get_input(tpc_bufferev_t *bufev);
tpc_evbuffer *tpc_bufferev_get_output(tpc_bufferev_t *bufev)
{
	return bufev->output;
}

int tpc_bufferev_enable(tpc_bufferev_t *bufev, short event)
{
	struct tpc_bufferev_private *bufev_private =
		TPC_EVUTIL_UPCAST(bufev, struct tpc_bufferev_private, bev);
	short impl_events = event;
	int r = 0;

	tpc_bufferev_incref_and_lock(bufev);
	//if (bufev_private->read_suspended)
	//	impl_events &= ~TPC_EV_READ;
	if (bufev_private->write_suspended)
		impl_events &= ~TPC_EV_WRITE;

	bufev->enabled |= event;

	if (impl_events && bufev->be_ops->enable(bufev, impl_events) < 0)
		r = -1;

	tpc_bufferev_decref_and_unlock(bufev);
	return r;
}

int tpc_bufferev_disable(tpc_bufferev_t *bufev, short event)
{
	int r = 0;

	TPC_BEV_LOCK(bufev);
	bufev->enabled &= ~event;

	if (bufev->be_ops->disable(bufev, event) < 0)
		r = -1;

	TPC_BEV_UNLOCK(bufev);
	return r;
}

int tpc_bufferev_add_event(tpc_events *ev, const struct timeval *tv)
{
	if (tv->tv_sec == 0 && tv->tv_usec == 0)
		return tpc_event_add(ev, NULL);
	else
		return tpc_event_add(ev, tv);
}

void tpc_bufferev_lock(tpc_bufferev_t *bufev)
{
	tpc_bufferev_incref_and_lock(bufev);
}
void tpc_bufferev_unlock(tpc_bufferev_t *bufev)
{
	tpc_bufferev_decref_and_unlock(bufev);
}

int tpc_bufferev_enable_locking(tpc_bufferev_t *bufev, void *lock)
{
#ifdef TPC_EVENT_DISABLE_THREAD_SUPPORT
	return -1;
#else
	if (TPC_BEV_UPCAST(bufev)->lock)
		return -1;

	if (!lock) {
		TPC_EVTHREAD_ALLOC_LOCK(lock, TPC_THREAD_LOCKTYPE_RECURSIVE);
		if (!lock)
			return -1;
		TPC_BEV_UPCAST(bufev)->lock = lock;
		TPC_BEV_UPCAST(bufev)->own_lock = 1;
	} else {
		TPC_BEV_UPCAST(bufev)->lock = lock;
		TPC_BEV_UPCAST(bufev)->own_lock = 0;
	}
	//tpc_evbuffer_enable_locking(bufev->input, lock);
	tpc_evbuffer_enable_locking(bufev->output, lock);

	return 0;
#endif
}
void tpc_bufferev_setwatermark(tpc_bufferev_t *bufev, short events, size_t lowmark, size_t highmark);
int tpc_bufferev_setch(tpc_bufferev_t *bufev, evutil_channel_t ch);
evutil_channel_t tpc_bufferev_getch(tpc_bufferev_t *bufev);
