/**
 * History:
 * ================================================================
 * 2018-10-15 qing.zou created
 *
 */

#include "tpc_event.h"
#include "tpc_event_map.h"

typedef struct evmap_io {
	struct tpc_event_queue	events;
	unsigned short			nread;
	unsigned short			nwrite;
} evmap_io;

typedef struct evmap_notice {
	struct tpc_event_queue	events;
} evmap_notice;

static int evmap_make_space(struct tpc_event_iomap *map, int slot, int msize)
{
	if (map->nentries <= slot) {
		int nentries = map->nentries ? map->nentries : 32;
		void **tmp;

		while (nentries <= slot)
			nentries <<= 1;

		tmp = (void **)TPC_REALLOC(map->entries, nentries * msize);
		if (tmp == NULL)
			return (-1);

		memset(&tmp[map->nentries], 0, (nentries - map->nentries) * msize);

		map->nentries = nentries;
		map->entries = tmp;
	}

	return (0);
}

void tpc_iomap_init(struct tpc_event_iomap *iomap)
{
	iomap->nentries = 0;
	iomap->entries = NULL;
}

void tpc_iomap_clear(struct tpc_event_iomap *iomap)
{
	if (iomap->entries != NULL) {
		int i;
		for (i = 0; i < iomap->nentries; i++) {
			if (iomap->entries[i] != NULL)
				TPC_FREE(iomap->entries[i]);
		}
		TPC_FREE(iomap->entries);
		iomap->entries = NULL;
	}
	iomap->nentries = 0;
}

static void tpc_evio_init(evmap_io *entry)
{
	TAILQ_INIT(&entry->events);
	entry->nread = 0;
	entry->nwrite = 0;
}

int tpc_evio_add(tpc_evbase_t *evbase, int fd, tpc_events *ev)
{
	const struct tpc_eventop *evsel = evbase->evsel;
	struct tpc_event_iomap *iomap = &evbase->iomap;
	struct evmap_io *ctx = NULL;
	int nread, nwrite, retval = 0;
	short res = 0, old = 0;
	//tpc_events *old_ev;

	if (fd < 0)
		return 0;

	if (fd >= iomap->nentries) {
		if (evmap_make_space(iomap, fd, sizeof(struct evmap_io *)) == -1)
			return -1;
	}

	if (iomap->entries[fd] == NULL) {
		iomap->entries[fd] = TPC_CALLOC(1, sizeof(struct evmap_io)+evsel->fdinfo_len);
		if (iomap->entries[fd] == NULL)
			return -1;
		tpc_evio_init((struct evmap_io *)iomap->entries[fd]);
	}
	ctx = (struct evmap_io *)iomap->entries[fd];

	nread = ctx->nread;
	nwrite = ctx->nwrite;

	if (nread)
		old |= TPC_EV_READ;
	if (nwrite)
		old |= TPC_EV_WRITE;

	if (ev->ev_events & TPC_EV_READ) {
		if (++nread == 1)
			res |= TPC_EV_READ;
	}
	if (ev->ev_events & TPC_EV_WRITE) {
		if (++nwrite == 1)
			res |= TPC_EV_WRITE;
	}
	if (nread > 0xffff || nwrite > 0xffff) {
		TPC_LOGW("Too many events reading or writing on fd %d", (int)fd);
		return -1;
	}

	if (res) {
		void *extra = ((char*)ctx) + sizeof(struct evmap_io);
		if (evsel->add(evbase, ev->ev_fd, old, (ev->ev_events & TPC_EV_ET) | res, extra) == -1)
			return -1;
		retval = 1;
	}

	ctx->nread = (unsigned short)nread;
	ctx->nwrite = (unsigned short)nwrite;
	TAILQ_INSERT_TAIL(&ctx->events, ev, ev_io_next);

	return retval;
}

int tpc_evio_del(tpc_evbase_t *evbase, int fd, tpc_events *ev)
{
	const struct tpc_eventop *evsel = evbase->evsel;
	struct tpc_event_iomap *iomap = &evbase->iomap;
	struct evmap_io *ctx = NULL;
	int nread, nwrite, retval = 0;
	short res = 0, old = 0;

	if (fd < 0)
		return 0;

	if (fd >= iomap->nentries)
		return -1;

	ctx = (struct evmap_io *)iomap->entries[fd];

	nread = ctx->nread;
	nwrite = ctx->nwrite;
	if (nread)
		old |= TPC_EV_READ;
	if (nwrite)
		old |= TPC_EV_WRITE;

	if (ev->ev_events & TPC_EV_READ) {
		if (--nread == 0)
			res |= TPC_EV_READ;
	}
	if (ev->ev_events & TPC_EV_WRITE) {
		if (--nwrite == 0)
			res |= TPC_EV_WRITE;
	}

	if (res) {
		void *extra = ((char*)ctx) + sizeof(struct evmap_io);
		if (evsel->del(evbase, ev->ev_fd, old, res, extra) == -1)
			return -1;
		retval = 1;
	}

	ctx->nread = nread;
	ctx->nwrite = nwrite;
	TAILQ_REMOVE(&ctx->events, ev, ev_io_next);

	return retval;
}

int tpc_evio_active(tpc_evbase_t *evbase, int fd, short events)
{
	struct tpc_event_iomap *iomap = &evbase->iomap;
	struct evmap_io *ctx = NULL;
	tpc_events *ev = NULL;

	ctx = (struct evmap_io *)iomap->entries[fd];

	TAILQ_FOREACH(ev, &ctx->events, ev_io_next) {
		if (ev->ev_events & events)
			tpc_event_active_nolock(ev, ev->ev_events & events, 1);
	}

	return 0;
}

/** notice **/
void tpc_noticemap_init(struct tpc_event_noticemap *ctx)
{
	tpc_iomap_init(ctx);
}

void tpc_noticemap_clear(struct tpc_event_noticemap *ctx)
{
	tpc_iomap_clear(ctx);
}

static void tpc_evnotice_init(evmap_notice *entry)
{
	TAILQ_INIT(&entry->events);
}

int tpc_evnotice_add(tpc_evbase_t *base, int msg, tpc_events *ev)
{
	const struct tpc_eventop *evsel = base->evmsgsel;
	struct tpc_event_noticemap *noticemap = &base->noticemap;
	struct evmap_notice *ctx = NULL;
	int fd = msg;

	if (fd >= noticemap->nentries) {
		if (evmap_make_space(noticemap, fd, sizeof(struct evmap_notice *)) == -1)
			return -1;
	}

	if (noticemap->entries[fd] == NULL) {
		noticemap->entries[fd] = TPC_CALLOC(1, sizeof(struct evmap_notice)+evsel->fdinfo_len);
		if (noticemap->entries[fd] == NULL)
			return -1;
		tpc_evnotice_init((struct evmap_notice *)noticemap->entries[fd]);
	}
	ctx = (struct evmap_notice *)noticemap->entries[fd];

	if (TAILQ_EMPTY(&ctx->events)) {
		if (evsel->add(base, ev->ev_fd, 0, TPC_EV_NOTICE, NULL) == -1)
			return -1;
	}

	TAILQ_INSERT_TAIL(&ctx->events, ev, ev_notice_next);

	if (ev->ev_events & TPC_EV_MOMENT) {
		TPC_LOGI("add moment_event TPC_EV_MOMENT: %p (fd %d), callback %p", ev, ev->ev_fd, ev->event_callback);
		base->moment_event = ev;
		return 1;
	}
	return 0;
}

int tpc_evnotice_del(tpc_evbase_t *base, int msg, tpc_events *ev)
{
	const struct tpc_eventop *evsel = base->evmsgsel;
	struct tpc_event_noticemap *noticemap = &base->noticemap;
	struct evmap_notice *ctx = NULL;
	int fd = msg;

	if (fd >= noticemap->nentries)
		return -1;

	if (ev->ev_events & TPC_EV_MOMENT) {
		TPC_LOGI("del moment_event TPC_EV_MOMENT: %p (fd %d), callback %p", ev, ev->ev_fd, ev->event_callback);
		base->moment_event = NULL;
	}

	ctx = (struct evmap_notice *)noticemap->entries[fd];

	if (TAILQ_FIRST(&ctx->events) == TAILQ_LAST(&ctx->events, tpc_event_queue)) {
		if (evsel->del(base, ev->ev_fd, 0, TPC_EV_NOTICE, NULL) == -1)
			return -1;
	}

	TAILQ_REMOVE(&ctx->events, ev, ev_notice_next);

	return 1;
}

void tpc_evnotice_active(tpc_evbase_t *base, int msg, int ncalls, void *data, int size)
{
	struct tpc_event_noticemap *noticemap = &base->noticemap;
	struct evmap_notice *ctx = NULL;
	tpc_events *ev;
	int fd = msg;

	ctx = (struct evmap_notice *)noticemap->entries[fd];

	TAILQ_FOREACH(ev, &ctx->events, ev_notice_next) {
		if (data && size > 0)
			tpc_event_data_set(ev, data, size);
		tpc_event_active_nolock(ev, TPC_EV_NOTICE, ncalls);
	}
}
