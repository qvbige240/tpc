/**
 * History:
 * ================================================================
 * 2019-1-18 qing.zou created
 *
 */

#include "tpc_event_buffer.h"
#include "ice_client.h"

#ifdef _DEBUG
static int cnt_malloc = 0;
static int cnt_free = 0;
static int cnt_memcpy = 0;
static int cnt_memmove = 0;

static int send_count = 0;
#endif

static tpc_evbuffer_chain *tpc_evbuffer_chain_new(size_t size)
{
	tpc_evbuffer_chain *chain;
	size_t to_alloc;

	if (size > TPC_EVBUFFER_CHAIN_MAX - TPC_EVBUFFER_CHAIN_SIZE)
		return (NULL);

	size += TPC_EVBUFFER_CHAIN_SIZE;

	/* get the next largest memory that can hold the buffer */
	if (size < TPC_EVBUFFER_CHAIN_MAX / 2) {
		to_alloc = TPC_MIN_BUFFER_SIZE;
		while (to_alloc < size) {
			to_alloc <<= 1;
		}
	} else {
		to_alloc = size;
	}

#ifdef _DEBUG
	cnt_malloc++;
#endif

	/* we get everything in one chunk */
	if ((chain = TPC_MALLOC(to_alloc)) == NULL)
		return (NULL);

	memset(chain, 0, TPC_EVBUFFER_CHAIN_SIZE);

	chain->buffer_len = to_alloc - TPC_EVBUFFER_CHAIN_SIZE;

	/* this way we can manipulate the buffer to different addresses,
	 * which is required for mmap for example.
	 */
	chain->buffer = TPC_EVBUFFER_CHAIN_EXTRA(unsigned char, chain);

	chain->refcnt = 1;

	return (chain);
}

static inline void tpc_evbuffer_chain_free(tpc_evbuffer_chain *chain)
{

#ifdef _DEBUG
	cnt_free++;
#endif

	TPC_FREE(chain);
}

static void tpc_evbuffer_free_all_chains(tpc_evbuffer_chain *chain)
{
	tpc_evbuffer_chain *next;
	for (; chain; chain = next) {
		next = chain->next;
		tpc_evbuffer_chain_free(chain);
	}
}

static int tpc_evbuffer_chains_all_empty(tpc_evbuffer_chain *chain)
{
	for (; chain; chain = chain->next) {
		if (chain->off)
			return 0;
	}
	return 1;
}

/* Free all trailing chains in 'buf' that are neither pinned nor empty, prior
 * to replacing them all with a new chain.  Return a pointer to the place
 * where the new chain will go.
 *
 * Internal; requires lock.  The caller must fix up buf->last and buf->first
 * as needed; they might have been freed.
 */
static tpc_evbuffer_chain **
tpc_evbuffer_free_trailing_empty_chains(tpc_evbuffer *buf)
{
	tpc_evbuffer_chain **ch = buf->last_with_datap;
	/* Find the first victim chain.  It might be *last_with_datap */
	while ((*ch) && ((*ch)->off != 0))
		ch = &(*ch)->next;

	if (*ch) {
		TPC_EVUTIL_ASSERT(tpc_evbuffer_chains_all_empty(*ch));
		tpc_evbuffer_free_all_chains(*ch);
		*ch = NULL;
	}
	return ch;
}

#if 0
/* Add a single chain 'chain' to the end of 'buf', freeing trailing empty
 * chains as necessary.  Requires lock.  Does not schedule callbacks.
 */
static void tpc_evbuffer_chain_insert(tpc_evbuffer *buf, tpc_evbuffer_chain *chain)
{
	//ASSERT_EVBUFFER_LOCKED(buf);
	if (*buf->last_with_datap == NULL) {
		/* There are no chains data on the buffer at all. */
		TPC_EVUTIL_ASSERT(buf->last_with_datap == &buf->first);
		TPC_EVUTIL_ASSERT(buf->first == NULL);
		buf->first = buf->last = chain;
	} else {
		tpc_evbuffer_chain **chp;
		chp = tpc_evbuffer_free_trailing_empty_chains(buf);
		*chp = chain;
		if (chain->off)
			buf->last_with_datap = chp;
		buf->last = chain;
	}
	buf->total_len += chain->off;
}
#else
/* Add a single chain 'chain' to the end of 'buf', freeing trailing empty
 * chains as necessary.  Requires lock.  Does not schedule callbacks.
 */
static void tpc_evbuffer_chain_insert(tpc_evbuffer *buf, tpc_evbuffer_chain *chain)
{
	//ASSERT_EVBUFFER_LOCKED(buf);
	if (*buf->last_with_datap == NULL) {
		/* There are no chains data on the buffer at all. */
		TPC_EVUTIL_ASSERT(buf->last_with_datap == &buf->first);
		TPC_EVUTIL_ASSERT(buf->first == NULL);
		buf->first = buf->last = chain;
	} else {
		tpc_evbuffer_chain **ch = buf->last_with_datap;
		/* Find the first victim chain.  It might be *last_with_datap */
		while ((*ch) && ((*ch)->off != 0))
			ch = &(*ch)->next;

		if (*ch == NULL) {
			/* There is no victim; just append this new chain. */
			buf->last->next = chain;
			if (chain->off)
				buf->last_with_datap = &buf->last->next;
		} else {
			/* Replace all victim chains with this chain. */
			TPC_EVUTIL_ASSERT(tpc_evbuffer_chains_all_empty(*ch));
			tpc_evbuffer_free_all_chains(*ch);
			*ch = chain;
		}
		buf->last = chain;
	}
	buf->total_len += chain->off;
}
#endif

/** Helper: realigns the memory in chain->buffer so that misalign is 0. */
static void tpc_evbuffer_chain_align(tpc_evbuffer_chain *chain)
{
	//EVUTIL_ASSERT(!(chain->flags & EVBUFFER_IMMUTABLE));
	//EVUTIL_ASSERT(!(chain->flags & EVBUFFER_MEM_PINNED_ANY));
	TPC_MEMMOVE1(chain->buffer, chain->buffer + chain->misalign, chain->off);
	chain->misalign = 0;
}

/** Return true if we should realign chain to fit datalen bytes of data in it. */
static int tpc_evbuffer_chain_should_realign(tpc_evbuffer_chain *chain, size_t datlen)
{
	return chain->buffer_len - chain->off >= datlen &&
	    (chain->off < chain->buffer_len / 2) &&
	    (chain->off <= TPC_MAX_TO_REALIGN_IN_EXPAND);
}

tpc_evbuffer *tpc_evbuffer_new(void)
{
	tpc_evbuffer *buffer;

	buffer = TPC_CALLOC(1, sizeof(tpc_evbuffer));
	if (buffer == NULL)
		return (NULL);

	//LIST_INIT(&buffer->callbacks);
	//TAILQ_INIT(&buffer->callbacks);
	buffer->refcnt = 1;
	buffer->last_with_datap = &buffer->first;

	return (buffer);
}

void tpc_evbuffer_free(tpc_evbuffer *buffer)
{
	TPC_EVBUFFER_LOCK(buffer);
	tpc_evbuffer_chain *chain, *next;
	//ASSERT_EVBUFFER_LOCKED(buffer);

	//TPC_EVUTIL_ASSERT(buffer->refcnt > 0);

	if (--buffer->refcnt > 0) {
		TPC_EVBUFFER_UNLOCK(buffer);
		TPC_LOGE("evbuffer free return: buffer->refcnt %d", buffer->refcnt);
		return;
	}

	for (chain = buffer->first; chain != NULL; chain = next) {
		next = chain->next;
		tpc_evbuffer_chain_free(chain);
	}
	//evbuffer_remove_all_callbacks(buffer);
	//if (buffer->deferred_cbs)
	//	event_deferred_cb_cancel_(buffer->cb_queue, &buffer->deferred);

	TPC_EVBUFFER_UNLOCK(buffer);
	if (buffer->own_lock)
		TPC_EVTHREAD_FREE_LOCK(buffer->lock, TPC_THREAD_LOCKTYPE_RECURSIVE);

	TPC_FREE(buffer);

#ifdef _DEBUG
	printf("===============================================\n");
	printf("malloc: %d, free: %d, memcpy: %d, memmove: %d\n", cnt_malloc, cnt_free, cnt_memcpy, cnt_memmove);
	printf("send count: %d\n", send_count);
	printf("===============================================\n");
#endif

}

int tpc_evbuffer_enable_locking(tpc_evbuffer *buf, void *lock)
{
#ifdef TPC_EVENT_DISABLE_THREAD_SUPPORT
	return -1;
#else
	if (buf->lock)
		return -1;

	if (!lock) {
		TPC_EVTHREAD_ALLOC_LOCK(lock, TPC_THREAD_LOCKTYPE_RECURSIVE);
		if (!lock)
			return -1;
		buf->lock = lock;
		buf->own_lock = 1;
	} else {
		buf->lock = lock;
		buf->own_lock = 0;
	}

	return 0;
#endif
}

void tpc_evbuffer_lock(tpc_evbuffer *buf)
{
	TPC_EVBUFFER_LOCK(buf);
}
void tpc_evbuffer_unlock(tpc_evbuffer *buf)
{
	TPC_EVBUFFER_UNLOCK(buf);
}

void tpc_evbuffer_set_parent(tpc_evbuffer *buf, tpc_bufferev_t *bev)
{
	TPC_EVBUFFER_LOCK(buf);
	buf->parent = bev;
	TPC_EVBUFFER_UNLOCK(buf);
}

static void tpc_evbuffer_run_callbacks(tpc_evbuffer *buffer, int running_deferred)
{
	struct tpc_evbuffer_cb_entry *cbent;
	struct tpc_evbuffer_cb_info info;
	size_t new_size;
	int mask, masked_val;
	int clear = 1;

	//if (running_deferred) {
	//	mask = EVBUFFER_CB_NODEFER|TPC_EVBUFFER_CB_ENABLED;
	//	masked_val = TPC_EVBUFFER_CB_ENABLED;
	//} else if (buffer->deferred_cbs) {
	//	mask = EVBUFFER_CB_NODEFER|TPC_EVBUFFER_CB_ENABLED;
	//	masked_val = EVBUFFER_CB_NODEFER|TPC_EVBUFFER_CB_ENABLED;
	//	/* Don't zero-out n_add/n_del, since the deferred callbacks
	//	   will want to see them. */
	//	clear = 0;
	//} else 
	{
		mask = TPC_EVBUFFER_CB_ENABLED;
		masked_val = TPC_EVBUFFER_CB_ENABLED;
	}

	//ASSERT_EVBUFFER_LOCKED(buffer);

	//if (TAILQ_EMPTY(&buffer->callbacks)) {
	if (!buffer->callback.cb_func) {
		buffer->n_add_for_cb = buffer->n_del_for_cb = 0;
		return;
	}
	if (buffer->n_add_for_cb == 0 && buffer->n_del_for_cb == 0)
		return;

	new_size = buffer->total_len;
	info.orig_size = new_size + buffer->n_del_for_cb - buffer->n_add_for_cb;
	info.n_added = buffer->n_add_for_cb;
	info.n_deleted = buffer->n_del_for_cb;
	if (clear) {
		buffer->n_add_for_cb = 0;
		buffer->n_del_for_cb = 0;
	}
	//for (cbent = TAILQ_FIRST(&buffer->callbacks);
	//     cbent != TAILQ_END(&buffer->callbacks);
	//     cbent = next) {
	//	/* Get the 'next' pointer now in case this callback decides
	//	 * to remove itself or something. */
	//	next = TAILQ_NEXT(cbent, next);

	//	if ((cbent->flags & mask) != masked_val)
	//		continue;

	//	if ((cbent->flags & EVBUFFER_CB_OBSOLETE))
	//		cbent->cb.cb_obsolete(buffer,
	//		    info.orig_size, new_size, cbent->cbarg);
	//	else
	//		cbent->cb.cb_func(buffer, &info, cbent->cbarg);
	//}
	cbent = &buffer->callback;
	cbent->cb_func(buffer, &info, cbent->cb_args);
}

void tpc_evbuffer_invoke_callbacks(tpc_evbuffer *buffer)
{
	//if (TAILQ_EMPTY(&buffer->callbacks)) {
	//	buffer->n_add_for_cb = buffer->n_del_for_cb = 0;
	//	return;
	//}

	//if (buffer->deferred_cbs) {
	//	if (buffer->deferred.queued)
	//		return;
	//	_evbuffer_incref_and_lock(buffer);
	//	if (buffer->parent)
	//		bufferevent_incref(buffer->parent);
	//	TPC_EVBUFFER_UNLOCK(buffer);
	//	event_deferred_cb_schedule(buffer->cb_queue, &buffer->deferred);
	//}

	tpc_evbuffer_run_callbacks(buffer, 0);
}

int tpc_evbuffer_add(tpc_evbuffer *buf, const void *data_in, size_t datlen)
{
	tpc_evbuffer_chain *chain, *tmp;
	const unsigned char *data = data_in;
	size_t remain, to_alloc;
	int result = -1;

	TPC_EVBUFFER_LOCK(buf);

	if (buf->freeze_end) {
		goto done;
	}
	/* Prevent buf->total_len overflow */
	if (datlen > TPC_EVBUFFER_CHAIN_MAX - buf->total_len) {
		goto done;
	}

	chain = buf->last;

	/* If there are no chains allocated for this buffer, allocate one
	 * big enough to hold all the data. */
	if (chain == NULL) {
		chain = tpc_evbuffer_chain_new(datlen);
		if (!chain)
			goto done;
		tpc_evbuffer_chain_insert(buf, chain);
	}

	//if ((chain->flags & EVBUFFER_IMMUTABLE) == 0) {
	//	/* Always true for mutable buffers */
	//	TPC_EVUTIL_ASSERT(chain->misalign >= 0 && chain->misalign <= TPC_EVBUFFER_CHAIN_MAX);
	//	remain = chain->buffer_len - (size_t)chain->misalign - chain->off;
	//	if (remain >= datlen) {
	//		/* there's enough space to hold all the data in the
	//		 * current last chain */
	//		TPC_MEMCPY1(chain->buffer + chain->misalign + chain->off,
	//		    data, datlen);
	//		chain->off += datlen;
	//		buf->total_len += datlen;
	//		buf->n_add_for_cb += datlen;
	//		goto out;
	//	} else if (/*!CHAIN_PINNED(chain) && */tpc_evbuffer_chain_should_realign(chain, datlen)) {
	//		/* we can fit the data into the misalignment */
	//		tpc_evbuffer_chain_align(chain);

	//		TPC_MEMCPY1(chain->buffer + chain->off, data, datlen);
	//		chain->off += datlen;
	//		buf->total_len += datlen;
	//		buf->n_add_for_cb += datlen;
	//		goto out;
	//	}
	//} else {
	//	/* we cannot write any data to the last chain */
	//	remain = 0;
	//}

	remain = chain->buffer_len - (size_t)chain->misalign - chain->off;
	if (remain >= datlen) {
		/* there's enough space to hold all the data in the
		* current last chain */
		TPC_MEMCPY1(chain->buffer + chain->misalign + chain->off, data, datlen);
		chain->off += datlen;
		buf->total_len += datlen;
		buf->n_add_for_cb += datlen;
		goto out;
	} else if (/*!CHAIN_PINNED(chain) && */tpc_evbuffer_chain_should_realign(chain, datlen)) {
		/* we can fit the data into the misalignment */
		tpc_evbuffer_chain_align(chain);

		TPC_MEMCPY1(chain->buffer + chain->off, data, datlen);
		chain->off += datlen;
		buf->total_len += datlen;
		buf->n_add_for_cb += datlen;
		goto out;
	}

	/* we need to add another chain */
	to_alloc = chain->buffer_len;
	if (to_alloc <= TPC_BUFFER_CHAIN_MAX_AUTO_SIZE/2)
		to_alloc <<= 1;
	if (datlen > to_alloc)
		to_alloc = datlen;
	tmp = tpc_evbuffer_chain_new(to_alloc);
	if (tmp == NULL)
		goto done;

	if (remain) {
		TPC_MEMCPY1(chain->buffer + chain->misalign + chain->off, data, remain);
		chain->off += remain;
		buf->total_len += remain;
		buf->n_add_for_cb += remain;
	}

	data += remain;
	datlen -= remain;

	TPC_MEMCPY1(tmp->buffer, data, datlen);
	tmp->off = datlen;
	tpc_evbuffer_chain_insert(buf, tmp);
	buf->n_add_for_cb += datlen;

out:
	tpc_evbuffer_invoke_callbacks(buf);
	result = 0;
done:
	TPC_EVBUFFER_UNLOCK(buf);
	return result;
}

unsigned char *tpc_evbuffer_pullup(tpc_evbuffer *buf, size_t size)
{
	tpc_evbuffer_chain *chain, *next, *tmp, *last_with_data;
	unsigned char *buffer, *result = NULL;
	size_t remaining;
	int removed_last_with_data = 0;
	int removed_last_with_datap = 0;

	TPC_EVBUFFER_LOCK(buf);

	chain = buf->first;

	if (size < 0)
		size = buf->total_len;
	/* if size > buf->total_len, we cannot guarantee to the user that she
	 * is going to have a long enough buffer afterwards; so we return
	 * NULL */
	if (size == 0 || (size_t)size > buf->total_len)
		goto done;

	/* No need to pull up anything; the first size bytes are
	 * already here. */
	if (chain->off >= (size_t)size) {
		result = chain->buffer + chain->misalign;
		goto done;
	}

	/* Make sure that none of the chains we need to copy from is pinned. */
	remaining = size - chain->off;
	TPC_EVUTIL_ASSERT(remaining >= 0);
	//for (tmp=chain->next; tmp; tmp=tmp->next) {
	//	if (CHAIN_PINNED(tmp))
	//		goto done;
	//	if (tmp->off >= (size_t)remaining)
	//		break;
	//	remaining -= tmp->off;
	//}

	//if (CHAIN_PINNED(chain)) {
	//	size_t old_off = chain->off;
	//	if (CHAIN_SPACE_LEN(chain) < size - chain->off) {
	//		/* not enough room at end of chunk. */
	//		goto done;
	//	}
	//	buffer = CHAIN_SPACE_PTR(chain);
	//	tmp = chain;
	//	tmp->off = size;
	//	size -= old_off;
	//	chain = chain->next;
	//} else 
	if (chain->buffer_len - chain->misalign >= (size_t)size) {
		/* already have enough space in the first chain */
		size_t old_off = chain->off;
		buffer = chain->buffer + chain->misalign + chain->off;
		tmp = chain;
		tmp->off = size;
		size -= old_off;
		chain = chain->next;
	} else {
		if ((tmp = tpc_evbuffer_chain_new(size)) == NULL) {
			//event_warn("%s: out of memory", __func__);
			TPC_LOGE("%s: out of memory", __func__);
			goto done;
		}
		buffer = tmp->buffer;
		tmp->off = size;
		buf->first = tmp;
	}

	/* Copy and free every chunk that will be entirely pulled into tmp */
	last_with_data = *buf->last_with_datap;
	for (; chain != NULL && (size_t)size >= chain->off; chain = next) {
		next = chain->next;

		TPC_MEMCPY1(buffer, chain->buffer + chain->misalign, chain->off);
		size -= chain->off;
		buffer += chain->off;
		if (chain == last_with_data)
			removed_last_with_data = 1;
		if (&chain->next == buf->last_with_datap)
			removed_last_with_datap = 1;

		tpc_evbuffer_chain_free(chain);
	}

	if (chain != NULL) {
		TPC_MEMCPY1(buffer, chain->buffer + chain->misalign, size);
		chain->misalign += size;
		chain->off -= size;
	} else {
		buf->last = tmp;
	}

	tmp->next = chain;

	if (removed_last_with_data) {
		buf->last_with_datap = &buf->first;
	} else if (removed_last_with_datap) {
		if (buf->first->next && buf->first->next->off)
			buf->last_with_datap = &buf->first->next;
		else
			buf->last_with_datap = &buf->first;
	}

	result = (tmp->buffer + tmp->misalign);

done:
	TPC_EVBUFFER_UNLOCK(buf);
	return result;
}

static inline void ZERO_CHAIN(tpc_evbuffer *dst)
{
	//ASSERT_EVBUFFER_LOCKED(dst);
	dst->first = NULL;
	dst->last = NULL;
	dst->last_with_datap = &(dst)->first;
	dst->total_len = 0;
}

int tpc_evbuffer_drain(tpc_evbuffer *buf, size_t len)
{
	tpc_evbuffer_chain *chain, *next;
	size_t remaining, old_len;
	int result = 0;

	TPC_EVBUFFER_LOCK(buf);
	old_len = buf->total_len;

	if (old_len == 0)
		goto done;

	if (buf->freeze_start) {
		result = -1;
		goto done;
	}

	if (len >= old_len/* && !HAS_PINNED_R(buf)*/) {
		len = old_len;
		for (chain = buf->first; chain != NULL; chain = next) {
			next = chain->next;
			tpc_evbuffer_chain_free(chain);
		}

		ZERO_CHAIN(buf);
	} else {
		buf->total_len -= len;
		remaining = len;
		for (chain = buf->first;
			remaining >= chain->off;
			chain = next) {
				next = chain->next;
				remaining -= chain->off;

				if (chain == *buf->last_with_datap) {
					buf->last_with_datap = &buf->first;
				}
				if (&chain->next == buf->last_with_datap)
					buf->last_with_datap = &buf->first;

				//if (CHAIN_PINNED_R(chain)) {
				//	EVUTIL_ASSERT(remaining == 0);
				//	chain->misalign += chain->off;
				//	chain->off = 0;
				//	break;
				//} else
				//	evbuffer_chain_free(chain);
				tpc_evbuffer_chain_free(chain);
		}

		buf->first = chain;
		if (chain) {
			TPC_EVUTIL_ASSERT(remaining <= chain->off);
			chain->misalign += remaining;
			chain->off -= remaining;
		}
	}

	buf->n_del_for_cb += len;
	/* Tell someone about changes in this buffer */
	//evbuffer_invoke_callbacks(buf);
	tpc_evbuffer_invoke_callbacks(buf);

done:
	TPC_EVBUFFER_UNLOCK(buf);
	return result;
}

/* Reads data from an event buffer and drains the bytes read */
int tpc_evbuffer_remove(tpc_evbuffer *buf, void *data_out, size_t datlen)
{
	size_t n;
	//TPC_EVBUFFER_LOCK(buf);
	//n = evbuffer_copyout(buf, data_out, datlen);
	//if (n > 0) {
	//	if (evbuffer_drain(buf, n)<0)
	//		n = -1;
	//}
	//TPC_EVBUFFER_UNLOCK(buf);
	return (int)n;
}

size_t tpc_evbuffer_get_length(const tpc_evbuffer *buffer)
{
	size_t result;

	TPC_EVBUFFER_LOCK(buffer);

	result = (buffer->total_len);

	TPC_EVBUFFER_UNLOCK(buffer);

	return result;
}

int tpc_evbuffer_prepend(tpc_evbuffer *buf, const void *data, size_t size);
int tpc_evbuffer_expand(tpc_evbuffer *buf, size_t datlen);
int tpc_evbuffer_write_atmost(tpc_evbuffer *buffer, evutil_channel_t ch, size_t howmuch)
{
	int n = -1;

	TPC_EVBUFFER_LOCK(buffer);

	if (buffer->freeze_start) {
		goto done;
	}

	if (howmuch < 0 || (size_t)howmuch > buffer->total_len)
		howmuch = buffer->total_len;

	if (howmuch > 0) {
//#ifdef USE_SENDFILE
//		struct evbuffer_chain *chain = buffer->first;
//		if (chain != NULL && (chain->flags & EVBUFFER_SENDFILE))
//			n = evbuffer_write_sendfile(buffer, fd, howmuch);
//		else {
//#endif
//#ifdef USE_IOVEC_IMPL
//		n = evbuffer_write_iovec(buffer, fd, howmuch);
//#elif defined(WIN32)
//		/* XXX(nickm) Don't disable this code until we know if
//		 * the WSARecv code above works. */
//		void *p = evbuffer_pullup(buffer, howmuch);
//		EVUTIL_ASSERT(p || !howmuch);
//		n = send(fd, p, howmuch, 0);
//#else
//		void *p = tpc_evbuffer_pullup(buffer, howmuch);
//		TPC_EVUTIL_ASSERT(p || !howmuch);
//		n = write(fd, p, howmuch);
//#endif
//#ifdef USE_SENDFILE
//		}
//#endif
		void *p = tpc_evbuffer_pullup(buffer, howmuch);
		TPC_EVUTIL_ASSERT(p || !howmuch);
		//n = write(fd, p, howmuch);
		//...
		//printf("send(%d): %s", howmuch, p);

		n = ice_packet_send(p, howmuch);
		//n = howmuch;
#ifdef _DEBUG
		printf("send(%d) ", howmuch);
		send_count += howmuch;
		usleep(1000);
#endif
	}

	if (howmuch > 0)
		tpc_evbuffer_drain(buffer, howmuch);

done:
	TPC_EVBUFFER_UNLOCK(buffer);
	return (n);
}

int tpc_evbuffer_write(tpc_evbuffer *buffer, evutil_channel_t ch)
{
	return tpc_evbuffer_write_atmost(buffer, ch, -1);
}

//void tpc_evbuffer_setcb(tpc_evbuffer *buffer, evbuffer_cb cb, void *cbarg)
//{
//	TPC_EVBUFFER_LOCK(buffer);
//
//	if (!TAILQ_EMPTY(&buffer->callbacks))
//		evbuffer_remove_all_callbacks(buffer);
//
//	if (cb) {
//		struct evbuffer_cb_entry *ent =
//			evbuffer_add_cb(buffer, NULL, cbarg);
//		ent->cb.cb_obsolete = cb;
//		ent->flags |= EVBUFFER_CB_OBSOLETE;
//	}
//	TPC_EVBUFFER_UNLOCK(buffer);
//}
struct tpc_evbuffer_cb_entry *tpc_evbuffer_add_cb(tpc_evbuffer *buffer, tpc_evbuffer_cb_func cb, void *cbarg)
{
	struct tpc_evbuffer_cb_entry *e = &buffer->callback;
	//if (! (e = TPC_CALLOC(1, sizeof(struct tpc_evbuffer_cb_entry))))
	//	return NULL;
	TPC_EVBUFFER_LOCK(buffer);
	e->cb_func = cb;
	e->cb_args = cbarg;
	e->flags = TPC_EVBUFFER_CB_ENABLED;
	//TAILQ_INSERT_HEAD(&buffer->callbacks, e, next);
	TPC_EVBUFFER_UNLOCK(buffer);
	return e;
}

int tpc_evbuffer_freeze(tpc_evbuffer *buf, int at_front)
{
	TPC_EVBUFFER_LOCK(buf);
	if (at_front)
		buf->freeze_start = 1;
	else
		buf->freeze_end = 1;
	TPC_EVBUFFER_UNLOCK(buf);
	return 0;
}
int tpc_evbuffer_unfreeze(tpc_evbuffer *buf, int at_front)
{
	TPC_EVBUFFER_LOCK(buf);
	if (at_front)
		buf->freeze_start = 0;
	else
		buf->freeze_end = 0;
	TPC_EVBUFFER_UNLOCK(buf);
	return 0;
}
