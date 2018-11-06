/**
 * History:
 * ================================================================
 * 2018-10-09 qing.zou created
 *
 */

#ifndef TPC_MINHEAP_H
#define TPC_MINHEAP_H

#include <sys/time.h>
#include <sys/queue.h>		/* tailq */

#include "tpc_util.h"

TPC_BEGIN_DELS

// #define EVENT_PARAM_DATA_SIZE_MAX	EVENT_MQ_MSG_LEN_MAX
#define EVENT_PARAM_DATA_SIZE_MAX	512

struct tpc_evbase_t;
typedef struct tpc_evbase_t tpc_evbase_t;

typedef struct tpc_events
{
	TAILQ_ENTRY(tpc_events)  ev_active_next;
	TAILQ_ENTRY(tpc_events)  ev_next;

	/* for managing timeouts */
	union {
		//TAILQ_ENTRY(tpc_events) ev_next_with_common_timeout;
		int			min_heap_idx;
	} ev_timeout_pos;
	int				ev_fd;

	tpc_evbase_t*	ev_base;

	union {
		/* used for io events */
		struct {
			TAILQ_ENTRY(tpc_events)  ev_io_next;
			struct timeval ev_timeout;		/** relative time **/
		} ev_io;

		/* used for msg notice events */
		struct {
			TAILQ_ENTRY(tpc_events)  ev_notice_next;
			short	ev_ncalls;
			short	*ev_pncalls;
			char	ev_pdata[EVENT_PARAM_DATA_SIZE_MAX];
		} ev_notice;
	} _ev;

	short			ev_events;
	short			ev_result;
	short			ev_flags;
	unsigned char	ev_priority;
	unsigned char	ev_closure;
	struct timeval	ev_timeout;				/** absolute time **/

	/* allows us to adopt for different types of events */
	void (*event_callback)(int fd, short events, void *args);
	void *ev_arg;
} tpc_events;

typedef struct tpc_minheap
{
	tpc_events**	p;
	unsigned		n, a;
} tpc_minheap_t;

static inline void	     tpc_minheap_ctor(tpc_minheap_t* s);
static inline void	     tpc_minheap_dtor(tpc_minheap_t* s);
static inline void	     tpc_minheap_elem_init(tpc_events* e);
static inline int	     tpc_minheap_elt_is_top(const tpc_events *e);
static inline int	     tpc_minheap_empty(tpc_minheap_t* s);
static inline unsigned	     tpc_minheap_size(tpc_minheap_t* s);
static inline tpc_events*  tpc_minheap_top(tpc_minheap_t* s);
static inline int	     tpc_minheap_reserve(tpc_minheap_t* s, unsigned n);
static inline int	     tpc_minheap_push(tpc_minheap_t* s, tpc_events* e);
static inline tpc_events*  tpc_minheap_pop(tpc_minheap_t* s);
static inline int	     tpc_minheap_adjust(tpc_minheap_t *s, tpc_events* e);
static inline int	     tpc_minheap_erase(tpc_minheap_t* s, tpc_events* e);
static inline void	     tpc_minheap_shift_up(tpc_minheap_t* s, unsigned hole_index, tpc_events* e);
static inline void	     tpc_minheap_shift_up_unconditional(tpc_minheap_t* s, unsigned hole_index, tpc_events* e);
static inline void	     tpc_minheap_shift_down(tpc_minheap_t* s, unsigned hole_index, tpc_events* e);

#define tpc_minheap_elem_greater(a, b) \
	(tpc_timercmp(&(a)->ev_timeout, &(b)->ev_timeout, >))

void tpc_minheap_ctor(tpc_minheap_t* s) { s->p = 0; s->n = 0; s->a = 0; }
void tpc_minheap_dtor(tpc_minheap_t* s) { if (s->p) TPC_FREE(s->p); }
void tpc_minheap_elem_init(tpc_events* e) { e->ev_timeout_pos.min_heap_idx = -1; }
int tpc_minheap_empty(tpc_minheap_t* s) { return 0u == s->n; }
unsigned tpc_minheap_size(tpc_minheap_t* s) { return s->n; }
tpc_events* tpc_minheap_top(tpc_minheap_t* s) { return s->n ? *s->p : 0; }

int tpc_minheap_push(tpc_minheap_t* s, tpc_events* e)
{
	if (tpc_minheap_reserve(s, s->n + 1))
		return -1;
	tpc_minheap_shift_up(s, s->n++, e);
	return 0;
}

tpc_events* tpc_minheap_pop(tpc_minheap_t* s)
{
	if (s->n)
	{
		tpc_events* e = *s->p;
		tpc_minheap_shift_down(s, 0u, s->p[--s->n]);
		e->ev_timeout_pos.min_heap_idx = -1;
		return e;
	}
	return 0;
}

int tpc_minheap_elt_is_top(const tpc_events *e)
{
	return e->ev_timeout_pos.min_heap_idx == 0;
}

int tpc_minheap_erase(tpc_minheap_t* s, tpc_events* e)
{
	if (-1 != e->ev_timeout_pos.min_heap_idx)
	{
		tpc_events *last = s->p[--s->n];
		unsigned parent = (e->ev_timeout_pos.min_heap_idx - 1) / 2;
		/* we replace e with the last element in the heap.  We might need to
		shift it upward if it is less than its parent, or downward if it is
		greater than one or both its children. Since the children are known
		to be less than the parent, it can't need to shift both up and
		down. */
		if (e->ev_timeout_pos.min_heap_idx > 0 && tpc_minheap_elem_greater(s->p[parent], last))
			tpc_minheap_shift_up_unconditional(s, e->ev_timeout_pos.min_heap_idx, last);
		else
			tpc_minheap_shift_down(s, e->ev_timeout_pos.min_heap_idx, last);
		e->ev_timeout_pos.min_heap_idx = -1;
		return 0;
	}
	return -1;
}

int tpc_minheap_adjust(tpc_minheap_t *s, tpc_events *e)
{
	if (-1 == e->ev_timeout_pos.min_heap_idx) {
		return tpc_minheap_push(s, e);
	} else {
		unsigned parent = (e->ev_timeout_pos.min_heap_idx - 1) / 2;
		/* The position of e has changed; we shift it up or down
		* as needed.  We can't need to do both. */
		if (e->ev_timeout_pos.min_heap_idx > 0 && tpc_minheap_elem_greater(s->p[parent], e))
			tpc_minheap_shift_up_unconditional(s, e->ev_timeout_pos.min_heap_idx, e);
		else
			tpc_minheap_shift_down(s, e->ev_timeout_pos.min_heap_idx, e);
		return 0;
	}
}

int tpc_minheap_reserve(tpc_minheap_t* s, unsigned n)
{
	if (s->a < n)
	{
		tpc_events** p;
		unsigned a = s->a ? s->a * 2 : 8;
		if (a < n)
			a = n;
		if (!(p = (tpc_events**)TPC_REALLOC(s->p, a * sizeof *p)))
			return -1;
		s->p = p;
		s->a = a;
	}
	return 0;
}

void tpc_minheap_shift_up_unconditional(tpc_minheap_t* s, unsigned hole_index, tpc_events* e)
{
	unsigned parent = (hole_index - 1) / 2;
	do
	{
		(s->p[hole_index] = s->p[parent])->ev_timeout_pos.min_heap_idx = hole_index;
		hole_index = parent;
		parent = (hole_index - 1) / 2;
	} while (hole_index && tpc_minheap_elem_greater(s->p[parent], e));
	(s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

void tpc_minheap_shift_up(tpc_minheap_t* s, unsigned hole_index, tpc_events* e)
{
	unsigned parent = (hole_index - 1) / 2;
	while (hole_index && tpc_minheap_elem_greater(s->p[parent], e))
	{
		(s->p[hole_index] = s->p[parent])->ev_timeout_pos.min_heap_idx = hole_index;
		hole_index = parent;
		parent = (hole_index - 1) / 2;
	}
	(s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

void tpc_minheap_shift_down(tpc_minheap_t* s, unsigned hole_index, tpc_events* e)
{
	unsigned min_child = 2 * (hole_index + 1);
	while (min_child <= s->n)
	{
		min_child -= min_child == s->n || tpc_minheap_elem_greater(s->p[min_child], s->p[min_child - 1]);
		if (!(tpc_minheap_elem_greater(e, s->p[min_child])))
			break;
		(s->p[hole_index] = s->p[min_child])->ev_timeout_pos.min_heap_idx = hole_index;
		hole_index = min_child;
		min_child = 2 * (hole_index + 1);
	}
	(s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

TPC_END_DELS

#endif /* TPC_MINHEAP_H */
