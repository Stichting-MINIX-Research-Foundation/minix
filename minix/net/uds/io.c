/* UNIX Domain Sockets - io.c - sending and receiving */

#include "uds.h"
#include <sys/mman.h>

/*
 * Our UDS sockets do not have a send buffer.  They only have a receive buffer.
 * This receive buffer, when not empty, is split up in segments.  Each segment
 * may contain regular data, ancillary data, both, or (for SOCK_SEQPACKET and
 * (SOCK_DGRAM) neither.  There are two types of ancillary data: in-flight file
 * descriptors and sender credentials.  In addition, for SOCK_DGRAM sockets,
 * the segment may contain the sender's socket path (if the sender's socket is
 * bound).  Each segment has a header, containing the full segment size, the
 * size of the actual data in the segment (if any), and a flags field that
 * states which ancillary are associated with the segment (if any).  For
 * SOCK_STREAM type sockets, new data may be merged into a previous segment,
 * but only if it has no ancillary data.  For the other two socket types, each
 * packet has its own header.  The resulting behavior should be in line with
 * the POSIX "Socket Receive Queue" specification.
 *
 * More specifically, each segment consists of the following parts:
 * - always a five-byte header, containing a two-byte segment length (including
 *   the header, so always non-zero), a two-byte regular data length (zero or
 *   more), and a one-byte flags field which is a bitwise combination of
 *   UDS_HAS_{FD,CRED,PATH} flags;
 * - next, if UDS_HAS_CRED is set in the segment header: a sockcred structure;
 *   since this structure is variable-size, the structure is prepended by a
 *   single byte that contains the length of the structure (excluding the byte
 *   itself, thus ranging from sizeof(struct sockcred) to UDS_MAXCREDLEN);
 * - next, if UDS_HAS_PATH is set in the segment header:
 * - next, if the data length is non-zero, the actual regular data.
 * If the segment is not the last in the receive buffer, it is followed by the
 * next segment immediately afterward.  There is no alignment.
 *
 * It is the sender's responsibility to merge new data into the last segment
 * whenever possible, so that the receiver side never needs to consider more
 * than one segment at once.  In order to allow such merging, each receive
 * buffer has not only a tail and in-use length (pointing to the head when
 * combined) but also an offset from the tail to the last header, if any.  Note
 * that the receiver may over time still look at multiple segments for a single
 * request: this happens when a MSG_WAITALL request empties the buffer and then
 * blocks - the next piece of arriving data can then obviously not be merged.
 *
 * If a segment has the UDS_HAS_FD flag set, then one or more in-flight file
 * descriptors are associated with the segment.  These are stored in a separate
 * data structure, mainly to simplify cleaning up when the socket is shut down
 * for reading or closed.  That structure also contains the number of file
 * descriptors associated with the current segment, so this is not stored in
 * the segment itself.  As mentioned later, this may be changed in the future.
 *
 * On the sender side, there is a trade-off between fully utilizing the receive
 * buffer, and not repeatedly performing expensive actions for the same call:
 * it may be costly to determine exactly how many in-flight file descriptors
 * there will be (if any) and/or how much space is needed to store credentials.
 * We currently use the policy that we rather block/reject a send request that
 * may (just) have fit in the remaining part of the receive buffer, than obtain
 * the same information multiple times or keep state between callbacks.  In
 * practice this is not expected to make a difference, especially since
 * transfer of ancillary data should be rare anyway.
 */
/*
 * The current layout of the segment header is as follows.
 *
 * The first byte contains the upper eight bits of the total segment length.
 * The second byte contains the lower eight bits of the total segment length.
 * The third byte contains the upper eight bits of the data length.
 * The fourth byte contains the lower eight bits of the data length.
 * The fifth byte is a bitmask for ancillary data associated with the segment.
 */
#define UDS_HDRLEN	5

#define UDS_HAS_FDS	0x01	/* segment has in-flight file descriptors */
#define UDS_HAS_CRED	0x02	/* segment has sender credentials */
#define UDS_HAS_PATH	0x04	/* segment has source socket path */

#define UDS_MAXCREDLEN	SOCKCREDSIZE(NGROUPS_MAX)

#define uds_get_head(uds) 	\
	((size_t)((uds)->uds_tail + (uds)->uds_len) % UDS_BUF)
#define uds_get_last(uds)	\
	((size_t)((uds)->uds_tail + (uds)->uds_last) % UDS_BUF)
#define uds_advance(pos,add) (((pos) + (add)) % UDS_BUF)

/*
 * All in-flight file descriptors are (co-)owned by the UDS driver itself, as
 * local open file descriptors.  Like any other process, the UDS driver can not
 * have more than OPEN_MAX open file descriptors at any time.  Thus, this is
 * also the inherent maximum number of in-flight file descriptors.  Therefore,
 * we maintain a single pool of in-flight FD structures, and we associate these
 * structures with sockets as needed.
 */
static struct uds_fd uds_fds[OPEN_MAX];
static SIMPLEQ_HEAD(uds_freefds, uds_fd) uds_freefds;

static char uds_ctlbuf[UDS_CTL_MAX];
static int uds_ctlfds[UDS_CTL_MAX / sizeof(int)];

/*
 * Initialize the input/output part of the UDS service.
 */
void
uds_io_init(void)
{
	unsigned int slot;

	SIMPLEQ_INIT(&uds_freefds);

	for (slot = 0; slot < __arraycount(uds_fds); slot++)
		SIMPLEQ_INSERT_TAIL(&uds_freefds, &uds_fds[slot], ufd_next);
}

/*
 * Set up all input/output state for the given socket, which has just been
 * allocated.  As part of this, allocate memory for the receive buffer of the
 * socket.  Return OK or a negative error code.
 */
int
uds_io_setup(struct udssock * uds)
{

	/* TODO: decide if we should preallocate the memory. */
	if ((uds->uds_buf = mmap(NULL, UDS_BUF, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE, -1, 0)) == MAP_FAILED)
		return ENOMEM;

	uds->uds_tail = 0;
	uds->uds_len = 0;
	uds->uds_last = 0;

	SIMPLEQ_INIT(&uds->uds_fds);

	return OK;
}

/*
 * Clean up the input/output state for the given socket, which is about to be
 * freed.  As part of this, deallocate memory for the receive buffer and close
 * any file descriptors still in flight on the socket.
 */
void
uds_io_cleanup(struct udssock * uds)
{

	/* Close any in-flight file descriptors. */
	uds_io_reset(uds);

	/* Free the receive buffer memory. */
	if (munmap(uds->uds_buf, UDS_BUF) != 0)
		panic("UDS: munmap failed: %d", errno);
}

/*
 * The socket is being closed or shut down for reading.  If there are still any
 * in-flight file descriptors, theey will never be received anymore, so close
 * them now.
 */
void
uds_io_reset(struct udssock * uds)
{
	struct uds_fd *ufd;

	/*
	 * The UDS service may have the last and only reference to any of these
	 * file descriptors here.  For that reason, we currently disallow
	 * transfer of UDS file descriptors, because the close(2) here could
	 * block on a socket close operation back to us, leading to a deadlock.
	 * Also, we use a non-blocking variant of close(2), to prevent that we
	 * end up hanging on sockets with SO_LINGER turned on.
	 */
	SIMPLEQ_FOREACH(ufd, &uds->uds_fds, ufd_next) {
		dprintf(("UDS: closing local fd %d\n", ufd->ufd_fd));

		closenb(ufd->ufd_fd);
	}

	SIMPLEQ_CONCAT(&uds_freefds, &uds->uds_fds);

	/*
	 * If this reset happens as part of a shutdown, it might be done
	 * again on close, so ensure that it will find a clean state.  The
	 * receive buffer should never be looked at again either way, but reset
	 * it too just to be sure.
	 */
	uds->uds_tail = 0;
	uds->uds_len = 0;
	uds->uds_last = 0;

	SIMPLEQ_INIT(&uds->uds_fds);
}

/*
 * Return the maximum usable part of the receive buffer, in bytes.  The return
 * value is used for the SO_SNDBUF and SO_RCVBUF socket options.
 */
size_t
uds_io_buflen(void)
{

	/*
	 * TODO: it would be nicer if at least for SOCK_STREAM-type sockets, we
	 * could use the full receive buffer for data.  This would require that
	 * we store up to one header in the socket object rather than in the
	 * receive buffer.
	 */
	return UDS_BUF - UDS_HDRLEN;
}

/*
 * Fetch 'len' bytes starting from absolute position 'pos' into the receive
 * buffer of socket 'uds', and copy them into the buffer pointed to by 'ptr'.
 * Return the absolute position of the first byte after the fetched data in the
 * receive buffer.
 */
static size_t
uds_fetch(struct udssock * uds, size_t off, void * ptr, size_t len)
{
	size_t left;

	assert(off < UDS_BUF);

	left = UDS_BUF - off;
	if (len >= left) {
		memcpy(ptr, &uds->uds_buf[off], left);

		if ((len -= left) > 0)
			memcpy((char *)ptr + left, &uds->uds_buf[0], len);

		return len;
	} else {
		memcpy(ptr, &uds->uds_buf[off], len);

		return off + len;
	}
}

/*
 * Store 'len' bytes from the buffer pointed to by 'ptr' into the receive
 * buffer of socket 'uds', starting at absolute position 'pos' into the receive
 * buffer.  Return the absolute position of the first byte after the stored
 * data in the receive buffer.
 */
static size_t
uds_store(struct udssock * uds, size_t off, const void * ptr, size_t len)
{
	size_t left;

	assert(off < UDS_BUF);

	left = UDS_BUF - off;
	if (len >= left) {
		memcpy(&uds->uds_buf[off], ptr, left);

		if ((len -= left) > 0)
			memcpy(&uds->uds_buf[0], (const char *)ptr + left,
			    len);

		return len;
	} else {
		memcpy(&uds->uds_buf[off], ptr, len);

		return off + len;
	}
}

/*
 * Fetch a segment header previously stored in the receive buffer of socket
 * 'uds' at absolute position 'off'.  Return the absolute position of the first
 * byte after the header, as well as the entire segment length in 'seglen', the
 * length of the data in the segment in 'datalen', and the segment flags in
 * 'segflags'.
 */
static size_t
uds_fetch_hdr(struct udssock * uds, size_t off, size_t * seglen,
	size_t * datalen, unsigned int * segflags)
{
	unsigned char hdr[UDS_HDRLEN];

	off = uds_fetch(uds, off, hdr, sizeof(hdr));

	*seglen = ((size_t)hdr[0] << 8) | (size_t)hdr[1];
	*datalen = ((size_t)hdr[2] << 8) | (size_t)hdr[3];
	*segflags = hdr[4];

	assert(*seglen >= UDS_HDRLEN);
	assert(*seglen <= uds->uds_len);
	assert(*datalen <= *seglen - UDS_HDRLEN);
	assert(*segflags != 0 || *datalen == *seglen - UDS_HDRLEN);
	assert(!(*segflags & ~(UDS_HAS_FDS | UDS_HAS_CRED | UDS_HAS_PATH)));

	return off;
}

/*
 * Store a segment header in the receive buffer of socket 'uds' at absolute
 * position 'off', with the segment length 'seglen', the segment data length
 * 'datalen', and the segment flags 'segflags'.  Return the absolute receive
 * buffer position of the first data byte after the stored header.
 */
static size_t
uds_store_hdr(struct udssock * uds, size_t off, size_t seglen, size_t datalen,
	unsigned int segflags)
{
	unsigned char hdr[UDS_HDRLEN];

	assert(seglen <= USHRT_MAX);
	assert(datalen <= seglen);
	assert(segflags <= UCHAR_MAX);
	assert(!(segflags & ~(UDS_HAS_FDS | UDS_HAS_CRED | UDS_HAS_PATH)));

	hdr[0] = (seglen >> 8) & 0xff;
	hdr[1] = seglen & 0xff;
	hdr[2] = (datalen >> 8) & 0xff;
	hdr[3] = datalen & 0xff;
	hdr[4] = segflags;

	return uds_store(uds, off, hdr, sizeof(hdr));
}

/*
 * Perform initial checks on a send request, before it may potentially be
 * suspended.  Return OK if this send request is valid, or a negative error
 * code if it is not.
 */
int
uds_pre_send(struct sock * sock, size_t len, socklen_t ctl_len __unused,
	const struct sockaddr * addr, socklen_t addr_len __unused,
	endpoint_t user_endpt __unused, int flags)
{
	struct udssock *uds = (struct udssock *)sock;
	size_t pathlen;

	/*
	 * Reject calls with unknown flags.  Besides the flags handled entirely
	 * by libsockevent (which are not part of 'flags' here), that is all of
	 * them.  TODO: ensure that we should really reject all other flags
	 * rather than ignore them.
	 */
	if (flags != 0)
		return EOPNOTSUPP;

	/*
	 * Perform very basic address and message size checks on the send call.
	 * For non-stream sockets, we must reject packets that may never fit in
	 * the receive buffer, or otherwise (at least for SOCK_SEQPACKET) the
	 * send call may end up being suspended indefinitely.  Therefore, we
	 * assume the worst-case scenario, which is that a full set of
	 * credentials must be associated with the packet.  As a result, we may
	 * reject some large packets that could actually just fit.  Checking
	 * the peer's LOCAL_CREDS setting here is not safe: even if we know the
	 * peer already at all (for SOCK_DGRAM we do not), the send may still
	 * block and the option toggled before it unblocks.
	 */
	switch (uds_get_type(uds)) {
	case SOCK_STREAM:
		/* Nothing to check for this case. */
		break;

	case SOCK_SEQPACKET:
		if (len > UDS_BUF - UDS_HDRLEN - 1 - UDS_MAXCREDLEN)
			return EMSGSIZE;

		break;

	case SOCK_DGRAM:
		if (!uds_has_link(uds) && addr == NULL)
			return EDESTADDRREQ;

		/*
		 * The path is stored without null terminator, but with leading
		 * byte containing the path length--if there is a path at all.
		 */
		pathlen = (size_t)uds->uds_pathlen;
		if (pathlen > 0)
			pathlen++;

		if (len > UDS_BUF - UDS_HDRLEN - pathlen - 1 - UDS_MAXCREDLEN)
			return EMSGSIZE;

		break;

	default:
		assert(0);
	}

	return OK;
}

/*
 * Determine whether the (real or pretend) send request should be processed
 * now, suspended until later, or rejected based on the current socket state.
 * Return OK if the send request should be processed now.  Return SUSPEND if
 * the send request should be retried later.  Return an appropriate negative
 * error code if the send request should fail.
 */
static int
uds_send_test(struct udssock * uds, size_t len, socklen_t ctl_len, size_t min,
	int partial)
{
	struct udssock *conn;
	size_t avail, hdrlen, credlen;

	assert(!uds_is_shutdown(uds, SFL_SHUT_WR));

	if (uds_get_type(uds) != SOCK_DGRAM) {
		if (uds_is_connecting(uds))
			return SUSPEND;
		if (!uds_is_connected(uds) && !uds_is_disconnected(uds))
			return ENOTCONN;
		if (!uds_has_conn(uds))
			return EPIPE;

		conn = uds->uds_conn;

		if (uds_is_shutdown(conn, SFL_SHUT_RD))
			return EPIPE;

		/*
		 * For connection-type sockets, we now have to check if there
		 * is enough room in the receive buffer.  For SOCK_STREAM
		 * sockets, we must check if at least 'min' bytes can be moved
		 * into the receive buffer, at least if that is a reasonable
		 * value for ever making any forward progress at all.  For
		 * SOCK_SEQPACKET sockets, we must check if the entire packet
		 * of size 'len' can be stored in the receive buffer.  In both
		 * cases, we must take into account any metadata to store along
		 * with the data.
		 *
		 * Unlike in uds_pre_send(), we can now check safely whether
		 * the peer is expecting credentials, but we still don't know
		 * the actual size of the credentials, so again we take the
		 * maximum possible size.  The same applies to file descriptors
		 * transferred via control data: all we have the control length
		 * right now, which if non-zero we assume to mean there might
		 * be file descriptors.
		 *
		 * In both cases, the reason of overestimating is that actually
		 * getting accurate sizes, by obtaining credentials or copying
		 * in control data, is very costly.  We want to do that only
		 * when we are sure we will not suspend the send call after
		 * all.  It is no problem to overestimate how much space will
		 * be needed here, but not to underestimate: that could cause
		 * applications that use select(2) and non-blocking sockets to
		 * end up in a busy-wait loop.
		 */
		if (!partial && (conn->uds_flags & UDSF_PASSCRED))
			credlen = 1 + UDS_MAXCREDLEN;
		else
			credlen = 0;

		avail = UDS_BUF - conn->uds_len;

		if (uds_get_type(uds) == SOCK_STREAM) {
			/*
			 * Limit the low threshold to the maximum that can ever
			 * be sent at once.
			 */
			if (min > UDS_BUF - UDS_HDRLEN - credlen)
				min = UDS_BUF - UDS_HDRLEN - credlen;

			/*
			 * Suspend the call only if not even the low threshold
			 * is met.  Otherwise we may make (partial) progress.
			 */
			if (len > min)
				len = min;

			/*
			 * If the receive buffer already has at least one
			 * segment, and there are certainly no file descriptors
			 * to transfer now, and we do not have to store
			 * credentials either, then this segment can be merged
			 * with the previous one.  In that case, we need no
			 * space for a header.  That is certainly the case if
			 * we are resuming an already partially completed send.
			 */
			hdrlen = (avail == UDS_BUF || ctl_len != 0 ||
			    credlen > 0) ? UDS_HDRLEN : 0;
		} else
			hdrlen = UDS_HDRLEN;

		if (avail < hdrlen + credlen + len)
			return SUSPEND;
	}

	return OK;
}

/*
 * Get the destination peer for a send request.  The send test has already been
 * performed first.  On success, return OK, with a pointer to the peer socket
 * stored in 'peerp'.  On failure, return an appropriate error code.
 */
static int
uds_send_peer(struct udssock * uds, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt, struct udssock ** peerp)
{
	struct udssock *peer;
	int r;

	if (uds_get_type(uds) == SOCK_DGRAM) {
		if (!uds_has_link(uds)) {
			/* This was already checked in uds_pre_check(). */
			assert(addr != NULL);

			/*
			 * Find the socket identified by the given address.
			 * If it exists at all, see if it is a proper match.
			 */
			if ((r = uds_lookup(uds, addr, addr_len, user_endpt,
			    &peer)) != OK)
				return r;

			/*
			 * If the peer socket is connected to a target, it
			 * must be this socket.  Unfortunately, POSIX does not
			 * specify an error code for this.  We borrow Linux's.
			 */
			if (uds_has_link(peer) && peer->uds_link != uds)
				return EPERM;
		} else
			peer = uds->uds_link;

		/*
		 * If the receiving end will never receive this packet, we
		 * might as well not send it, so drop it immeiately.  Indicate
		 * as such to the caller, using NetBSD's chosen error code.
		 */
		if (uds_is_shutdown(peer, SFL_SHUT_RD))
			return ENOBUFS;
	} else {
		assert(uds_has_conn(uds));

		peer = uds->uds_conn;
	}

	*peerp = peer;
	return OK;
}

/*
 * Generate a new segment for the current send request, or arrange things such
 * that new data can be merged with a previous segment.  As part of this,
 * decide whether we can merge data at all.  The segment will be merged if, and
 * only if, all of the following requirements are met:
 *
 *   1) the socket is of type SOCK_STREAM;
 *   2) there is a previous segment in the receive buffer;
 *   3) there is no ancillary data for the current send request.
 *
 * Also copy in regular data (if any), retrieve the sender's credentials (if
 * needed), and copy over the source path (if applicable).  However, do not yet
 * commit the segment (or the new part to be merged), because the send request
 * may still fail for other reasons.
 *
 * On success, return the length of the new segment (or, when merging, the
 * length to be added to the last segment), as well as a flag indicating
 * whether we are merging into the last segment in 'mergep', the length of the
 * (new) data in the segment in 'datalenp', and the new segment's flags in
 * 'segflagsp' (always zero when merging).  Note that a return value of zero
 * implies that we are merging zero extra bytes into the last segment, which
 * means that effectively nothing changes; in that case the send call will be
 * cut short and return zero to the caller as well.  On failure, return a
 * negative error code.
 */
static int
uds_send_data(struct udssock * uds, struct udssock * peer,
	const struct sockdriver_data * data, size_t len, size_t off,
	endpoint_t user_endpt, unsigned int nfds, int * __restrict mergep,
	size_t * __restrict datalenp, unsigned int * __restrict segflagsp)
{
	struct sockcred sockcred;
	gid_t groups[NGROUPS_MAX];
	iovec_t iov[2];
	unsigned int iovcnt, segflags;
	unsigned char lenbyte;
	size_t credlen, pathlen, datalen, seglen;
	size_t avail, pos, left;
	int r, merge;

	/*
	 * At this point we should add the data to the peer's receive buffer.
	 * In the case of SOCK_STREAM sockets, we should add as much of the
	 * data as possible and suspend the call to send the rest later, if
	 * applicable.  In the case of SOCK_DGRAM sockets, we should drop the
	 * packet if it does not fit in the buffer.
	 *
	 * Due to the checks in uds_can_send(), we know for sure that we no
	 * longer have to suspend without making any progress at this point.
	 */
	segflags = (nfds > 0) ? UDS_HAS_FDS : 0;

	/*
	 * Obtain the credentials now.  Doing so allows us to determine how
	 * much space we actually need for them.
	 */
	if (off == 0 && (peer->uds_flags & UDSF_PASSCRED)) {
		memset(&sockcred, 0, sizeof(sockcred));

		if ((r = getsockcred(user_endpt, &sockcred, groups,
		    __arraycount(groups))) != OK)
			return r;

		/*
		 * getsockcred(3) returns the total number of groups for the
		 * process, which may exceed the size of the given array.  Our
		 * groups array should always be large enough for all groups,
		 * but we check to be sure anyway.
		 */
		assert(sockcred.sc_ngroups <= (int)__arraycount(groups));

		credlen = 1 + SOCKCREDSIZE(sockcred.sc_ngroups);

		segflags |= UDS_HAS_CRED;
	} else
		credlen = 0;

	/* For bound source datagram sockets, include the source path. */
	if (uds_get_type(uds) == SOCK_DGRAM && uds->uds_pathlen != 0) {
		pathlen = (size_t)uds->uds_pathlen + 1;

		segflags |= UDS_HAS_PATH;
	} else
		pathlen = 0;

	avail = UDS_BUF - peer->uds_len;

	if (uds_get_type(uds) == SOCK_STREAM) {
		/*
		 * Determine whether we can merge data into the previous
		 * segment.  This is a more refined version of the test in
		 * uds_can_send(), as we now know whether there are actually
		 * any FDs to transfer.
		 */
		merge = (peer->uds_len != 0 && nfds == 0 && credlen == 0);

		/* Determine how much we can send at once. */
		if (!merge) {
			assert(avail > UDS_HDRLEN + credlen);
			datalen = avail - UDS_HDRLEN - credlen;
		} else
			datalen = avail;

		if (datalen > len)
			datalen = len;

		/* If we cannot make progress, we should have suspended.. */
		assert(datalen != 0 || len == 0);
	} else {
		merge = FALSE;

		datalen = len;
	}
	assert(datalen <= len);
	assert(datalen <= UDS_BUF);

	/*
	 * Compute the total amount of space we need for the segment in the
	 * receive buffer.  Given that we have done will-it-fit tests in
	 * uds_can_send() for SOCK_STREAM and SOCK_SEQPACKET, there is only one
	 * case left where the result may not fit, and that is for SOCK_DGRAM
	 * packets.  In that case, we drop the packet.  POSIX says we should
	 * throw an error in that case, and that is also what NetBSD does.
	 */
	if (!merge)
		seglen = UDS_HDRLEN + credlen + pathlen + datalen;
	else
		seglen = datalen;

	if (seglen > avail) {
		assert(uds_get_type(uds) == SOCK_DGRAM);

		/* Drop the packet, borrowing NetBSD's chosen error code. */
		return ENOBUFS;
	}

	/*
	 * Generate the full segment, but do not yet update the buffer head.
	 * We may still run into an error (copying in file descriptors) or even
	 * decide that nothing gets sent after all (if there are no data or
	 * file descriptors).  If we are merging the new data into the previous
	 * segment, do not generate a header.
	 */
	pos = uds_get_head(peer);

	/* Generate the header, if needed. */
	if (!merge)
		pos = uds_store_hdr(peer, pos, seglen, datalen, segflags);
	else
		assert(segflags == 0);

	/* Copy in and store the sender's credentials, if desired. */
	if (credlen > 0) {
		assert(credlen >= 1 + sizeof(sockcred));
		assert(credlen <= UCHAR_MAX);

		lenbyte = credlen - 1;
		pos = uds_store(peer, pos, &lenbyte, 1);

		if (sockcred.sc_ngroups > 0) {
			pos = uds_store(peer, pos, &sockcred,
			    offsetof(struct sockcred, sc_groups));
			pos = uds_store(peer, pos, groups,
			    sockcred.sc_ngroups * sizeof(gid_t));
		} else
			pos = uds_store(peer, pos, &sockcred,
			    sizeof(sockcred));
	}

	/* Store the sender's address if any.  Datagram sockets only. */
	if (pathlen > 0) {
		assert(pathlen > 1);
		assert(pathlen <= UCHAR_MAX);

		lenbyte = uds->uds_pathlen;
		pos = uds_store(peer, pos, &lenbyte, 1);
		pos = uds_store(peer, pos, uds->uds_path, pathlen - 1);
	}

	/* Lastly, copy in the actual data (if any) from the caller. */
	if (datalen > 0) {
		iov[0].iov_addr = (vir_bytes)&peer->uds_buf[pos];
		left = UDS_BUF - pos;

		if (left < datalen) {
			assert(left > 0);
			iov[0].iov_size = left;
			iov[1].iov_addr = (vir_bytes)&peer->uds_buf[0];
			iov[1].iov_size = datalen - left;
			iovcnt = 2;
		} else {
			iov[0].iov_size = datalen;
			iovcnt = 1;
		}

		if ((r = sockdriver_vcopyin(data, off, iov, iovcnt)) != OK)
			return r;
	}

	*mergep = merge;
	*datalenp = datalen;
	*segflagsp = segflags;
	return seglen;
}

/*
 * Copy in control data for the current send request, and extract any file
 * descriptors to be transferred.  Do not yet duplicate the file descriptors,
 * but rather store a list in a temporary buffer: the send request may still
 * fail in which case we want to avoid having to undo the duplication.
 *
 * On success, return the number of (zero or more) file descriptors extracted
 * from the request and stored in the temporary buffer.  On failure, return a
 * negative error code.
 */
static int
uds_send_ctl(const struct sockdriver_data * ctl, socklen_t ctl_len,
	endpoint_t user_endpt)
{
	struct msghdr msghdr;
	struct cmsghdr *cmsg;
	socklen_t left;
	unsigned int i, n, nfds;
	int r;

	/*
	 * Copy in the control data.  We can spend a lot of effort copying in
	 * the data in small chunks, and change the receiving side to do the
	 * same, but it is really not worth it: applications never send a whole
	 * lot of file descriptors at once, and the buffer size is currently
	 * such that the UDS service itself will exhaust its OPEN_MAX limit
	 * anyway if they do.
	 */
	if (ctl_len > sizeof(uds_ctlbuf))
		return ENOBUFS;

	if ((r = sockdriver_copyin(ctl, 0, uds_ctlbuf, ctl_len)) != OK)
		return r;

	if (ctl_len < sizeof(uds_ctlbuf))
		memset(&uds_ctlbuf[ctl_len], 0, sizeof(uds_ctlbuf) - ctl_len);

	/*
	 * Look for any file descriptors, and store their remote file
	 * descriptor numbers into a temporary array.
	 */
	memset(&msghdr, 0, sizeof(msghdr));
	msghdr.msg_control = uds_ctlbuf;
	msghdr.msg_controllen = ctl_len;

	nfds = 0;
	r = OK;

	/*
	 * The sender may provide file descriptors in multiple chunks.
	 * Currently we do not preserve these chunk boundaries, instead
	 * generating one single chunk with all file descriptors for the
	 * segment upon receipt.  If needed, we can fairly easily adapt this
	 * later.
	 */
	for (cmsg = CMSG_FIRSTHDR(&msghdr); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msghdr, cmsg)) {
		/*
		 * Check for bogus lengths.  There is no excuse for this;
		 * either the caller does not know what they are doing or we
		 * are looking at a hacking attempt.
		 */
		assert((socklen_t)((char *)cmsg - uds_ctlbuf) <= ctl_len);
		left = ctl_len - (socklen_t)((char *)cmsg - uds_ctlbuf);
		assert(left >= CMSG_LEN(0)); /* guaranteed by CMSG_xxHDR */

		if (cmsg->cmsg_len < CMSG_LEN(0) || cmsg->cmsg_len > left) {
			printf("UDS: malformed control data from %u\n",
			    user_endpt);
			r = EINVAL;
			break;
		}

		if (cmsg->cmsg_level != SOL_SOCKET ||
		    cmsg->cmsg_type != SCM_RIGHTS)
			continue;

		n = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);

		for (i = 0; i < n; i++) {
			/*
			 * Copy the file descriptor to the temporary buffer,
			 * whose size is based on the control data buffer, so
			 * it is always large enough to contain all FDs.
			 */
			assert(nfds < __arraycount(uds_ctlfds));

			memcpy(&uds_ctlfds[nfds],
			    &((int *)CMSG_DATA(cmsg))[i], sizeof(int));

			nfds++;
		}
	}

	return nfds;
}

/*
 * Actually duplicate any file descriptors that we extracted from the sender's
 * control data and stored in our temporary buffer.  On success, return OK,
 * with all file descriptors stored in file descriptor objects that are
 * appended to the socket's list of in-flight FD objects.  Thus, on success,
 * the send request may no longer fail.  On failure, return a negative error
 * code, with any partial duplication undone.
 */
static int
uds_send_fds(struct udssock * peer, unsigned int nfds, endpoint_t user_endpt)
{
	SIMPLEQ_HEAD(, uds_fd) fds;
	struct uds_fd *ufd;
	unsigned int i;
	int r;

	SIMPLEQ_INIT(&fds);

	for (i = 0; i < nfds; i++) {
		if (SIMPLEQ_EMPTY(&uds_freefds)) {
			/* UDS itself may already have OPEN_MAX FDs. */
			r = ENFILE;
			break;
		}

		/*
		 * The caller may have given an invalid FD, or UDS itself may
		 * unexpectedly have run out of available file descriptors etc.
		 */
		if ((r = copyfd(user_endpt, uds_ctlfds[i], COPYFD_FROM)) < 0)
			break;

		ufd = SIMPLEQ_FIRST(&uds_freefds);
		SIMPLEQ_REMOVE_HEAD(&uds_freefds, ufd_next);

		ufd->ufd_fd = r;
		ufd->ufd_count = 0;

		SIMPLEQ_INSERT_TAIL(&fds, ufd, ufd_next);

		dprintf(("UDS: copied in fd %d -> %d\n", uds_ctlfds[i], r));
	}

	/* Did we experience an error while copying in the file descriptors? */
	if (r < 0) {
		/* Revert the successful copyfd() calls made so far. */
		SIMPLEQ_FOREACH(ufd, &fds, ufd_next) {
			dprintf(("UDS: closing local fd %d\n", ufd->ufd_fd));

			closenb(ufd->ufd_fd);
		}

		SIMPLEQ_CONCAT(&uds_freefds, &fds);

		return r;
	}

	/*
	 * Success.  If there were any file descriptors at all, add them to the
	 * peer's list of in-flight file descriptors.  Assign the number of
	 * file descriptors copied in to the first file descriptor object, so
	 * that we know how many to copy out (or discard) for this segment.
	 * Also set the UDS_HAS_FDS flag on the segment.
	 */
	ufd = SIMPLEQ_FIRST(&fds);
	ufd->ufd_count = nfds;

	SIMPLEQ_CONCAT(&peer->uds_fds, &fds);

	return OK;
}

/*
 * The current send request is successful or at least has made progress.
 * Commit the new segment or, if we decided to merge the new data into the last
 * segment, update the header of the last segment.  Also wake up the receiving
 * side, because there will now be new data to receive.
 */
static void
uds_send_advance(struct udssock * uds, struct udssock * peer, size_t datalen,
	int merge, size_t seglen, unsigned int segflags)
{
	size_t pos, prevseglen, prevdatalen;

	/*
	 * For non-datagram sockets, credentials are sent only once after
	 * setting the LOCAL_CREDS option.  After that, the option is unset.
	 */
	if ((segflags & UDS_HAS_CRED) && uds_get_type(uds) != SOCK_DGRAM)
		peer->uds_flags &= ~UDSF_PASSCRED;

	if (merge) {
		assert(segflags == 0);

		pos = uds_get_last(peer);

		(void)uds_fetch_hdr(peer, pos, &prevseglen, &prevdatalen,
		    &segflags);

		peer->uds_len += seglen;
		assert(peer->uds_len <= UDS_BUF);

		seglen += prevseglen;
		datalen += prevdatalen;
		assert(seglen <= UDS_BUF);

		uds_store_hdr(peer, pos, seglen, datalen, segflags);
	} else {
		peer->uds_last = peer->uds_len;

		peer->uds_len += seglen;
		assert(peer->uds_len <= UDS_BUF);
	}

	/* Now that there are new data, wake up the receiver side. */
	sockevent_raise(&peer->uds_sock, SEV_RECV);
}

/*
 * Process a send request.  Return OK if the send request has successfully
 * completed, SUSPEND if it should be tried again later, or a negative error
 * code on failure.  In all cases, the values of 'off' and 'ctl_off' must be
 * updated if any progress has been made; if either is non-zero, libsockevent
 * will return the partial progress rather than an error code.
 */
int
uds_send(struct sock * sock, const struct sockdriver_data * data, size_t len,
	size_t * off, const struct sockdriver_data * ctl, socklen_t ctl_len,
	socklen_t * ctl_off, const struct sockaddr * addr, socklen_t addr_len,
	endpoint_t user_endpt, int flags __unused, size_t min)
{
	struct udssock *uds = (struct udssock *)sock;
	struct udssock *peer;
	size_t seglen, datalen = 0 /*gcc*/;
	unsigned int nfds, segflags = 0 /*gcc*/;
	int r, partial, merge = 0 /*gcc*/;

	dprintf(("UDS: send(%d,%zu,%zu,%u,%u,0x%x)\n",
	    uds_get_id(uds), len, (off != NULL) ? *off : 0, ctl_len,
	    (ctl_off != NULL) ? *ctl_off : 0, flags));

	partial = (off != NULL && *off > 0);

	/*
	 * First see whether we can process this send call at all right now.
	 * Most importantly, for connected sockets, if the peer's receive
	 * buffer is full, we may have to suspend the call until some space has
	 * been freed up.
	 */
	if ((r = uds_send_test(uds, len, ctl_len, min, partial)) != OK)
		return r;

	/*
	 * Then get the peer socket.  For connected sockets, this is trivial.
	 * For unconnected sockets, it may involve a lookup of the given
	 * address.
	 */
	if ((r = uds_send_peer(uds, addr, addr_len, user_endpt, &peer)) != OK)
		return r;

	/*
	 * We now know for sure that we will not suspend this call without
	 * making any progress.  However, the call may still fail.  Copy in
	 * control data first now, so that we know whether there are any file
	 * descriptors to transfer.  This aspect may determine whether or not
	 * we can merge data with a previous segment.  Do not actually copy in
	 * the actual file descriptors yet, because that is much harder to undo
	 * in case of a failure later on.
	 */
	if (ctl_len > 0) {
		/* We process control data once, in full. */
		assert(*ctl_off == 0);

		if ((r = uds_send_ctl(ctl, ctl_len, user_endpt)) < 0)
			return r;
		nfds = (unsigned int)r;
	} else
		nfds = 0;

	/*
	 * Now generate a new segment, or (if possible) merge new data into the
	 * last segment.  Since the call may still fail, prepare the segment
	 * but do not update the buffer head yet.  Note that the segment
	 * contains not just regular data (in fact it may contain no data at
	 * all) but (also) certain ancillary data.
	 */
	if ((r = uds_send_data(uds, peer, data, len, *off, user_endpt, nfds,
	    &merge, &datalen, &segflags)) <= 0)
		return r;
	seglen = (size_t)r;

	/*
	 * If we extracted any file descriptors from the control data earlier,
	 * copy them over to ourselves now.  The resulting in-flight file
	 * descriptors are stored in a separate data structure.  This is the
	 * last point where the send call may actually fail.
	 */
	if (nfds > 0) {
		if ((r = uds_send_fds(peer, nfds, user_endpt)) != OK)
			return r;
	}

	/*
	 * The transmission is now known to be (partially) successful.  Commit
	 * the new work by moving the receive buffer head.
	 */
	uds_send_advance(uds, peer, datalen, merge, seglen, segflags);

	/*
	 * Register the result.  For stream-type sockets, the expected behavior
	 * is that all data be sent, and so we may still have to suspend the
	 * call after partial progress.  Otherwise, we are now done.  Either
	 * way, we are done with the control data, so mark it as consumed.
	 */
	*off += datalen;
	*ctl_off += ctl_len;
	if (uds_get_type(uds) == SOCK_STREAM && datalen < len)
		return SUSPEND;
	else
		return OK;
}

/*
 * Test whether a send request would block.  The given 'min' parameter contains
 * the minimum number of bytes that should be possible to send without blocking
 * (the low send watermark).  Return SUSPEND if the send request would block,
 * or any other error code if it would not.
 */
int
uds_test_send(struct sock * sock, size_t min)
{
	struct udssock *uds = (struct udssock *)sock;

	return uds_send_test(uds, min, 0, min, FALSE /*partial*/);
}

/*
 * Perform initial checks on a receive request, before it may potentially be
 * suspended.  Return OK if this receive request is valid, or a negative error
 * code if it is not.
 */
int
uds_pre_recv(struct sock * sock __unused, endpoint_t user_endpt __unused,
	int flags)
{

	/*
	 * Reject calls with unknown flags.  TODO: ensure that we should really
	 * reject all other flags rather than ignore them.
	 */
	if ((flags & ~(MSG_PEEK | MSG_WAITALL | MSG_CMSG_CLOEXEC)) != 0)
		return EOPNOTSUPP;

	return OK;
}

/*
 * Determine whether the (real or pretend) receive request should be processed
 * now, suspended until later, or rejected based on the current socket state.
 * Return OK if the receive request should be processed now, along with a first
 * indication whether the call may still be suspended later in 'may_block'.
 * Return SUSPEND if the receive request should be retried later.  Return an
 * appropriate negative error code if the receive request should fail.
 */
static int
uds_recv_test(struct udssock * uds, size_t len, size_t min, int partial,
	int * may_block)
{
	size_t seglen, datalen;
	unsigned int segflags;
	int r;

	/*
	 * If there are any pending data, those should always be received
	 * first.  However, if there is nothing to receive, then whether we
	 * should suspend the receive call or fail immediately depends on other
	 * conditions.  We first look at these other conditions.
	 */
	r = OK;

	if (uds_get_type(uds) != SOCK_DGRAM) {
		if (uds_is_connecting(uds))
			r = SUSPEND;
		else if (!uds_is_connected(uds) && !uds_is_disconnected(uds))
			r = ENOTCONN;
		else if (!uds_has_conn(uds) ||
		    uds_is_shutdown(uds->uds_conn, SFL_SHUT_WR))
			r = SOCKEVENT_EOF;
	}

	if (uds->uds_len == 0) {
		/*
		 * For stream-type sockets, we use the policy: if no regular
		 * data is requested, then end the call without receiving
		 * anything.  For packet-type sockets, the request should block
		 * until there is a packet to discard, though.
		 */
		if (r != OK || (uds_get_type(uds) == SOCK_STREAM && len == 0))
			return r;

		return SUSPEND;
	}

	/*
	 * For stream-type sockets, we should still suspend the call if fewer
	 * than 'min' bytes are available right now, and there is a possibility
	 * that more data may arrive later.  More may arrive later iff 'r' is
	 * OK (i.e., no EOF or error will follow) and, in case we already
	 * received some partial results, there is not already a next segment
	 * with ancillary data (i.e, nonzero segment flags), or in any case
	 * there isn't more than one segment in the buffer.  Limit 'min' to the
	 * maximum that can ever be received, though.  Since that is difficult
	 * in our case, we check whether the buffer is entirely full instead.
	 */
	if (r == OK && uds_get_type(uds) == SOCK_STREAM && min > 0 &&
	    uds->uds_len < UDS_BUF) {
		assert(uds->uds_len >= UDS_HDRLEN);

		(void)uds_fetch_hdr(uds, uds->uds_tail, &seglen, &datalen,
		    &segflags);

		if (datalen < min && seglen == uds->uds_len &&
		    (!partial || segflags == 0))
			return SUSPEND;
	}

	/*
	 * Also start the decision process as to whether we should suspend the
	 * current call if MSG_WAITALL is given.  Unfortunately there is no one
	 * place where we can conveniently do all the required checks.
	 */
	if (may_block != NULL)
		*may_block = (r == OK && uds_get_type(uds) == SOCK_STREAM);
	return OK;
}

/*
 * Receive regular data, and possibly the source path, from the tail segment in
 * the receive buffer.  On success, return the positive non-zero length of the
 * tail segment, with 'addr' and 'addr_len' modified to store the source
 * address if applicable, the result flags in 'rflags' updated as appropriate,
 * the tail segment's data length stored in 'datalen', the number of received
 * regular data bytes stored in 'reslen', the segment flags stored in
 * 'segflags', and the absolute receive buffer position of the credentials in
 * the segment stored in 'credpos' if applicable.  Since the receive call may
 * still fail, this function must not yet update the tail or any other aspect
 * of the receive buffer.  Return zero if the current receive call was already
 * partially successful (due to MSG_WAITALL) and can no longer make progress,
 * and thus should be ended.  Return a negative error code on failure.
 */
static int
uds_recv_data(struct udssock * uds, const struct sockdriver_data * data,
	size_t len, size_t off, struct sockaddr * addr, socklen_t * addr_len,
	int * __restrict rflags, size_t * __restrict datalen,
	size_t * __restrict reslen, unsigned int * __restrict segflags,
	size_t * __restrict credpos)
{
	iovec_t iov[2];
	unsigned char lenbyte;
	unsigned int iovcnt;
	size_t pos, seglen, left;
	int r;

	pos = uds_fetch_hdr(uds, uds->uds_tail, &seglen, datalen, segflags);

	/*
	 * If a partially completed receive now runs into a segment that cannot
	 * be logically merged with the previous one (because it has at least
	 * one segment flag set, meaning it has ancillary data), then we must
	 * shortcut the receive now.
	 */
	if (off != 0 && *segflags != 0)
		return OK;

	/*
	 * As stated, for stream-type sockets, we choose to ignore zero-size
	 * receive calls.  This has the consequence that reading a zero-sized
	 * segment (with ancillary data) requires a receive request for at
	 * least one regular data byte.  Such a receive call would then return
	 * zero.  The problem with handling zero-data receive requests is that
	 * we need to know whether the current segment is terminated (i.e., no
	 * more data can possibly be merged into it later), which is a test
	 * that we rather not perform, not in the least because we do not know
	 * whether there is an error pending on the socket.
	 *
	 * For datagrams, we currently allow a zero-size receive call to
	 * discard the next datagram.
	 *
	 * TODO: compare this against policies on other platforms.
	 */
	if (len == 0 && uds_get_type(uds) == SOCK_STREAM)
		return OK;

	/*
	 * We have to skip the credentials for now: these are copied out as
	 * control data, and thus will (well, may) be looked at when dealing
	 * with the control data.  For the same reason, we do not even look at
	 * UDS_HAS_FDS here.
	 */
	if (*segflags & UDS_HAS_CRED) {
		*credpos = pos;

		pos = uds_fetch(uds, pos, &lenbyte, 1);
		pos = uds_advance(pos, (size_t)lenbyte);
	}

	/*
	 * Copy out the source address, but only if the (datagram) socket is
	 * not connected.  TODO: even when it is connected, it may still
	 * receive packets sent to it from other sockets *before* being
	 * connected, and the receiver has no way of knowing that those packets
	 * did not come from its new peer.  Ideally, the older packets should
	 * be dropped..
	 */
	if (*segflags & UDS_HAS_PATH) {
		pos = uds_fetch(uds, pos, &lenbyte, 1);

		if (uds_get_type(uds) == SOCK_DGRAM && !uds_has_link(uds))
			uds_make_addr((const char *)&uds->uds_buf[pos],
			    (size_t)lenbyte, addr, addr_len);

		pos = uds_advance(pos, (size_t)lenbyte);
	}

	/*
	 * We can receive no more data than those that are present in the
	 * segment, obviously.  For stream-type sockets, any more data that
	 * could have been received along with the current data would have been
	 * merged in the current segment, so we need not search for any next
	 * segments.
	 *
	 * For non-stream sockets, the caller may receive less than a whole
	 * packet if it supplied a small buffer.  In that case, the rest of the
	 * packet will be discarded (but not here yet!) and the caller gets
	 * the MSG_TRUNC flag in its result, if it was using sendmsg(2) anyway.
	 */
	if (len > *datalen)
		len = *datalen;
	else if (len < *datalen && uds_get_type(uds) != SOCK_STREAM)
		*rflags |= MSG_TRUNC;

	/* Copy out the data to the caller. */
	if (len > 0) {
		iov[0].iov_addr = (vir_bytes)&uds->uds_buf[pos];
		left = UDS_BUF - pos;

		if (left < len) {
			iov[0].iov_size = left;
			iov[1].iov_addr = (vir_bytes)&uds->uds_buf[0];
			iov[1].iov_size = len - left;
			iovcnt = 2;
		} else {
			iov[0].iov_size = len;
			iovcnt = 1;
		}

		if ((r = sockdriver_vcopyout(data, off, iov, iovcnt)) != OK)
			return r;
	}

	*reslen = len;
	assert(seglen > 0 && seglen <= INT_MAX);
	return (int)seglen;
}

/*
 * The current segment has associated file descriptors.  If possible, copy out
 * all file descriptors to the receiver, and generate and copy out a chunk of
 * control data that contains their file descriptor numbers.  If not all
 * file descriptors fit in the receiver's buffer, or if any error occurs, no
 * file descriptors are copied out.
 */
static int
uds_recv_fds(struct udssock * uds, const struct sockdriver_data * ctl,
	socklen_t ctl_len, socklen_t ctl_off, endpoint_t user_endpt, int flags)
{
	struct msghdr msghdr;
	struct cmsghdr *cmsg;
	struct uds_fd *ufd;
	unsigned int i, nfds;
	socklen_t chunklen, chunkspace;
	int r, fd, what;

	/* See how many file descriptors should be part of this chunk. */
	assert(!SIMPLEQ_EMPTY(&uds->uds_fds));
	ufd = SIMPLEQ_FIRST(&uds->uds_fds);
	nfds = ufd->ufd_count;
	assert(nfds > 0);

	/*
	 * We produce and copy out potentially unaligned chunks, using
	 * CMSG_LEN, but return the aligned size at the end, using CMSG_SPACE.
	 * This may leave "gap" bytes unchanged in userland, but that should
	 * not be a problem.  By producing unaligned chunks, we eliminate a
	 * potential boundary case where the unaligned chunk passed in (by the
	 * sender) no longer fits in the same buffer after being aligned here.
	 */
	chunklen = CMSG_LEN(sizeof(int) * nfds);
	chunkspace = CMSG_SPACE(sizeof(int) * nfds);
	assert(chunklen <= sizeof(uds_ctlbuf));
	if (chunklen > ctl_len)
		return 0; /* chunk would not fit, so produce nothing instead */
	if (chunkspace > ctl_len)
		chunkspace = ctl_len;

	memset(&msghdr, 0, sizeof(msghdr));
	msghdr.msg_control = uds_ctlbuf;
	msghdr.msg_controllen = sizeof(uds_ctlbuf);

	memset(uds_ctlbuf, 0, chunklen);
	cmsg = CMSG_FIRSTHDR(&msghdr);
	cmsg->cmsg_len = chunklen;
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;

	/*
	 * Copy the group's local file descriptors to the target endpoint, and
	 * store the resulting remote file descriptors in the chunk buffer.
	 */
	r = OK;

	for (i = 0; i < nfds; i++) {
		assert(ufd != SIMPLEQ_END(&uds->uds_fds));
		assert(i == 0 || ufd->ufd_count == 0);

		what = COPYFD_TO;
		if (flags & MSG_CMSG_CLOEXEC)
			what |= COPYFD_CLOEXEC;

		/* Failure may happen legitimately here (e.g., EMFILE). */
		if ((r = copyfd(user_endpt, ufd->ufd_fd, what)) < 0)
			break; /* we keep our progress so far in 'i' */

		fd = r;

		dprintf(("UDS: copied out fd %d -> %d\n", ufd->ufd_fd, fd));

		memcpy(&((int *)CMSG_DATA(cmsg))[i], &fd, sizeof(int));

		ufd = SIMPLEQ_NEXT(ufd, ufd_next);
	}

	/* If everything went well so far, copy out the produced chunk. */
	if (r >= 0)
		r = sockdriver_copyout(ctl, ctl_off, uds_ctlbuf, chunklen);

	/*
	 * Handle errors.  At this point, the 'i' variable contains the number
	 * of file descriptors that have already been successfully copied out.
	 */
	if (r < 0) {
		/* Revert the successful copyfd() calls made so far. */
		while (i-- > 0) {
			memcpy(&fd, &((int *)CMSG_DATA(cmsg))[i], sizeof(int));

			(void)copyfd(user_endpt, fd, COPYFD_CLOSE);
		}

		return r;
	}

	/*
	 * Success.  Return the aligned size of the produced chunk, if the
	 * given length permits it.  From here on, the receive call may no
	 * longer fail, as that would result in lost file descriptors.
	 */
	return chunkspace;
}

/*
 * Generate and copy out a chunk of control data with the sender's credentials.
 * Return the aligned chunk size on success, or a negative error code on
 * failure.
 */
static int
uds_recv_cred(struct udssock * uds, const struct sockdriver_data * ctl,
	socklen_t ctl_len, socklen_t ctl_off, size_t credpos)
{
	struct msghdr msghdr;
	struct cmsghdr *cmsg;
	socklen_t chunklen, chunkspace;
	unsigned char lenbyte;
	size_t credlen;
	int r;

	/*
	 * Since the sender side already did the hard work of producing the
	 * (variable-size) sockcred structure as it should be received, there
	 * is relatively little work to be done here.
	 */
	credpos = uds_fetch(uds, credpos, &lenbyte, 1);
	credlen = (size_t)lenbyte;

	chunklen = CMSG_LEN(credlen);
	chunkspace = CMSG_SPACE(credlen);
	assert(chunklen <= sizeof(uds_ctlbuf));
	if (chunklen > ctl_len)
		return 0; /* chunk would not fit, so produce nothing instead */
	if (chunkspace > ctl_len)
		chunkspace = ctl_len;

	memset(&msghdr, 0, sizeof(msghdr));
	msghdr.msg_control = uds_ctlbuf;
	msghdr.msg_controllen = sizeof(uds_ctlbuf);

	memset(uds_ctlbuf, 0, chunklen);
	cmsg = CMSG_FIRSTHDR(&msghdr);
	cmsg->cmsg_len = chunklen;
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_CREDS;

	uds_fetch(uds, credpos, CMSG_DATA(cmsg), credlen);

	if ((r = sockdriver_copyout(ctl, ctl_off, uds_ctlbuf, chunklen)) != OK)
		return r;

	return chunkspace;
}

/*
 * Copy out control data for the ancillary data associated with the current
 * segment, if any.  Return OK on success, at which point the current receive
 * call may no longer fail.  'rflags' may be updated with additional result
 * flags.  Return a negative error code on failure.
 */
static int
uds_recv_ctl(struct udssock * uds, const struct sockdriver_data * ctl,
	socklen_t ctl_len, socklen_t * ctl_off, endpoint_t user_endpt,
	int flags, unsigned int segflags, size_t credpos, int * rflags)
{
	int r;

	/*
	 * We first copy out all file descriptors, if any.  We put them in one
	 * SCM_RIGHTS chunk, even if the sender put them in separate SCM_RIGHTS
	 * chunks.  We believe that this should not cause application-level
	 * issues, but if it does, we can change that later with some effort.
	 * We then copy out credentials, if any.
	 *
	 * We copy out each control chunk independently of the others, and also
	 * perform error recovery on a per-chunk basis.  This implies the
	 * following.  If producing or copying out the first chunk fails, the
	 * entire recvmsg(2) call will fail with an appropriate error.  If
	 * producing or copying out any subsequent chunk fails, the recvmsg(2)
	 * call will still return the previously generated chunks (a "short
	 * control read" if you will) as well as the MSG_CTRUNC flag.  This
	 * approach is simple and clean, and it guarantees that we can always
	 * copy out at least as many file descriptors as we copied in for this
	 * segment, even if credentials are present as well.  However, the
	 * approach does cause slightly more overhead when there are multiple
	 * chunks per call, as those are copied out separately.
	 *
	 * Since the generated SCM_RIGHTS chunk is never larger than the
	 * originally received SCM_RIGHTS chunk, the temporary "uds_ctlbuf"
	 * buffer is always large enough to contain the chunk in its entirety.
	 * SCM_CREDS chunks should always fit easily as well.
	 *
	 * The MSG_CTRUNC flag will be returned iff not the entire user-given
	 * control buffer was filled and not all control chunks were delivered.
	 * Our current implementation does not deliver partial chunks.  NetBSD
	 * does, except for SCM_RIGHTS chunks.
	 *
	 * TODO: get rid of the redundancy in processing return values.
	 */
	if (segflags & UDS_HAS_FDS) {
		r = uds_recv_fds(uds, ctl, ctl_len, *ctl_off, user_endpt,
		    flags);

		/*
		 * At this point, 'r' contains one of the following:
		 *
		 *   r > 0	a chunk of 'r' bytes was added successfully.
		 *   r == 0	not enough space left; the chunk was not added.
		 *   r < 0	an error occurred; the chunk was not added.
		 */
		if (r < 0 && *ctl_off == 0)
			return r;

		if (r > 0) {
			ctl_len -= r;
			*ctl_off += r;
		} else
			*rflags |= MSG_CTRUNC;
	}

	if (segflags & UDS_HAS_CRED) {
		r = uds_recv_cred(uds, ctl, ctl_len, *ctl_off, credpos);

		/* As above. */
		if (r < 0 && *ctl_off == 0)
			return r;

		if (r > 0) {
			ctl_len -= r;
			*ctl_off += r;
		} else
			*rflags |= MSG_CTRUNC;
	}

	return OK;
}

/*
 * The current receive request is successful or, in the case of MSG_WAITALL,
 * has made progress.  Advance the receive buffer tail, either by discarding
 * the entire tail segment or by generating a new, smaller tail segment that
 * contains only the regular data left to be received from the original tail
 * segment.  Also wake up the sending side for connection-oriented sockets if
 * applicable, because there may now be room for more data to be sent.  Update
 * 'may_block' if we are now sure that the call may not block on MSG_WAITALL
 * after all.
 */
static void
uds_recv_advance(struct udssock * uds, size_t seglen, size_t datalen,
	size_t reslen, unsigned int segflags, int * may_block)
{
	struct udssock *conn;
	struct uds_fd *ufd;
	size_t delta, nseglen, advance;
	unsigned int nfds;

	/* Note that 'reslen' may be legitimately zero. */
	assert(reslen <= datalen);

	if (uds_get_type(uds) != SOCK_STREAM && reslen < datalen)
		reslen = datalen;

	delta = datalen - reslen;

	if (delta == 0) {
		/*
		 * Fully consume the tail segment.  We advance the tail by the
		 * full segment length, thus moving up to either the next
		 * segment in the receive buffer, or an empty receive buffer.
		 */
		advance = seglen;

		uds->uds_tail = uds_advance(uds->uds_tail, advance);
	} else {
		/*
		 * Partially consume the tail segment.  We put a new segment
		 * header right in front of the remaining data, which obviously
		 * always fits.  Since any ancillary data was consumed along
		 * with the first data byte of the segment, the new segment has
		 * no ancillary data anymore (and thus a zero flags field).
		 */
		nseglen = UDS_HDRLEN + delta;
		assert(nseglen < seglen);

		advance = seglen - nseglen;

		uds->uds_tail = uds_advance(uds->uds_tail, advance);

		uds_store_hdr(uds, uds->uds_tail, nseglen, delta, 0);
	}

	/*
	 * For datagram-oriented sockets, we always consume at least a header.
	 * For stream-type sockets, we either consume a zero-data segment along
	 * with its ancillary data, or we consume at least one byte from a
	 * segment that does have regular data.  In all other cases, the
	 * receive call has already been ended by now.  Thus, we always advance
	 * the tail of the receive buffer here.
	 */
	assert(advance > 0);

	/*
	 * The receive buffer's used length (uds_len) and pointer to the
	 * previous segment header (uds_last) are offsets from the tail.  Now
	 * that we have moved the tail, we need to adjust these accordingly.
	 * If the buffer is now empty, reset the tail to the buffer start so as
	 * to avoid splitting inter-process copies whenever possible.
	 */
	assert(uds->uds_len >= advance);
	uds->uds_len -= advance;

	if (uds->uds_len == 0)
		uds->uds_tail = 0;

	/*
	 * If uds_last is zero here, it was pointing to the segment we just
	 * (partially) consumed.  By leaving it zero, it will still point to
	 * the new or next segment.
	 */
	if (uds->uds_last > 0) {
		assert(uds->uds_len > 0);
		assert(uds->uds_last >= advance);
		uds->uds_last -= advance;
	}

	/*
	 * If there were any file descriptors associated with this segment,
	 * close and free them now.
	 */
	if (segflags & UDS_HAS_FDS) {
		assert(!SIMPLEQ_EMPTY(&uds->uds_fds));
		ufd = SIMPLEQ_FIRST(&uds->uds_fds);
		nfds = ufd->ufd_count;
		assert(nfds > 0);

		while (nfds-- > 0) {
			assert(!SIMPLEQ_EMPTY(&uds->uds_fds));
			ufd = SIMPLEQ_FIRST(&uds->uds_fds);
			SIMPLEQ_REMOVE_HEAD(&uds->uds_fds, ufd_next);

			dprintf(("UDS: closing local fd %d\n", ufd->ufd_fd));

			closenb(ufd->ufd_fd);

			SIMPLEQ_INSERT_TAIL(&uds_freefds, ufd, ufd_next);
		}
	}

	/*
	 * If there is now any data left in the receive buffer, then there has
	 * been a reason that we haven't received it.  For stream sockets, that
	 * reason is that the next segment has ancillary data.  In any case,
	 * this means we should never block the current receive operation
	 * waiting for more data.  Otherwise, we may block on MSG_WAITALL.
	 */
	if (uds->uds_len > 0)
		*may_block = FALSE;

	/*
	 * If the (non-datagram) socket has a peer that is not shut down for
	 * writing, see if it can be woken up to send more data.  Note that
	 * the event will never be processed immediately.
	 */
	if (uds_is_connected(uds)) {
		assert(uds_get_type(uds) != SOCK_DGRAM);

		conn = uds->uds_conn;

		if (!uds_is_shutdown(conn, SFL_SHUT_WR))
			sockevent_raise(&conn->uds_sock, SEV_SEND);
	}
}

/*
 * Process a receive request.  Return OK if the receive request has completed
 * successfully, SUSPEND if it should be tried again later, SOCKEVENT_EOF if an
 * end-of-file condition is reached, or a negative error code on failure.  In
 * all cases, the values of 'off' and 'ctl_off' must be updated if any progress
 * has been made; if either is non-zero, libsockevent will return the partial
 * progress rather than an error code or EOF.
 */
int
uds_recv(struct sock * sock, const struct sockdriver_data * data, size_t len,
	size_t * off, const struct sockdriver_data * ctl, socklen_t ctl_len,
	socklen_t * ctl_off, struct sockaddr * addr, socklen_t * addr_len,
	endpoint_t user_endpt, int flags, size_t min, int * rflags)
{
	struct udssock *uds = (struct udssock *)sock;
	size_t seglen, datalen, reslen = 0 /*gcc*/, credpos = 0 /*gcc*/;
	unsigned int segflags;
	int r, partial, may_block = 0 /*gcc*/;

	dprintf(("UDS: recv(%d,%zu,%zu,%u,%u,0x%x)\n",
	    uds_get_id(uds), len, (off != NULL) ? *off : 0, ctl_len,
	    (ctl_off != NULL) ? *ctl_off : 0, flags));

	/*
	 * Start by testing whether anything can be received at all, or whether
	 * an error or EOF should be returned instead, or whether the receive
	 * call should be suspended until later otherwise.  If no (regular or
	 * control) data can be received, or if this was a test for select,
	 * we bail out right after.
	 */
	partial = (off != NULL && *off > 0);

	if ((r = uds_recv_test(uds, len, min, partial, &may_block)) != OK)
		return r;

	/*
	 * Copy out regular data, if any.  Do this before copying out control
	 * data, because the latter is harder to undo on failure.  This data
	 * copy function returns returns OK (0) if we are to return a result of
	 * zero bytes (which is *not* EOF) to the caller without doing anything
	 * else.  The function returns a nonzero positive segment length if we
	 * should carry on with the receive call (as it happens, all its other
	 * returned values may in fact be zero).
	 */
	if ((r = uds_recv_data(uds, data, len, *off, addr, addr_len, rflags,
	    &datalen, &reslen, &segflags, &credpos)) <= 0)
		return r;
	seglen = (size_t)r;

	/*
	 * Copy out control data, if any: transfer and copy out records of file
	 * descriptors, and/or copy out sender credentials.  This is the last
	 * part of the call that may fail.
	 */
	if ((r = uds_recv_ctl(uds, ctl, ctl_len, ctl_off, user_endpt, flags,
	    segflags, credpos, rflags)) != OK)
		return r;

	/*
	 * Now that the call has succeeded, move the tail of the receive
	 * buffer, unless we were merely peeking.
	 */
	if (!(flags & MSG_PEEK))
		uds_recv_advance(uds, seglen, datalen, reslen, segflags,
		    &may_block);
	else
		may_block = FALSE;

	/*
	 * If the MSG_WAITALL flag was given, we may still have to suspend the
	 * call after partial success.  In particular, the receive call may
	 * suspend after partial success if all of these conditions are met:
	 *
	 *   1) the socket is a stream-type socket;
	 *   2) MSG_WAITALL is set;
	 *   3) MSG_PEEK is not set;
	 *   4) MSG_DONTWAIT is not set (tested upon return);
	 *   5) the socket must not have a pending error (tested upon return);
	 *   6) the socket must not be shut down for reading (tested later);
	 *   7) the socket must still be connected to a peer (no EOF);
	 *   8) the peer must not have been shut down for writing (no EOF);
	 *   9) the next segment, if any, contains no ancillary data.
	 *
	 * Together, these points guarantee that the call could conceivably
	 * receive more after being resumed.  Points 4 to 6 are covered by
	 * libsockevent, which will end the call even if we return SUSPEND
	 * here.  Due to segment merging, we cover point 9 by checking that
	 * there is currently no next segment at all.  Once a new segment
	 * arrives, the ancillary-data test is done then.
	 */
	*off += reslen;
	if ((flags & MSG_WAITALL) && reslen < len && may_block)
		return SUSPEND;
	else
		return OK;
}

/*
 * Test whether a receive request would block.  The given 'min' parameter
 * contains the minimum number of bytes that should be possible to receive
 * without blocking (the low receive watermark).  Return SUSPEND if the send
 * request would block.  Otherwise, return any other error code (including OK
 * or SOCKEVENT_EOF), and if 'size' is not a NULL pointer, it should be filled
 * with the number of bytes available for receipt right now (if not zero).
 * Note that if 'size' is not NULL, 'min' will always be zero.
 */
int
uds_test_recv(struct sock * sock, size_t min, size_t * size)
{
	struct udssock *uds = (struct udssock *)sock;
	size_t seglen;
	unsigned int segflags;
	int r;

	if ((r = uds_recv_test(uds, min, min, FALSE /*partial*/,
	    NULL /*may_block*/)) == SUSPEND)
		return r;

	if (size != NULL && uds->uds_len > 0)
		(void)uds_fetch_hdr(uds, uds->uds_tail, &seglen, size,
		    &segflags);

	return r;
}
