/**
 * History:
 * ================================================================
 * 2018-10-16 qing.zou created
 *
 */

//#include <time.h>
//#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>

#include "tpc_event.h"
#include "tpc_event_msg.h"
#include "tpc_event_map.h"
#include "tpc_event_thread.h"

struct evnotice_table {
	int *notices;
	int total;
};

static int evmsg_add(tpc_evbase_t *base, int msgfd, short old, short events, void *p);
static int evmsg_del(tpc_evbase_t *base, int msgfd, short old, short events, void *p);

static const struct tpc_eventop evmsg_ops = {
	"notice",
	NULL,
	evmsg_add,
	evmsg_del,
	NULL,
	NULL,
	0
};

#define EVENT_MSG_NOTICE_BUFFER_MAX		1024

typedef struct _PrivInfo
{
#ifndef TPC_EVENT_DISABLE_THREAD_SUPPORT
	void			*evmsg_lock;
	void			*evmsg_cond;
#endif

	tpc_evbase_t	*evmsg_base;
	int				notice_added_cnt;
	int				notice_fd;

	int				buffer_flag;
	int				buffer_len;
	char			buffer[EVENT_MSG_NOTICE_BUFFER_MAX];

	struct evnotice_table	evtable;

} PrivInfo;

#define TPC_EVNOTICE_LOCK(priv)		TPC_EVLOCK_LOCK(((priv)->evmsg_lock), 0)
#define TPC_EVNOTICE_UNLOCK(priv)	TPC_EVLOCK_UNLOCK(((priv)->evmsg_lock), 0)

#define	EVNOTICE_COND_WAIT(priv)	TPC_EVTHREAD_COND_WAIT(((priv)->evmsg_cond), ((priv)->evmsg_lock))
#define EVNOTICE_COND_BROADCAST(priv)	TPC_EVTHREAD_COND_BROADCAST(((priv)->evmsg_cond))

static void evmsg_callback(int fd, short what, void *arg)
{
	static char notices[1024];
	int n, i;
	int ncaught[TPC_EVENT_NOTICE_MAX_NUM];
	tpc_evbase_t *base = arg;
	
	return_if_fail(base && &base->notice);
	struct tpc_notice_info *thiz = &base->notice;
	DECL_PRIV(thiz, priv);

	memset(&ncaught, 0, sizeof(ncaught));

	while (1) {
		n = recv(fd, notices, sizeof(notices), 0);
		if (n == -1) {
			if (!(errno == EINTR || errno == EAGAIN))
				TPC_LOGE("evmsg_callback: recv error.");
			break;
		} else if (n == 0) {
			break;
		}
		for (i = 0; i < n; ++i) {
			unsigned char key = notices[i];
			TPC_LOGD(("event msg recv key: %d", (int)key));
			if (key < TPC_EVENT_NOTICE_MAX_NUM)
				ncaught[key]++;
		}
	}

#if 0
	TPC_EVACQUIRE_LOCK(base, th_base_lock);
	for (i = 0; i < TPC_EVENT_NOTICE_MAX_NUM; i++)
	{
		if (ncaught[i])
			tpc_evnotice_active(base, i, ncaught[i]);
	}
	TPC_EVRELEASE_LOCK(base, th_base_lock);
#else
	TPC_EVNOTICE_LOCK(priv);

	TPC_EVACQUIRE_LOCK(base, th_base_lock);
	for (i = 0; i < TPC_EVENT_NOTICE_MAX_NUM; i++)
	{
		if (ncaught[i]) {
			if (i == priv->buffer_flag)
				tpc_evnotice_active(base, i, ncaught[i], priv->buffer, priv->buffer_len);
			else
				tpc_evnotice_active(base, i, ncaught[i], NULL, 0);
		}
	}
	TPC_EVRELEASE_LOCK(base, th_base_lock);

	if (priv->buffer_flag > -1) {
		priv->buffer_flag = -1;
		EVNOTICE_COND_BROADCAST(priv);
	}

	TPC_EVNOTICE_UNLOCK(priv);
#endif
}

int tpc_evmsg_init(tpc_evbase_t *base)
{
	struct tpc_notice_info *thiz = &base->notice;
	thiz->priv = TPC_CALLOC(1, sizeof(PrivInfo));
	DECL_PRIV(thiz, priv);
	priv->evmsg_base = base;
	priv->notice_added_cnt = 0;
	priv->notice_fd = -1;

	priv->buffer_flag = -1;
	priv->buffer_len = 0;

	base->notice.ev_notice_pair[0] = -1;
	base->notice.ev_notice_pair[1] = -1;
	tpc_evmsg_global_setup_locks(base, 1);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, base->notice.ev_notice_pair) == -1) {
		TPC_LOGE("%s: socketpair error.", __func__);
		return -1;
	}

	tpc_socket_closeonexec(base->notice.ev_notice_pair[0]);
	tpc_socket_closeonexec(base->notice.ev_notice_pair[1]);

	tpc_socket_nonblocking(base->notice.ev_notice_pair[0]);
	tpc_socket_nonblocking(base->notice.ev_notice_pair[1]);

	tpc_event_assign(&base->notice.ev_notice, base, base->notice.ev_notice_pair[1], 
		TPC_EV_READ | TPC_EV_PERSIST, evmsg_callback, base);

	base->notice.ev_notice.ev_flags |= TPC_EVLIST_INTERNAL;
	base->notice.ev_notice.ev_priority = 0;

	base->evmsgsel = &evmsg_ops;

	return 0;
}

static int evmsg_notice_table_expand(struct evnotice_table *table, int slot)
{
	if (table->total < slot) {
		int total = table->total ? table->total : TPC_EVENT_NOTICE_MAX_NUM;
		int *tmp;

		while (total <= slot) {
			//total <<= 1;
			total = total + (total >> 1);
		}

		tmp = TPC_REALLOC(table->notices, total * sizeof(table->notices[0]));
		if (!tmp)
			return -1;

		memset(&tmp[table->total], 0, (total-table->total) * sizeof(table->notices[0]));

		table->total = total;
		table->notices = tmp;
	}
	return 0;
}

static int evmsg_add(tpc_evbase_t *base, int msgfd, short old, short events, void *p)
{
	struct tpc_notice_info *notice = &base->notice;
	return_val_if_fail(msgfd > 0 && msgfd < TPC_EVENT_NOTICE_MAX_NUM, -1);
	DECL_PRIV(notice, priv);

	TPC_EVNOTICE_LOCK(priv);

	if (msgfd >= priv->evtable.total) {
		if (evmsg_notice_table_expand(&(priv->evtable), msgfd) == -1)
			TPC_EVNOTICE_UNLOCK(priv);
	}
	priv->evtable.notices[msgfd]++;
	
	priv->notice_added_cnt = ++notice->ev_notice_added_cnt;
	priv->notice_fd = base->notice.ev_notice_pair[0];

	TPC_EVNOTICE_UNLOCK(priv);

	if (!notice->ev_notice_added) {
		if (tpc_event_add(&notice->ev_notice, NULL))
			goto add_err;
		notice->ev_notice_added = 1;
	}

	return 0;
add_err:
	TPC_EVNOTICE_LOCK(priv);
	priv->notice_added_cnt--;
	notice->ev_notice_added_cnt--;
	TPC_EVNOTICE_UNLOCK(priv);
	return -1;
}

static int evmsg_del(tpc_evbase_t *base, int msgfd, short old, short events, void *p)
{
	struct tpc_notice_info *thiz = &base->notice;
	return_val_if_fail(msgfd > 0 && msgfd < TPC_EVENT_NOTICE_MAX_NUM, -1);
	DECL_PRIV(thiz, priv);

	TPC_EVNOTICE_LOCK(priv);

	priv->notice_added_cnt--;
	thiz->ev_notice_added_cnt--;

	if (msgfd < priv->evtable.total)
		priv->evtable.notices[msgfd]--;		//... whether could multi-callback for notice

	TPC_EVNOTICE_UNLOCK(priv);

	return 0;
}

//static int evmsg_dispatch(tpc_evbase_t *base, struct timeval *tv)
#if 0
int tpc_evmsg_notice(int key)
{
	int msg = key;
	int flag = 0;

	TPC_EVNOTICE_LOCK(priv);
	if (msg < evtable.total)
		flag = evtable.notices[msg];
	TPC_EVNOTICE_UNLOCK(priv);

	if (flag > 0) {
		send(evmsg_base_fd, (char*)&msg, 1, 0);
		return 0;
	}

	return -1;
}
#else
int tpc_evmsg_notice(tpc_evbase_t *base, int key, void *data, int size)
{
	int msg = key;
	int flag = 0;
	int len = size > EVENT_MSG_NOTICE_BUFFER_MAX - 1 ? EVENT_MSG_NOTICE_BUFFER_MAX - 1 : size;
	return_val_if_fail(base && &base->notice, -1);

	struct tpc_notice_info *thiz = &base->notice;
	DECL_PRIV(thiz, priv);

	TPC_EVNOTICE_LOCK(priv);
	if (msg < priv->evtable.total)
		flag = priv->evtable.notices[msg];
	TPC_EVNOTICE_UNLOCK(priv);

	/* whether this notice is registered */
	if (flag > 0) {
		if (data && len > 0) {
			TPC_EVNOTICE_LOCK(priv);

			while (priv->buffer_flag > -1)
				EVNOTICE_COND_WAIT(priv);

			/* because of 'msg >= 0' */
			priv->buffer_flag = msg;
			priv->buffer_len = len;
			memset(priv->buffer, 0x00, sizeof(priv->buffer));
			memcpy(priv->buffer, data, len);

			TPC_EVNOTICE_UNLOCK(priv);
		}

		send(priv->notice_fd, (char*)&msg, 1, 0);
		return 0;
	}

	return -1;
}
#endif

void tpc_evmsg_dealloc(tpc_evbase_t *base)
{
	return_if_fail(base && &base->notice);
	struct tpc_notice_info *thiz = &base->notice;
	DECL_PRIV(thiz, priv);

	if(thiz->ev_notice_added) {
		tpc_event_del(&thiz->ev_notice);
		thiz->ev_notice_added = 0;
	}
	thiz->ev_notice.ev_flags = 0;

	if (thiz->ev_notice_pair[0] != -1) {
		close(thiz->ev_notice_pair[0]);
		thiz->ev_notice_pair[0] = -1;
	}
	if (thiz->ev_notice_pair[1] != -1) {
		close(thiz->ev_notice_pair[0]);
		thiz->ev_notice_pair[1] = -1;
	}

	TPC_EVNOTICE_LOCK(priv);

	if (priv) {
		priv->evmsg_base = NULL;
		priv->notice_fd = -1;
		priv->notice_added_cnt = 0;

		priv->evtable.total = 0;
		if (priv->evtable.notices)
			TPC_FREE(priv->evtable.notices);
	}

	TPC_EVNOTICE_UNLOCK(priv);
	
	tpc_evmsg_global_free_locks(base);

	if (priv) {
		TPC_FREE(priv);
		priv = NULL;
	}
}


#ifndef TPC_EVENT_DISABLE_THREAD_SUPPORT
int tpc_evmsg_global_setup_locks(tpc_evbase_t *base, const int enable_locks)
{
	struct tpc_notice_info *thiz = &base->notice;
	DECL_PRIV(thiz, priv);

	TPC_EVTHREAD_SETUP_GLOBAL_LOCK(priv->evmsg_lock, 0);
	//TPC_EVTHREAD_ALLOC_LOCK(evmsg_base_lock, 0);
	TPC_EVTHREAD_ALLOC_COND(priv->evmsg_cond);
	return 0;
}
void tpc_evmsg_global_free_locks(tpc_evbase_t *base)
{
	struct tpc_notice_info *thiz = &base->notice;
	DECL_PRIV(thiz, priv);

	TPC_EVTHREAD_FREE_COND(priv->evmsg_lock);
	TPC_EVTHREAD_FREE_LOCK(priv->evmsg_cond, 0);
}
#endif
