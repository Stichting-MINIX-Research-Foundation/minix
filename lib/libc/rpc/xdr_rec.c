/*	$NetBSD: xdr_rec.c,v 1.33 2012/03/20 17:14:50 matt Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint) 
#if 0
static char *sccsid = "@(#)xdr_rec.c 1.21 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)xdr_rec.c	2.2 88/08/01 4.0 RPCSRC";
#else
__RCSID("$NetBSD: xdr_rec.c,v 1.33 2012/03/20 17:14:50 matt Exp $");
#endif
#endif

/*
 * xdr_rec.c, Implements TCP/IP based XDR streams with a "record marking"
 * layer above tcp (for rpc's use).
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * These routines interface XDRSTREAMS to a tcp/ip connection.
 * There is a record marking layer between the xdr stream
 * and the tcp transport level.  A record is composed on one or more
 * record fragments.  A record fragment is a thirty-two bit header followed
 * by n bytes of data, where n is contained in the header.  The header
 * is represented as a htonl(u_long).  Thegh order bit encodes
 * whether or not the fragment is the last fragment of the record
 * (1 => fragment is last, 0 => more fragments to follow. 
 * The other 31 bits encode the byte length of the fragment.
 */

#include "namespace.h"

#include <sys/types.h>

#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/svc.h>
#include <rpc/clnt.h>

#include "rpc_internal.h"

#ifdef __weak_alias
__weak_alias(xdrrec_create,_xdrrec_create)
__weak_alias(xdrrec_endofrecord,_xdrrec_endofrecord)
__weak_alias(xdrrec_eof,_xdrrec_eof)
__weak_alias(xdrrec_skiprecord,_xdrrec_skiprecord)
#endif

static bool_t	xdrrec_getlong(XDR *, long *);
static bool_t	xdrrec_putlong(XDR *, const long *);
static bool_t	xdrrec_getbytes(XDR *, char *, u_int);

static bool_t	xdrrec_putbytes(XDR *, const char *, u_int);
static u_int	xdrrec_getpos(XDR *);
static bool_t	xdrrec_setpos(XDR *, u_int);
static int32_t *xdrrec_inline(XDR *, u_int);
static void	xdrrec_destroy(XDR *);

static const struct  xdr_ops xdrrec_ops = {
	xdrrec_getlong,
	xdrrec_putlong,
	xdrrec_getbytes,
	xdrrec_putbytes,
	xdrrec_getpos,
	xdrrec_setpos,
	xdrrec_inline,
	xdrrec_destroy,
	NULL, /* xdrrec_control */
};

/*
 * A record is composed of one or more record fragments.
 * A record fragment is a four-byte header followed by zero to
 * 2**32-1 bytes.  The header is treated as a long unsigned and is
 * encode/decoded to the network via htonl/ntohl.  The low order 31 bits
 * are a byte count of the fragment.  The highest order bit is a boolean:
 * 1 => this fragment is the last fragment of the record,
 * 0 => this fragment is followed by more fragment(s).
 *
 * The fragment/record machinery is not general;  it is constructed to
 * meet the needs of xdr and rpc based on tcp.
 */

#define LAST_FRAG ((uint32_t)(1 << 31))

typedef struct rec_strm {
	char *tcp_handle;
	/*
	 * out-goung bits
	 */
	int (*writeit)(char *, char *, int);
	char *out_base;	/* output buffer (points to frag header) */
	char *out_finger;	/* next output position */
	char *out_boundry;	/* data cannot up to this address */
	uint32_t *frag_header;	/* beginning of curren fragment */
	bool_t frag_sent;	/* true if buffer sent in middle of record */
	/*
	 * in-coming bits
	 */
	int (*readit)(char *, char *, int);
	u_long in_size;	/* fixed size of the input buffer */
	char *in_base;
	char *in_finger;	/* location of next byte to be had */
	char *in_boundry;	/* can read up to this location */
	long fbtbc;		/* fragment bytes to be consumed */
	bool_t last_frag;
	u_int sendsize;
	u_int recvsize;

	bool_t nonblock;
	bool_t in_haveheader;
	uint32_t in_header;
	char *in_hdrp;
	int in_hdrlen;
	int in_reclen;
	int in_received;
	int in_maxrec;
} RECSTREAM;

static u_int	fix_buf_size(u_int);
static bool_t	flush_out(RECSTREAM *, bool_t);
static bool_t	fill_input_buf(RECSTREAM *);
static bool_t	get_input_bytes(RECSTREAM *, char *, u_int);
static bool_t	set_input_fragment(RECSTREAM *);
static bool_t	skip_input_bytes(RECSTREAM *, long);
static bool_t	realloc_stream(RECSTREAM *, int);


/*
 * Create an xdr handle for xdrrec
 * xdrrec_create fills in xdrs.  Sendsize and recvsize are
 * send and recv buffer sizes (0 => use default).
 * tcp_handle is an opaque handle that is passed as the first parameter to
 * the procedures readit and writeit.  Readit and writeit are read and
 * write respectively.   They are like the system
 * calls expect that they take an opaque handle rather than an fd.
 */
void
xdrrec_create(
	XDR *xdrs,
	u_int sendsize,
	u_int recvsize,
	char *tcp_handle,
	/* like read, but pass it a tcp_handle, not sock */
	int (*readit)(char *, char *, int),
	/* like write, but pass it a tcp_handle, not sock */
	int (*writeit)(char *, char *, int))
{
	RECSTREAM *rstrm = mem_alloc(sizeof(RECSTREAM));

	if (rstrm == NULL) {
		warnx("xdrrec_create: out of memory");
		/* 
		 *  This is bad.  Should rework xdrrec_create to 
		 *  return a handle, and in this case return NULL
		 */
		return;
	}

	rstrm->sendsize = sendsize = fix_buf_size(sendsize);
	rstrm->out_base = malloc(rstrm->sendsize);
	if (rstrm->out_base == NULL) {
		warnx("xdrrec_create: out of memory");
		mem_free(rstrm, sizeof(RECSTREAM));
		return;
	}

	rstrm->recvsize = recvsize = fix_buf_size(recvsize);
	rstrm->in_base = malloc(recvsize);
	if (rstrm->in_base == NULL) {
		warnx("xdrrec_create: out of memory");
		mem_free(rstrm->out_base, sendsize);
		mem_free(rstrm, sizeof(RECSTREAM));
		return;
	}
	/*
	 * now the rest ...
	 */
	xdrs->x_ops = &xdrrec_ops;
	xdrs->x_private = rstrm;
	rstrm->tcp_handle = tcp_handle;
	rstrm->readit = readit;
	rstrm->writeit = writeit;
	rstrm->out_finger = rstrm->out_boundry = rstrm->out_base;
	rstrm->frag_header = (uint32_t *)(void *)rstrm->out_base;
	rstrm->out_finger += sizeof(uint32_t);
	rstrm->out_boundry += sendsize;
	rstrm->frag_sent = FALSE;
	rstrm->in_size = recvsize;
	rstrm->in_boundry = rstrm->in_base;
	rstrm->in_finger = (rstrm->in_boundry += recvsize);
	rstrm->fbtbc = 0;
	rstrm->last_frag = TRUE;
	rstrm->in_haveheader = FALSE;
	rstrm->in_hdrlen = 0;
	rstrm->in_hdrp = (char *)(void *)&rstrm->in_header;
	rstrm->nonblock = FALSE;
	rstrm->in_reclen = 0;
	rstrm->in_received = 0;
}


/*
 * The reoutines defined below are the xdr ops which will go into the
 * xdr handle filled in by xdrrec_create.
 */

static bool_t
xdrrec_getlong(XDR *xdrs, long *lp)
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	int32_t *buflp = (int32_t *)(void *)(rstrm->in_finger);
	int32_t mylong;

	/* first try the inline, fast case */
	if ((rstrm->fbtbc >= (long)sizeof(int32_t)) &&
		(((uintptr_t)rstrm->in_boundry - (uintptr_t)buflp) >= sizeof(int32_t))) {
		*lp = (long)ntohl((uint32_t)(*buflp));
		rstrm->fbtbc -= sizeof(int32_t);
		rstrm->in_finger += sizeof(int32_t);
	} else {
		if (! xdrrec_getbytes(xdrs, (char *)(void *)&mylong,
		    (u_int)sizeof(int32_t)))
			return (FALSE);
		*lp = (long)ntohl((uint32_t)mylong);
	}
	return (TRUE);
}

static bool_t
xdrrec_putlong(XDR *xdrs, const long *lp)
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	int32_t *dest_lp = ((int32_t *)(void *)(rstrm->out_finger));

	if ((rstrm->out_finger += sizeof(int32_t)) > rstrm->out_boundry) {
		/*
		 * this case should almost never happen so the code is
		 * inefficient
		 */
		rstrm->out_finger -= sizeof(int32_t);
		rstrm->frag_sent = TRUE;
		if (! flush_out(rstrm, FALSE))
			return (FALSE);
		dest_lp = ((int32_t *)(void *)(rstrm->out_finger));
		rstrm->out_finger += sizeof(int32_t);
	}
	*dest_lp = (int32_t)htonl((uint32_t)(*lp));
	return (TRUE);
}

static bool_t  /* must manage buffers, fragments, and records */
xdrrec_getbytes(XDR *xdrs, char *addr, u_int len)
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	u_int current;

	while (len > 0) {
		current = (u_int)rstrm->fbtbc;
		if (current == 0) {
			if (rstrm->last_frag)
				return (FALSE);
			if (! set_input_fragment(rstrm))
				return (FALSE);
			continue;
		}
		current = (len < current) ? len : current;
		if (! get_input_bytes(rstrm, addr, current))
			return (FALSE);
		addr += current; 
		rstrm->fbtbc -= current;
		len -= current;
	}
	return (TRUE);
}

static bool_t
xdrrec_putbytes(XDR *xdrs, const char *addr, u_int len)
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	size_t current;

	while (len > 0) {
		current = (size_t)((u_long)rstrm->out_boundry -
		    (u_long)rstrm->out_finger);
		current = (len < current) ? len : current;
		memmove(rstrm->out_finger, addr, current);
		rstrm->out_finger += current;
		addr += current;
		_DIAGASSERT(__type_fit(u_int, current));
		len -= (u_int)current;
		if (rstrm->out_finger == rstrm->out_boundry) {
			rstrm->frag_sent = TRUE;
			if (! flush_out(rstrm, FALSE))
				return (FALSE);
		}
	}
	return (TRUE);
}

static u_int
xdrrec_getpos(XDR *xdrs)
{
	RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	off_t pos;

	pos = lseek((int)(u_long)rstrm->tcp_handle, (off_t)0, 1);
	if (pos != -1)
		switch (xdrs->x_op) {

		case XDR_ENCODE:
			pos += rstrm->out_finger - rstrm->out_base;
			break;

		case XDR_DECODE:
			pos -= rstrm->in_boundry - rstrm->in_finger;
			break;

		default:
			pos = (off_t) -1;
			break;
		}
	return ((u_int) pos);
}

static bool_t
xdrrec_setpos(XDR *xdrs, u_int pos)
{
	RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	u_int currpos = xdrrec_getpos(xdrs);
	int delta = currpos - pos;
	char *newpos;

	if ((int)currpos != -1)
		switch (xdrs->x_op) {

		case XDR_ENCODE:
			newpos = rstrm->out_finger - delta;
			if ((newpos > (char *)(void *)(rstrm->frag_header)) &&
				(newpos < rstrm->out_boundry)) {
				rstrm->out_finger = newpos;
				return (TRUE);
			}
			break;

		case XDR_DECODE:
			newpos = rstrm->in_finger - delta;
			if ((delta < (int)(rstrm->fbtbc)) &&
				(newpos <= rstrm->in_boundry) &&
				(newpos >= rstrm->in_base)) {
				rstrm->in_finger = newpos;
				rstrm->fbtbc -= delta;
				return (TRUE);
			}
			break;

		case XDR_FREE:
			break;
		}
	return (FALSE);
}

static int32_t *
xdrrec_inline(XDR *xdrs, u_int len)
{
	RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	int32_t *buf = NULL;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		if ((rstrm->out_finger + len) <= rstrm->out_boundry) {
			buf = (int32_t *)(void *)rstrm->out_finger;
			rstrm->out_finger += len;
		}
		break;

	case XDR_DECODE:
		if ((len <= (u_int)rstrm->fbtbc) &&
			((rstrm->in_finger + len) <= rstrm->in_boundry)) {
			buf = (int32_t *)(void *)rstrm->in_finger;
			rstrm->fbtbc -= len;
			rstrm->in_finger += len;
		}
		break;

	case XDR_FREE:
		break;
	}
	return (buf);
}

static void
xdrrec_destroy(XDR *xdrs)
{
	RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;

	mem_free(rstrm->out_base, rstrm->sendsize);
	mem_free(rstrm->in_base, rstrm->recvsize);
	mem_free(rstrm, sizeof(RECSTREAM));
}


/*
 * Exported routines to manage xdr records
 */

/*
 * Before reading (deserializing from the stream, one should always call
 * this procedure to guarantee proper record alignment.
 */
bool_t
xdrrec_skiprecord(XDR *xdrs)
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	enum xprt_stat xstat;

	if (rstrm->nonblock) {
		if (__xdrrec_getrec(xdrs, &xstat, FALSE)) {
			rstrm->fbtbc = 0;
			return TRUE;
		}
		if (rstrm->in_finger == rstrm->in_boundry &&
		    xstat == XPRT_MOREREQS) {
			rstrm->fbtbc = 0;
			return TRUE;
		}
		return FALSE;
	}
	while (rstrm->fbtbc > 0 || (! rstrm->last_frag)) {
		if (! skip_input_bytes(rstrm, rstrm->fbtbc))
			return (FALSE);
		rstrm->fbtbc = 0;
		if ((! rstrm->last_frag) && (! set_input_fragment(rstrm)))
			return (FALSE);
	}
	rstrm->last_frag = FALSE;
	return (TRUE);
}

/*
 * Look ahead fuction.
 * Returns TRUE iff there is no more input in the buffer 
 * after consuming the rest of the current record.
 */
bool_t
xdrrec_eof(XDR *xdrs)
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);

	while (rstrm->fbtbc > 0 || (! rstrm->last_frag)) {
		if (!skip_input_bytes(rstrm, rstrm->fbtbc))
			return (TRUE);
		rstrm->fbtbc = 0;
		if ((!rstrm->last_frag) && (!set_input_fragment(rstrm)))
			return (TRUE);
	}
	if (rstrm->in_finger == rstrm->in_boundry)
		return (TRUE);
	return (FALSE);
}

/*
 * The client must tell the package when an end-of-record has occurred.
 * The second paraemters tells whether the record should be flushed to the
 * (output) tcp stream.  (This let's the package support batched or
 * pipelined procedure calls.)  TRUE => immmediate flush to tcp connection.
 */
bool_t
xdrrec_endofrecord(XDR *xdrs, bool_t sendnow)
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	u_long len;  /* fragment length */

	if (sendnow || rstrm->frag_sent ||
		((u_long)rstrm->out_finger + sizeof(uint32_t) >=
		(u_long)rstrm->out_boundry)) {
		rstrm->frag_sent = FALSE;
		return (flush_out(rstrm, TRUE));
	}
	len = (u_long)(rstrm->out_finger) - (u_long)(rstrm->frag_header) -
	   sizeof(uint32_t);
	*(rstrm->frag_header) = htonl((uint32_t)len | LAST_FRAG);
	rstrm->frag_header = (uint32_t *)(void *)rstrm->out_finger;
	rstrm->out_finger += sizeof(uint32_t);
	return (TRUE);
}

/*
 * Fill the stream buffer with a record for a non-blocking connection.
 * Return true if a record is available in the buffer, false if not.
 */
bool_t
__xdrrec_getrec(XDR *xdrs, enum xprt_stat *statp, bool_t expectdata)
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	ssize_t n;
	int fraglen;

	if (!rstrm->in_haveheader) {
		n = rstrm->readit(rstrm->tcp_handle, rstrm->in_hdrp,
		    (int)sizeof (rstrm->in_header) - rstrm->in_hdrlen);
		if (n == 0) {
			*statp = expectdata ? XPRT_DIED : XPRT_IDLE;
			return FALSE;
		}
		if (n < 0) {
			*statp = XPRT_DIED;
			return FALSE;
		}
		rstrm->in_hdrp += n;
		_DIAGASSERT(__type_fit(int, n));
		rstrm->in_hdrlen += (int)n;
		if (rstrm->in_hdrlen < (int)sizeof(rstrm->in_header)) {
			*statp = XPRT_MOREREQS;
			return FALSE;
		}
		rstrm->in_header = ntohl(rstrm->in_header);
		fraglen = (int)(rstrm->in_header & ~LAST_FRAG);
		if (fraglen == 0 || fraglen > rstrm->in_maxrec ||
		    (rstrm->in_reclen + fraglen) > rstrm->in_maxrec) {
			*statp = XPRT_DIED;
			return FALSE;
		}
		rstrm->in_reclen += fraglen;
		if ((u_int)rstrm->in_reclen > rstrm->recvsize) {
			if (!realloc_stream(rstrm, rstrm->in_reclen)) {
				*statp = XPRT_DIED;
				return FALSE;
			}
		}
		if (rstrm->in_header & LAST_FRAG) {
			rstrm->in_header &= ~LAST_FRAG;
			rstrm->last_frag = TRUE;
		}
	}

	n =  rstrm->readit(rstrm->tcp_handle,
	    rstrm->in_base + rstrm->in_received,
	    (rstrm->in_reclen - rstrm->in_received));

	if (n < 0) {
		*statp = XPRT_DIED;
		return FALSE;
	}

	if (n == 0) {
		*statp = expectdata ? XPRT_DIED : XPRT_IDLE;
		return FALSE;
	}

	_DIAGASSERT(__type_fit(int, n));
	rstrm->in_received += (int)n;

	if (rstrm->in_received == rstrm->in_reclen) {
		rstrm->in_haveheader = FALSE;
		rstrm->in_hdrp = (char *)(void *)&rstrm->in_header;
		rstrm->in_hdrlen = 0;
		if (rstrm->last_frag) {
			rstrm->fbtbc = rstrm->in_reclen;
			rstrm->in_boundry = rstrm->in_base + rstrm->in_reclen;
			rstrm->in_finger = rstrm->in_base;
			rstrm->in_reclen = rstrm->in_received = 0;
			*statp = XPRT_MOREREQS;
			return TRUE;
		}
	}

	*statp = XPRT_MOREREQS;
	return FALSE;
}

bool_t
__xdrrec_setnonblock(XDR *xdrs, int maxrec)
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);

	rstrm->nonblock = TRUE;
	if (maxrec == 0)
		maxrec = rstrm->recvsize;
	rstrm->in_maxrec = maxrec;
	return TRUE;
}


/*
 * Internal useful routines
 */
static bool_t
flush_out(RECSTREAM *rstrm, bool_t eor)
{
	uint32_t eormask = (eor == TRUE) ? LAST_FRAG : 0;
	uint32_t len = (uint32_t)((u_long)(rstrm->out_finger) - 
		(u_long)(rstrm->frag_header) - sizeof(uint32_t));

	*(rstrm->frag_header) = htonl(len | eormask);
	len = (uint32_t)((u_long)(rstrm->out_finger) - 
	    (u_long)(rstrm->out_base));
	if ((*(rstrm->writeit))(rstrm->tcp_handle, rstrm->out_base, (int)len)
		!= (int)len)
		return (FALSE);
	rstrm->frag_header = (uint32_t *)(void *)rstrm->out_base;
	rstrm->out_finger = (char *)rstrm->out_base + sizeof(uint32_t);
	return (TRUE);
}

static bool_t  /* knows nothing about records!  Only about input buffers */
fill_input_buf(RECSTREAM *rstrm)
{
	char *where;
	uint32_t i;
	int len;

	if (rstrm->nonblock)
		return FALSE;
	where = rstrm->in_base;
	i = (uint32_t)((u_long)rstrm->in_boundry % BYTES_PER_XDR_UNIT);
	where += i;
	len = (uint32_t)(rstrm->in_size - i);
	if ((len = (*(rstrm->readit))(rstrm->tcp_handle, where, len)) == -1)
		return (FALSE);
	rstrm->in_finger = where;
	where += len;
	rstrm->in_boundry = where;
	return (TRUE);
}

static bool_t  /* knows nothing about records!  Only about input buffers */
get_input_bytes(RECSTREAM *rstrm, char *addr, u_int len)
{
	u_int current;

	if (rstrm->nonblock) {
		if (len > ((uintptr_t)rstrm->in_boundry - (uintptr_t)rstrm->in_finger))
			return FALSE;
		memcpy(addr, rstrm->in_finger, len);
		rstrm->in_finger += len;
		return TRUE;
	}

	while (len > 0) {
		uintptr_t d = ((uintptr_t)rstrm->in_boundry -
		    (uintptr_t)rstrm->in_finger);
		_DIAGASSERT(__type_fit(u_int, d));
		current = (u_int)d;
		if (current == 0) {
			if (! fill_input_buf(rstrm))
				return (FALSE);
			continue;
		}
		current = (len < current) ? len : current;
		memmove(addr, rstrm->in_finger, current);
		rstrm->in_finger += current;
		addr += current;
		len -= current;
	}
	return (TRUE);
}

static bool_t  /* next two bytes of the input stream are treated as a header */
set_input_fragment(RECSTREAM *rstrm)
{
	uint32_t header;

	if (rstrm->nonblock)
		return FALSE;
	if (! get_input_bytes(rstrm, (char *)(void *)&header,
	    (u_int)sizeof(header)))
		return (FALSE);
	header = ntohl(header);
	rstrm->last_frag = ((header & LAST_FRAG) == 0) ? FALSE : TRUE;
	/*
	 * Sanity check. Try not to accept wildly incorrect
	 * record sizes. Unfortunately, the only record size
	 * we can positively identify as being 'wildly incorrect'
	 * is zero. Ridiculously large record sizes may look wrong,
	 * but we don't have any way to be certain that they aren't
	 * what the client actually intended to send us.
	 */
	if (header == 0)
		return(FALSE);
	rstrm->fbtbc = header & (~LAST_FRAG);
	return (TRUE);
}

static bool_t  /* consumes input bytes; knows nothing about records! */
skip_input_bytes(RECSTREAM *rstrm, long cnt)
{
	uint32_t current;

	while (cnt > 0) {
		current = (uint32_t)((long)rstrm->in_boundry - 
		    (long)rstrm->in_finger);
		if (current == 0) {
			if (! fill_input_buf(rstrm))
				return (FALSE);
			continue;
		}
		current = ((uint32_t)cnt < current) ? (uint32_t)cnt : current;
		rstrm->in_finger += current;
		cnt -= current;
	}
	return (TRUE);
}

static u_int
fix_buf_size(u_int s)
{

	if (s < 100)
		s = 4000;
	return (RNDUP(s));
}

/*
 * Reallocate the input buffer for a non-block stream.
 */
static bool_t
realloc_stream(RECSTREAM *rstrm, int size)
{
	ptrdiff_t diff;
	char *buf;

	if ((u_int)size > rstrm->recvsize) {
		buf = realloc(rstrm->in_base, (size_t)size);
		if (buf == NULL)
			return FALSE;
		diff = buf - rstrm->in_base;
		rstrm->in_finger += diff;
		rstrm->in_base = buf;
		rstrm->in_boundry = buf + size;
		rstrm->recvsize = size;
		rstrm->in_size = size;
	}

	return TRUE;
}
