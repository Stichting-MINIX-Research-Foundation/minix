/* LWIP service - pchain.c - pbuf chain utility functions */

#include "lwip.h"

/*
 * Allocate a chain of pbuf buffers as though it were a PBUF_POOL allocation,
 * except that each buffer is of type PBUF_RAM.  Return the pbuf chain on
 * success, or NULL on memory allocation failure.
 */
struct pbuf *
pchain_alloc(int layer, size_t size)
{
	struct pbuf *pbuf, *phead, **pnext;
	size_t chunk, left;
	int offset = 0;

	/*
	 * Check for length overflow.  Note that we do this before prepending
	 * the header, because otherwise we could never send a full-sized
	 * (65535-byte) IP packet.  This does mean that we are generating a
	 * pbuf chain that has over 64KB worth of allocated space, but our
	 * header hiding ensures that tot_len stays under 64KB.  A check in
	 * pbuf_header() prevents that later header adjustments end up lifting
	 * tot_len over this limit.
	 */
	if (size > UINT16_MAX)
		return NULL;

	/*
	 * Unfortunately, we have no choice but to replicate this block from
	 * lwIP's pbuf_alloc() code.  It is however unlikely that the offsets
	 * change for the currently supported layer types, and we do not need
	 * to support any layer types that we do not use ourselves.
	 */
	switch (layer) {
	case PBUF_TRANSPORT:
		offset = PBUF_LINK_ENCAPSULATION_HLEN + PBUF_LINK_HLEN +
		    PBUF_IP_HLEN + PBUF_TRANSPORT_HLEN;
		break;
	case PBUF_IP:
		offset = PBUF_LINK_ENCAPSULATION_HLEN + PBUF_LINK_HLEN +
		    PBUF_IP_HLEN;
		break;
	case PBUF_LINK:
		offset = PBUF_LINK_ENCAPSULATION_HLEN + PBUF_LINK_HLEN;
		break;
	case PBUF_RAW_TX:
		offset = PBUF_LINK_ENCAPSULATION_HLEN;
		break;
	case PBUF_RAW:
		offset = 0;
		break;
	default:
		panic("invalid pbuf layer: %d", layer);
	}

	chunk = size + offset;
	if (chunk > MEMPOOL_BUFSIZE)
		chunk = MEMPOOL_BUFSIZE;

	if ((phead = pbuf_alloc(PBUF_RAW, chunk, PBUF_RAM)) == NULL)
		return NULL;

	if (offset > 0)
		util_pbuf_header(phead, -offset);

	phead->tot_len = size;

	pnext = &phead->next;

	for (left = size - (chunk - offset); left > 0; left -= chunk) {
		chunk = (left < MEMPOOL_BUFSIZE) ? left : MEMPOOL_BUFSIZE;

		if ((pbuf = pbuf_alloc(PBUF_RAW, chunk, PBUF_RAM)) == NULL) {
			/*
			 * Adjust tot_len to match the actual length of the
			 * chain so far, just in case pbuf_free() starts caring
			 * about this in the future.
			 */
			for (pbuf = phead; pbuf != NULL; pbuf = pbuf->next)
				pbuf->tot_len -= left;

			pbuf_free(phead);

			return NULL;
		}

		pbuf->tot_len = left;

		*pnext = pbuf;
		pnext = &pbuf->next;
	}

	return phead;
}

/*
 * Given the (non-empty) chain of buffers 'pbuf', return a pointer to the
 * 'next' field of the last buffer in the chain.  This function is packet queue
 * friendly.  A packet queue is a queue of packet chains, where each chain is
 * delimited using the 'tot_len' field.  As a result, while the pointer
 * returned is never NULL, the value pointed to by the returned pointer may or
 * may not be NULL (and will point to the next chain if not NULL).  As notable
 * exception, in cases where the buffer type is a single PBUF_REF, 'tot_len'
 * may be zero and 'len' may be non-zero.  In such cases, the chain consists of
 * that single buffer only.  This function must handle that case as well.
 */
struct pbuf **
pchain_end(struct pbuf * pbuf)
{

	assert(pbuf != NULL);

	while (pbuf->tot_len > pbuf->len) {
		pbuf = pbuf->next;

		assert(pbuf != NULL);
	}

	return &pbuf->next;
}

/*
 * Given the (non-empty) chain of buffers 'pbuf', return a byte size estimation
 * of the memory used by the chain, rounded up to pool buffer sizes.  This
 * function is packet queue friendly.
 */
size_t
pchain_size(struct pbuf * pbuf)
{
	size_t size;

	assert(pbuf != NULL);

	/*
	 * Count the first buffer separately, as its length may be seriously
	 * off due to header hiding.  While the caller should always provide
	 * exactly the same pbuf chain twice if it intends to get back the same
	 * size twice, this also protects against accidental size differences
	 * due to header hiding in that case.
	 */
	size = MEMPOOL_BUFSIZE;

	/*
	 * Round up the size of the rest of the chain to whole buffers.
	 */
	if (pbuf->tot_len > pbuf->len) {
		size += pbuf->tot_len - pbuf->len + MEMPOOL_BUFSIZE - 1;

		size -= size % MEMPOOL_BUFSIZE;
	}

	return size;
}
