/**
 * History:
 * ================================================================
 * 2019-1-23 qing.zou created
 *
 */
#ifndef TPC_EVBUFFER_H
#define TPC_EVBUFFER_H

#include "tpc_typedef.h"

TPC_BEGIN_DELS


struct tpc_evbuffer;
typedef struct tpc_evbuffer tpc_evbuffer;

/** Structure passed to an evbuffer_cb_func evbuffer callback

    @see evbuffer_cb_func, evbuffer_add_cb()
 */
typedef struct tpc_evbuffer_cb_info {
	/** The number of bytes in this evbuffer when callbacks were last
	 * invoked. */
	size_t		orig_size;
	/** The number of bytes added since callbacks were last invoked. */
	size_t		n_added;
	/** The number of bytes removed since callbacks were last invoked. */
	size_t		n_deleted;
} tpc_evbuffer_cb_info;

/** Type definition for a callback that is invoked whenever data is added or
    removed from an evbuffer.

    If a callback adds or removes data from the buffer or from another
    buffer, this can cause a recursive invocation of your callback or
    other callbacks.  If you ask for an infinite loop, you might just get
    one: watch out!

    @param buffer the buffer whose size has changed
    @param info a structure describing how the buffer changed.
    @param arg a pointer to user data
*/
typedef void (*tpc_evbuffer_cb_func)(tpc_evbuffer *buffer, const tpc_evbuffer_cb_info *info, void *arg);
//typedef void (*evb_callback_func)(tpc_evbuffer *buffer, const evb_callback_info *info, void *arg);

struct tpc_evbuffer_cb_entry;
/** Add a callback to an evbuffer.

  Subsequent calls to evbuffer_add_cb() add new callbacks.  To remove this
  callback, call evbuffer_remove_cb or evbuffer_remove_cb_entry.

  @param buffer the evbuffer to be monitored
  @param cb the callback function to invoke when the evbuffer is modified,
	or NULL to remove all callbacks.
  @param cbarg an argument to be provided to the callback function
  @return a handle to the callback on success, or NULL on failure.
 */
struct tpc_evbuffer_cb_entry *tpc_evbuffer_add_cb(tpc_evbuffer *buffer, tpc_evbuffer_cb_func cb, void *cbarg);

TPCAPI size_t tpc_evbuffer_get_length(const tpc_evbuffer *buf);

TPC_END_DELS

#endif // TPC_EVBUFFER_H
