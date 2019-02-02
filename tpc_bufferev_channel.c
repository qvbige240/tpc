/**
 * History:
 * ================================================================
 * 2019-1-23 qing.zou created
 *
 */

#include "tpc_bufferev_channel.h"

/* prototypes */
static int be_channel_enable(tpc_bufferev_t *, short);
static int be_channel_disable(tpc_bufferev_t *, short);
static void be_channel_destruct(tpc_bufferev_t *);
//static int be_channel_adj_timeouts(tpc_bufferev_t *);
//static int be_channel_flush(tpc_bufferev_t *, short, enum tpc_bufferev_flush_mode);
//static int be_channel_ctrl(tpc_bufferev_t *, enum tpc_bufferev_ctrl_op, union tpc_bufferev_ctrl_data *);

static void be_channel_setfd(tpc_bufferev_t *, evutil_channel_t);

const struct tpc_bufferev_ops tpc_bufferev_ops_channel = {
	"socket",
	tpc_offsetof(struct tpc_bufferev_private, bev),
	be_channel_enable,
	be_channel_disable,
	be_channel_destruct,
	//be_channel_adj_timeouts,
	//be_channel_flush,
	//be_channel_ctrl,
};

#define be_channel_add(ev, t)			\
	tpc_bufferev_add_event((ev), (t))

static void tpc_bufferev_channel_outbuf_cb(tpc_evbuffer *buf,
				const tpc_evbuffer_cb_info *cbinfo, void *arg)
{
	tpc_bufferev_t *bufev = arg;
	struct tpc_bufferev_private *bufev_p =
	    TPC_EVUTIL_UPCAST(bufev, struct tpc_bufferev_private, bev);

	if (cbinfo->n_added &&
	    (bufev->enabled & TPC_EV_WRITE) &&
		//!tpc_event_pending(&bufev->ev_write, TPC_EV_WRITE, NULL) &&
		!tpc_event_pending(&bufev->ev_write, TPC_EV_NOTICE, NULL) &&
	    !bufev_p->write_suspended) {
		/* Somebody added data to the buffer, and we would like to
		 * write, and we were not writing.  So, start writing. */
		if (be_channel_add(&bufev->ev_write, &bufev->timeout_write) == -1) {
		    /* Should we log this? */
		}
	}
}

static void tpc_bufferev_writecb(evutil_channel_t ch, short event, void *arg)
{
	tpc_bufferev_t *bufev = arg;
	struct tpc_bufferev_private *bufev_p =
	    TPC_EVUTIL_UPCAST(bufev, struct tpc_bufferev_private, bev);
	int res = 0;
	short what = TPC_BEV_EVENT_WRITING;
	int connected = 0;
	size_t atmost = -1;

	tpc_bufferev_incref_and_lock(bufev);

	if (event == TPC_EV_TIMEOUT) {
		/* Note that we only check for event==EV_TIMEOUT. If
		 * event==EV_TIMEOUT|EV_WRITE, we can safely ignore the
		 * timeout, since a read has occurred */
		what |= TPC_BEV_EVENT_TIMEOUT;
		goto error;
	}
#if 0
	if (bufev_p->connecting) {
		int c = evutil_channel_finished_connecting(fd);
		/* we need to fake the error if the connection was refused
		 * immediately - usually connection to localhost on BSD */
		if (bufev_p->connection_refused) {
		  bufev_p->connection_refused = 0;
		  c = -1;
		}

		if (c == 0)
			goto done;

		bufev_p->connecting = 0;
		if (c < 0) {
			event_del(&bufev->ev_write);
			event_del(&bufev->ev_read);
			_tpc_bufferev_run_eventcb(bufev, BEV_EVENT_ERROR);
			goto done;
		} else {
			connected = 1;
//#ifdef WIN32
//			if (BEV_IS_ASYNC(bufev)) {
//				event_del(&bufev->ev_write);
//				tpc_bufferev_async_set_connected(bufev);
//				_tpc_bufferev_run_eventcb(bufev,
//						BEV_EVENT_CONNECTED);
//				goto done;
//			}
//#endif
			tpc_bufferev_run_eventcb(bufev, TPC_BEV_EVENT_CONNECTED);
			if (!(bufev->enabled & TPC_EV_WRITE) ||
			    bufev_p->write_suspended) {
				tpc_event_del(&bufev->ev_write);
				goto done;
			}
		}
	}
#endif
	//atmost = _tpc_bufferev_get_write_max(bufev_p);
	atmost = TPC_CHANNEL_PACKAGE_SIZE;

	if (bufev_p->write_suspended)
		goto done;

	if (tpc_evbuffer_get_length(bufev->output)) {
		tpc_evbuffer_unfreeze(bufev->output, 1);
		res = tpc_evbuffer_write_atmost(bufev->output, ch, atmost);
		tpc_evbuffer_freeze(bufev->output, 1);
		if (res < 0) {
			what |= TPC_BEV_EVENT_ERROR;
			goto error;
		}
		//if (res == -1) {
		//	//int err = evutil_channel_geterror(fd);
		//	//if (EVUTIL_ERR_RW_RETRIABLE(err))
		//	//	goto reschedule;
		//	what |= TPC_BEV_EVENT_ERROR;
		//} else if (res == 0) {
		//	/* eof case
		//	   XXXX Actually, a 0 on write doesn't indicate
		//	   an EOF. An ECONNRESET might be more typical.
		//	 */
		//	what |= TPC_BEV_EVENT_EOF;
		//}
		//if (res <= 0)
		//	goto error;

		//_tpc_bufferev_decrement_write_buckets(bufev_p, res);
	}

	if (tpc_evbuffer_get_length(bufev->output) == 0) {
		tpc_event_del(&bufev->ev_write);
	}

	/*
	 * Invoke the user callback if our buffer is drained or below the
	 * low watermark.
	 */
	if ((res || !connected) &&
	    tpc_evbuffer_get_length(bufev->output) <= bufev->wm_write.low) {
		tpc_bufferev_run_writecb(bufev);
	}

	goto done;

 reschedule:
	if (tpc_evbuffer_get_length(bufev->output) == 0) {
		tpc_event_del(&bufev->ev_write);
	}
	goto done;

 error:
	tpc_bufferev_disable(bufev, TPC_EV_WRITE);
	tpc_bufferev_run_eventcb(bufev, what);

 done:
	tpc_bufferev_decref_and_unlock(bufev);
}

tpc_bufferev_t *tpc_bufferev_channel_new(tpc_evbase_t *base, evutil_channel_t ch, int options)
{
	struct tpc_bufferev_private *bufev_p;
	tpc_bufferev_t *bufev;

//#ifdef WIN32
//	if (base && event_base_get_iocp(base))
//		return tpc_bufferev_async_new(base, fd, options);
//#endif

	if ((bufev_p = TPC_CALLOC(1, sizeof(struct tpc_bufferev_private)))== NULL)
		return NULL;

	if (tpc_bufferev_init_common(bufev_p, base, &tpc_bufferev_ops_channel, options) < 0) {
		TPC_FREE(bufev_p);
		return NULL;
	}
	bufev = &bufev_p->bev;
	//evbuffer_set_flags(bufev->output, EVBUFFER_FLAG_DRAINS_TO_FD);

	//event_assign(&bufev->ev_read, bufev->ev_base, fd,
	//    EV_READ|EV_PERSIST, tpc_bufferev_readcb, bufev);
	tpc_event_assign(&bufev->ev_write, bufev->ev_base, ch,
	    TPC_EV_NOTICE|TPC_EV_MOMENT|TPC_EV_PERSIST, tpc_bufferev_writecb, bufev);

	tpc_evbuffer_add_cb(bufev->output, tpc_bufferev_channel_outbuf_cb, bufev);

	//tpc_evbuffer_freeze(bufev->input, 0);
	tpc_evbuffer_freeze(bufev->output, 1);

	return bufev;
}


static int be_channel_enable(tpc_bufferev_t *bufev, short event)
{
	//if (event & EV_READ) {
	//	if (be_channel_add(&bufev->ev_read, &bufev->timeout_read) == -1)
	//		return -1;
	//}
	if (event & TPC_EV_WRITE) {
		if (be_channel_add(&bufev->ev_write, &bufev->timeout_write) == -1)
			return -1;
	}
	return 0;
}

static int be_channel_disable(tpc_bufferev_t *bufev, short event)
{
	//struct tpc_bufferev_private *bufev_p =
	//	EVUTIL_UPCAST(bufev, struct tpc_bufferev_private, bev);
	//if (event & EV_READ) {
	//	if (event_del(&bufev->ev_read) == -1)
	//		return -1;
	//}
	///* Don't actually disable the write if we are trying to connect. */
	if ((event & TPC_EV_WRITE) /*&& ! bufev_p->connecting*/) {
		if (tpc_event_del(&bufev->ev_write) == -1)
			return -1;
	}
	return 0;
}

static void be_channel_destruct(tpc_bufferev_t *bufev)
{
	//struct tpc_bufferev_private *bufev_p =
	//	TPC_EVUTIL_UPCAST(bufev, struct tpc_bufferev_private, bev);
	//evutil_channel_t ch;
	//TPC_EVUTIL_ASSERT(bufev->be_ops == &tpc_bufferev_ops_channel);

	//fd = event_get_ch(&bufev->ev_read);

	//event_del(&bufev->ev_read);
	//event_del(&bufev->ev_write);

	//if ((bufev_p->options & BEV_OPT_CLOSE_ON_FREE) && fd >= 0)
	//	EVUTIL_CLOSESOCKET(fd);

	tpc_event_del(&bufev->ev_write);
}

int tpc_bufferev_base_set(tpc_evbase_t *base, tpc_bufferev_t *bufev)
{
	int res = -1;

	TPC_BEV_LOCK(bufev);
	if (bufev->be_ops != &tpc_bufferev_ops_channel)
		goto done;

	bufev->ev_base = base;

	//res = event_base_set(base, &bufev->ev_read);
	//if (res == -1)
	//	goto done;

	res = tpc_event_base_set(base, &bufev->ev_write);
done:
	TPC_BEV_UNLOCK(bufev);
	return res;
}

