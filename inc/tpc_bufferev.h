/**
 * History:
 * ================================================================
 * 2019-1-23 qing.zou created
 *
 */
#ifndef TPC_BUFFEREV_H
#define TPC_BUFFEREV_H

#include "tpc_events.h"
#include "tpc_evbuffer.h"

TPC_BEGIN_DELS

#define evutil_channel_t			int
#define TPC_EVBUFFER_CB_ENABLED		1

#define TPC_BEV_EVENT_READING	0x01	/**< error encountered while reading */
#define TPC_BEV_EVENT_WRITING	0x02	/**< error encountered while writing */
#define TPC_BEV_EVENT_EOF		0x10	/**< eof file reached */
#define TPC_BEV_EVENT_ERROR		0x20	/**< unrecoverable error encountered */
#define TPC_BEV_EVENT_TIMEOUT	0x40	/**< user-specified timeout reached */
#define TPC_BEV_EVENT_CONNECTED	0x80	/**< connect operation finished. */


struct tpc_endpoints_t;
typedef struct tpc_endpoints_t tpc_endpoints_t;

struct tpc_bufferev_ops;
typedef struct tpc_bufferev_ops tpc_bufferev_ops;

struct tpc_bufferev_t;
typedef struct tpc_bufferev_t tpc_bufferev_t;

/**
   A read or write callback for a tpc_bufferev_t.

   The read callback is triggered when new data arrives in the input
   buffer and the amount of readable data exceed the low watermark
   which is 0 by default.

   The write callback is triggered if the write buffer has been
   exhausted or fell below its low watermark.

   @param bev the tpc_bufferev_t that triggered the callback
   @param ctx the user-specified context for this tpc_bufferev_t
 */
typedef void (*tpc_bufferev_data_cb)(tpc_bufferev_t *bev, void *ctx);

typedef void (*tpc_bufferev_event_cb)(tpc_bufferev_t *bev, short what, void *ctx);


struct tpc_watermark {
	size_t low;
	size_t high;
};

struct tpc_bufferev_t {
	tpc_evbase_t				*ev_base;
	const tpc_bufferev_ops		*be_ops;

	//tpc_events				ev_read;
	tpc_events					ev_write;

	//tpc_evbuffer				*input;
	tpc_evbuffer				*output;

	//struct event_watermark wm_read;
	struct tpc_watermark		wm_write;

	//tpc_bufferev_data_cb		readcb;
	tpc_bufferev_data_cb		writecb;

	tpc_bufferev_event_cb		errorcb;
	void						*cbarg;

	//struct timeval			timeout_read;
	struct timeval				timeout_write;

	short						enabled;
};

TPCAPI tpc_bufferev_t *tpc_bufferev_channel_new(tpc_evbase_t *base, evutil_channel_t ch, int options);
TPCAPI tpc_bufferev_t *tpc_bufferev_new(tpc_endpoints_t *endpoint, evutil_channel_t ch);
TPCAPI int tpc_bufferev_channel_connect(tpc_bufferev_t *bev, tpc_endpoints_t *p, void *uid);

TPCAPI void tpc_bufferev_free(tpc_bufferev_t *bufev);

TPCAPI void tpc_bufferev_setcb(tpc_bufferev_t *bufev,
					   tpc_bufferev_data_cb readcb, tpc_bufferev_data_cb writecb,
					   tpc_bufferev_event_cb eventcb, void *cbarg);
TPCAPI int tpc_bufferev_setch(tpc_bufferev_t *bufev, evutil_channel_t ch);
TPCAPI evutil_channel_t tpc_bufferev_getch(tpc_bufferev_t *bufev);
TPCAPI int tpc_bufferev_write(tpc_bufferev_t *bufev, const void *data, size_t size);
TPCAPI size_t tpc_bufferev_read(tpc_bufferev_t *bufev, void *data, size_t size);
TPCAPI tpc_evbuffer *tpc_bufferev_get_input(tpc_bufferev_t *bufev);
TPCAPI tpc_evbuffer *tpc_bufferev_get_output(tpc_bufferev_t *bufev);
TPCAPI int tpc_bufferev_enable(tpc_bufferev_t *bufev, short event);
TPCAPI int tpc_bufferev_disable(tpc_bufferev_t *bufev, short event);
TPCAPI void tpc_bufferev_setwatermark(tpc_bufferev_t *bufev, short events, size_t lowmark, size_t highmark);
TPCAPI void tpc_bufferev_lock(tpc_bufferev_t *bufev);
TPCAPI void tpc_bufferev_unlock(tpc_bufferev_t *bufev);

TPC_END_DELS

#endif // TPC_BUFFEREV_H
