/**
 * History:
 * ================================================================
 * 2018-10-09 qing.zou created
 *
 */
#ifndef TPC_EVENT_MAP_H
#define TPC_EVENT_MAP_H

#include "tpc_typedef.h"

TPC_BEGIN_DELS

void tpc_iomap_init(struct tpc_event_iomap *iomap);

void tpc_iomap_clear(struct tpc_event_iomap *iomap);

int tpc_evio_add(tpc_evbase_t *evbase, int fd, tpc_events *ev);

int tpc_evio_del(tpc_evbase_t *evbase, int fd, tpc_events *ev);

int tpc_evio_active(tpc_evbase_t *evbase, int fd, short events);

/** notice **/
void tpc_noticemap_init(struct tpc_event_noticemap *ctx);

void tpc_noticemap_clear(struct tpc_event_noticemap *ctx);

int tpc_evnotice_add(tpc_evbase_t *base, int msg, tpc_events *ev);

int tpc_evnotice_del(tpc_evbase_t *base, int msg, tpc_events *ev);

void tpc_evnotice_active(tpc_evbase_t *base, int msg, int ncalls, void *data, int size);

TPC_END_DELS

#endif // TPC_EVENT_MAP_H
