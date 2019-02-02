/**
 * History:
 * ================================================================
 * 2018-10-09 qing.zou created
 *
 */
#ifndef TPC_EVENT_H
#define TPC_EVENT_H

//#include <sys/queue.h>		/* tailq */
#include <time.h>				/* defined(CLOCK_MONOTONIC), it's important */

#include "tpc_events.h"
#include "tpc_event_msg.h"

TPC_BEGIN_DELS


#define ev_notice_next	_ev.ev_notice.ev_notice_next
#define ev_io_next	_ev.ev_io.ev_io_next
#define ev_io_timeout	_ev.ev_io.ev_timeout

#define ev_ncalls	_ev.ev_notice.ev_ncalls
#define ev_pdata	_ev.ev_notice.ev_pdata


#define TPC_EVLIST_TIMEOUT			0x01
#define TPC_EVLIST_INSERTED			0x02
#define TPC_EVLIST_SIGNAL			0x04
#define TPC_EVLIST_ACTIVE			0x08
#define TPC_EVLIST_INTERNAL			0x10
#define TPC_EVLIST_NOTICE			0x40		// notice
#define TPC_EVLIST_INIT				0x80

/* Possible values for ev_closure in struct event. */
#define TPC_EV_CLOSURE_NONE			0
#define TPC_EV_CLOSURE_SIGNAL		1
#define TPC_EV_CLOSURE_PERSIST		2
#define TPC_EV_CLOSURE_NOTICE		3

TAILQ_HEAD(tpc_event_queue, tpc_events);

struct tpc_event_iomap {
	/* An array of evmap_io * or of evmap_signal *; empty entries are
	 * set to NULL. */
	void **entries;
	/* The number of entries available in entries */
	int nentries;
};
#define tpc_event_noticemap tpc_event_iomap

struct event_change {
	/* The fd whose events are to be changed */
	int fd;
	/* The events that were enabled on the fd before any of these changes
	   were made.  May include TPC_EV_READ or TPC_EV_WRITE. */
	short old_events;

	/* The changes that we want to make in reading and writing on this fd.
	 * If this is a signal, then read_change has EV_CHANGE_SIGNAL set,
	 * and write_change is unused. */
	unsigned char read_change;
	unsigned char write_change;
};

/* If set, add the event. */
#define TPC_EVCHANGE_ADD		0x01
/* If set, delete the event.  Exclusive with TPC_EVCHANGE_ADD */
#define TPC_EVCHANGE_DEL		0x02
/* If set, this event refers a signal, not an fd. */
#define TPC_EVCHANGE_SIGNAL		TPC_EV_SIGNAL
/* Set for persistent events.  Currently not used. */
#define TPC_EVCHANGE_PERSIST	TPC_EV_PERSIST
/* Set for adding edge-triggered events. */
#define TPC_EVCHANGE_ET			TPC_EV_ET
/* If set, this event refers a notice msg, not an fd. */
#define TPC_EVCHANGE_NOTICE		TPC_EV_NOTICE

struct tpc_eventop {
	const char *name;
	void *(*init)(tpc_evbase_t *);
	/** 
	 * Enable reading/writing on a given fd or signal.  'events' will be
	 * the events that we're trying to enable: one or more of EV_READ,
	 * EV_WRITE, EV_SIGNAL, and EV_ET.  'old' will be those events that
	 * were enabled on this fd previously.  'fdinfo' will be a structure
	 * associated with the fd by the evmap; its size is defined by the
	 * fdinfo field below.  It will be set to 0 the first time the fd is
	 * added.  The function should return 0 on success and -1 on error.
	 */
	int (*add)(tpc_evbase_t *, int fd, short old, short events, void *fdinfo);
	/** As "add", except 'events' contains the events we mean to disable. */
	int (*del)(tpc_evbase_t *, int fd, short old, short events, void *fdinfo);
	int (*dispatch)(tpc_evbase_t *, struct timeval *);
	/** Function to clean up and free our data from the event_base. */
	void (*dealloc)(tpc_evbase_t *);

	//int need_reinit;
	//enum event_method_feature features;
	size_t fdinfo_len;
};

struct tpc_evbase_t
{
	/** multi-plexing **/
	const struct tpc_eventop		*evsel;
	void							*priv;

	/** uses for message event notice */
	const struct tpc_eventop		*evmsgsel;
	struct tpc_notice_info			notice;

	/** Number of total events added to this base */
	int								event_count;
	/** Number of total events active in this base */
	int								event_count_active;

	int								running_loop;
	int								event_break;

	struct tpc_event_queue			*activequeues;
	int								nactivequeues;

	struct tpc_event_iomap			iomap;
	struct tpc_event_noticemap		noticemap;

	struct tpc_event_queue			eventqueue;

	struct timeval					timer_tv;
	tpc_minheap_t					timeheap;

	tpc_events				*moment_event;			/* for transport channel */

	unsigned long			th_owner_id;
	void					*th_base_lock;
	tpc_events				*current_event;
	void					*current_event_cond;
	int						current_event_waiters;

	int						is_notify_pending;
	int						th_notify_fd[2];
	tpc_events				th_notify;
	int						(*th_notify_func)(tpc_evbase_t *base);

//#if defined(TPC_HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
	/** 
	 * Difference between internal time (maybe from clock_gettime) and
	 * gettimeofday. 
	 */
	struct timeval			tv_clock_diff;
	/** Second in which we last updated tv_clock_diff, in monotonic time. **/
	time_t					last_updated_clock_diff;
//#endif

};

int tpc_evthread_make_notifiable(tpc_evbase_t *thiz);

int tpc_event_data_set(tpc_events *ev, void *data, int len);

void tpc_event_active_nolock(tpc_events *ev, int flag, short ncalls);

TPC_END_DELS

#endif // TPC_EVENT_H
