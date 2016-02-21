/* The protocol family independent socket driver framework. */
/*
 * The table below lists all supported socket driver requests, along with
 * information on whether the request handler may suspend the call for later
 * processing, and which message layout is to be used for the request and reply
 * messages for each call.
 *
 * Type			May suspend	Request	layout	Reply layout
 * ----			-----------	--------------	------------
 * SDEV_SOCKET		no		socket		socket_reply
 * SDEV_SOCKETPAIR	no		socket		socket_reply
 * SDEV_BIND		yes		addr		reply
 * SDEV_CONNECT		yes		addr		reply
 * SDEV_LISTEN		no		simple		reply
 * SDEV_ACCEPT		yes		addr		accept_reply
 * SDEV_SEND		yes		sendrecv	reply
 * SDEV_RECV		yes		sendrecv	recv_reply
 * SDEV_IOCTL		yes		ioctl		reply
 * SDEV_SETSOCKOPT	no		getset		reply
 * SDEV_GETSOCKOPT	no		getset		reply
 * SDEV_GETSOCKNAME	no		getset		reply
 * SDEV_GETPEERNAME	no		getset		reply
 * SDEV_SHUTDOWN	no		simple		reply
 * SDEV_CLOSE		yes		simple		reply
 * SDEV_CANCEL		n/a		simple		-
 * SDEV_SELECT		yes (special)	select		select_reply
 *
 * The request message layouts are prefixed with "m_vfs_lsockdriver_".  The
 * reply message layouts are prefixed with "m_lsockdriver_vfs_", and use
 * message types of the format SDEV_{,SOCKET_,ACCEPT_,RECV_}REPLY, matching the
 * listed reply layout.  One exception is SDEV_CANCEL, which itself has no
 * reply at all.  The other exception is SDEV_SELECT, which has two reply
 * codes: SDEV_SELECT1_REPLY (for immediate replies) and SDEV_SELECT2_REPLY
 * (for late replies), both using the select_reply reply layout.
 */

#include <minix/drivers.h>
#include <minix/sockdriver.h>
#include <sys/ioctl.h>

static int running;

/*
 * Announce that we are up and running, after a fresh start or a restart.
 */
void
sockdriver_announce(void)
{
	static const char *sockdriver_prefix = "drv.sck.";
	char key[DS_MAX_KEYLEN], label[DS_MAX_KEYLEN];
	int r;

	/* Publish a driver up event. */
	if ((r = ds_retrieve_label_name(label, sef_self())) != OK)
		panic("sockdriver: unable to get own label: %d", r);

	snprintf(key, sizeof(key), "%s%s", sockdriver_prefix, label);
	if ((r = ds_publish_u32(key, DS_DRIVER_UP, DSF_OVERWRITE)) != OK)
		panic("sockdriver: unable to publish driver up event: %d", r);
}

/*
 * Copy data from the caller into the local address space.  Return OK or a
 * negative error code.
 */
int
sockdriver_copyin(const struct sockdriver_data * __restrict data, size_t off,
	void * __restrict ptr, size_t len)
{

	assert(data != NULL);
	assert(off + len <= data->_sd_len);
	assert(data->_sd_endpt != SELF);
	assert(GRANT_VALID(data->_sd_grant));

	return sys_safecopyfrom(data->_sd_endpt, data->_sd_grant, off,
	    (vir_bytes)ptr, len);
}

/*
 * Copy data from the local address space to the caller.  Return OK or a
 * negative error code.
 */
int
sockdriver_copyout(const struct sockdriver_data * __restrict data, size_t off,
	const void * __restrict ptr, size_t len)
{

	assert(data != NULL);
	assert(off + len <= data->_sd_len);
	assert(data->_sd_endpt != SELF);
	assert(GRANT_VALID(data->_sd_grant));

	return sys_safecopyto(data->_sd_endpt, data->_sd_grant, off,
	    (vir_bytes)ptr, len);
}

/*
 * Copy data between the caller and the local address space, using a vector of
 * at most SOCKDRIVER_IOV_MAX buffers.  Return OK or an error code.
 */
static int
sockdriver_vcopy(const struct sockdriver_data * __restrict data, size_t off,
	const iovec_t * __restrict iov, unsigned int iovcnt, int copyin)
{
	static struct vscp_vec vec[SOCKDRIVER_IOV_MAX];
	unsigned int i;

	assert(iov != NULL);
	assert(iovcnt <= __arraycount(vec));

	/* We allow zero-element vectors, because we are nice. */
	if (iovcnt == 0)
		return OK;

	/*
	 * Do not use a vector copy operation for single-element copies, as
	 * this saves the kernel from having to copy in the vector itself.
	 */
	if (iovcnt == 1) {
		if (copyin)
			return sockdriver_copyin(data, off,
			    (void *)iov->iov_addr, iov->iov_size);
		else
			return sockdriver_copyout(data, off,
			    (const void *)iov->iov_addr, iov->iov_size);
	}

	assert(data != NULL);
	assert(data->_sd_endpt != SELF);
	assert(GRANT_VALID(data->_sd_grant));

	for (i = 0; i < iovcnt; i++, iov++) {
		if (copyin) {
			vec[i].v_from = data->_sd_endpt;
			vec[i].v_to = SELF;
		} else {
			vec[i].v_from = SELF;
			vec[i].v_to = data->_sd_endpt;
		}
		vec[i].v_gid = data->_sd_grant;
		vec[i].v_offset = off;
		vec[i].v_addr = iov->iov_addr;
		vec[i].v_bytes = iov->iov_size;

		off += iov->iov_size;
	}

	assert(off <= data->_sd_len);

	return sys_vsafecopy(vec, iovcnt);
}

/*
 * Copy data from the caller into the local address space, using a vector of
 * buffers.  Return OK or a negative error code.
 */
int
sockdriver_vcopyin(const struct sockdriver_data * __restrict data, size_t off,
	const iovec_t * __restrict iov, unsigned int iovcnt)
{

	return sockdriver_vcopy(data, off, iov, iovcnt, TRUE /*copyin*/);
}

/*
 * Copy data from the local address space to the caller, using a vector of
 * buffers.  Return OK or a negative error code.
 */
int
sockdriver_vcopyout(const struct sockdriver_data * __restrict data, size_t off,
	const iovec_t * __restrict iov, unsigned int iovcnt)
{

	return sockdriver_vcopy(data, off, iov, iovcnt, FALSE /*copyin*/);
}

/*
 * Copy data from the caller into the local address space, using socket option
 * semantics: fail the call with EINVAL if the given 'optlen' is not equal to
 * the given 'len'.  Return OK or a negative error code.
 */
int
sockdriver_copyin_opt(const struct sockdriver_data * __restrict data,
	void * __restrict ptr, size_t len, socklen_t optlen)
{

	if (len != optlen)
		return EINVAL;
	else
		return sockdriver_copyin(data, 0, ptr, len);
}

/*
 * Copy data from the local address space to the caller, using socket option
 * semantics: limit the size of the copied-out data to the size pointed to by
 * 'optlen', and return the possibly truncated size in 'optlen' on success.
 * Return OK or a negative error code.
 */
int
sockdriver_copyout_opt(const struct sockdriver_data * __restrict data,
	const void * __restrict ptr, size_t len, socklen_t * __restrict optlen)
{
	int r;

	if (len > *optlen)
		len = *optlen;

	if ((r = sockdriver_copyout(data, 0, ptr, len)) == OK)
		*optlen = len;

	return r;
}

/*
 * Compress a sockdriver_data structure to a smaller variant that stores only
 * the fields that are not already stored redundantly in/as the given 'call'
 * and 'len' parameters.  The typical use case here this call suspension.  In
 * that case, the caller will already store 'call' and 'len' as is, and can
 * save memory by storing a packed version of 'data' rather than that structure
 * itself.  Return OK on success, with 'pack' containing a compressed version
 * of 'data'.  Return EINVAL if the given parameters do not match; this would
 * typically be a sign that the calling application messed up badly.
 */
int
sockdriver_pack_data(struct sockdriver_packed_data * pack,
	const struct sockdriver_call * call,
	const struct sockdriver_data * data, size_t len)
{

	if (data->_sd_endpt != call->sc_endpt)
		return EINVAL;
	if (data->_sd_len != len)
		return EINVAL;

	pack->_spd_grant = data->_sd_grant;
	return OK;
}

/*
 * Decompress a previously packed sockdriver data structure into a full
 * sockdriver_data structure, with the help of the given 'call' and 'len'
 * parameters.  Return the unpacked version of 'pack' in 'data'.  This function
 * always succeeds.
 */
void
sockdriver_unpack_data(struct sockdriver_data * data,
	const struct sockdriver_call * call,
	const struct sockdriver_packed_data * pack, size_t len)
{

	data->_sd_endpt = call->sc_endpt;
	data->_sd_grant = pack->_spd_grant;
	data->_sd_len = len;
}

/*
 * Send a reply to a request.
 */
static void
send_reply(endpoint_t endpt, int type, message * m_ptr)
{
	int r;

	m_ptr->m_type = type;

	if ((r = asynsend(endpt, m_ptr)) != OK)
		printf("sockdriver: sending reply to %d failed (%d)\n",
		    endpt, r);
}

/*
 * Send a reply which takes only a result code and no additional reply fields.
 */
static void
send_generic_reply(endpoint_t endpt, sockreq_t req, int reply)
{
	message m;

	assert(reply != SUSPEND && reply != EDONTREPLY);

	memset(&m, 0, sizeof(m));
	m.m_lsockdriver_vfs_reply.req_id = req;
	m.m_lsockdriver_vfs_reply.status = reply;

	send_reply(endpt, SDEV_REPLY, &m);
}

/*
 * Send a reply to an earlier suspended request which takes only a result code
 * and no additional reply fields.
 */
void
sockdriver_reply_generic(const struct sockdriver_call * call, int reply)
{

	send_generic_reply(call->sc_endpt, call->sc_req, reply);
}

/*
 * Send a reply to a socket or a socketpair request.  Since these calls may not
 * be suspended, this function is used internally only.
 */
static void
send_socket_reply(endpoint_t endpt, sockreq_t req, sockid_t reply,
	sockid_t reply2)
{
	message m;

	assert(reply != SUSPEND && reply != EDONTREPLY);

	memset(&m, 0, sizeof(m));
	m.m_lsockdriver_vfs_socket_reply.req_id = req;
	m.m_lsockdriver_vfs_socket_reply.sock_id = reply;
	m.m_lsockdriver_vfs_socket_reply.sock_id2 = reply2;

	send_reply(endpt, SDEV_SOCKET_REPLY, &m);
}

/*
 * Send a reply to an earlier suspended accept request.  The given reply is
 * either a socket identifier (>= 0) or an error code (< 0).  On success, an
 * address must be given as 'addr', and its nonzero length must be given as
 * 'addr_len'.
 */
void
sockdriver_reply_accept(const struct sockdriver_call * __restrict call,
	sockid_t reply, struct sockaddr * __restrict addr, socklen_t addr_len)
{
	sockid_t id;
	message m;

	assert(reply != SUSPEND && reply != EDONTREPLY);

	/*
	 * If the accept was successful, copy out the address, if requested.
	 * If the copy fails, send both a valid socket ID and an error to VFS.
	 * VFS will then close the newly created socket immediately, and return
	 * the error to the caller.
	 *
	 * While not particularly nice, the general behavior of closing the
	 * socket after accepting it seems to be common among other OSes for
	 * address copy errors.  Most importantly, it frees the socket driver
	 * from having to deal with address copy errors itself.
	 *
	 * Letting VFS close the socket is also not all that great.  However,
	 * it is the lesser evil compared to the two main alternatives: 1)
	 * immediately calling sdr_close() from here, which would seriously
	 * complicate writing socket drivers due to sockets disappearing from
	 * under it, so to speak, and 2) queuing a forged incoming SDEV_CLOSE
	 * request, for which we do not have the necessary infrastructure.
	 * Additionally, VFS may close the newly accepted socket when out of
	 * other required resources anyway, so logically this fits in well.
	 * The only real price to pay is a slightly uglier message protocol.
	 *
	 * Copying out the address *length* is not our responsibility at all;
	 * if VFS chooses to do this itself (as opposed to letting libc do it),
	 * it too will have to close the socket on failure, using a separate
	 * close call.  This is always multithreading-safe because userland can
	 * not access the accepted socket yet anyway.
	 */
	if (reply >= 0) {
		id = reply;
		reply = OK;
	} else
		id = -1;

	if (reply == OK && GRANT_VALID(call->_sc_grant)) {
		if (addr == NULL || addr_len == 0)
			panic("libsockdriver: success but no address given");

		if (addr_len > call->_sc_len)
			addr_len = call->_sc_len; /* truncate addr and len */

		if (addr_len > 0) {
			reply = sys_safecopyto(call->sc_endpt, call->_sc_grant,
			    0, (vir_bytes)addr, addr_len);

			/* Intentionally leave 'id' set on failure here. */
		}
	} else
		addr_len = 0;	/* not needed, but cleaner */

	memset(&m, 0, sizeof(m));
	m.m_lsockdriver_vfs_accept_reply.req_id = call->sc_req;
	m.m_lsockdriver_vfs_accept_reply.sock_id = id;
	m.m_lsockdriver_vfs_accept_reply.status = reply;
	m.m_lsockdriver_vfs_accept_reply.len = addr_len;

	send_reply(call->sc_endpt, SDEV_ACCEPT_REPLY, &m);
}

/*
 * Send a reply to an earlier suspended receive call.  The given reply code is
 * the number of regular data bytes received (>= 0) or an error code (< 0).
 * On success, for connectionless sockets, 'addr' must point to the source
 * address and 'addr_len' must contain the address length; for connection-
 * oriented sockets, 'addr_len' must be zero, in which case 'addr' is ignored.
 */
void
sockdriver_reply_recv(const struct sockdriver_call * __restrict call,
	int reply, socklen_t ctl_len, struct sockaddr * __restrict addr,
	socklen_t addr_len, int flags)
{
	message m;
	int r;

	assert(reply != SUSPEND && reply != EDONTREPLY);

	/*
	 * If applicable, copy out the address.  If this fails, the result is
	 * loss of the data received; in the case of AF_UNIX, this may include
	 * references to file descriptors already created in the receiving
	 * process.  At least Linux and NetBSD behave this way as well, which
	 * is not an excuse to be lazy, but we need to change just about
	 * everything for the worse (including having additional grants just
	 * for storing lengths) in order to fully solve this corner case.
	 *
	 * TODO: a reasonable compromise might be to add a callback routine for
	 * closing file descriptors in any already-written control data.  This
	 * would solve the worst aspect of the data loss, not the loss itself.
	 */
	if (reply >= 0 && addr_len > 0 && GRANT_VALID(call->_sc_grant)) {
		if (addr == NULL)
			panic("libsockdriver: success but no address given");

		if (addr_len > call->_sc_len)
			addr_len = call->_sc_len; /* truncate addr and len */

		if (addr_len > 0 && (r = sys_safecopyto(call->sc_endpt,
		    call->_sc_grant, 0, (vir_bytes)addr, addr_len)) != OK)
			reply = r;
	} else
		addr_len = 0;

	memset(&m, 0, sizeof(m));
	m.m_lsockdriver_vfs_recv_reply.req_id = call->sc_req;
	m.m_lsockdriver_vfs_recv_reply.status = reply;
	m.m_lsockdriver_vfs_recv_reply.ctl_len = ctl_len;
	m.m_lsockdriver_vfs_recv_reply.addr_len = addr_len;
	m.m_lsockdriver_vfs_recv_reply.flags = flags;

	send_reply(call->sc_endpt, SDEV_RECV_REPLY, &m);
}

/*
 * Send a reply to a select request.
 */
static void
send_select_reply(const struct sockdriver_select * sel, int type, sockid_t id,
	int ops)
{
	message m;

	assert(ops != SUSPEND && ops != EDONTREPLY);

	memset(&m, 0, sizeof(m));
	m.m_lsockdriver_vfs_select_reply.sock_id = id;
	m.m_lsockdriver_vfs_select_reply.status = ops;

	send_reply(sel->ss_endpt, type, &m);
}

/*
 * Send a reply to an earlier select call that requested notifications.
 */
void
sockdriver_reply_select(const struct sockdriver_select * sel, sockid_t id,
	int ops)
{

	send_select_reply(sel, SDEV_SELECT2_REPLY, id, ops);
}

/*
 * Create a new socket.  This call may not be suspended.
 */
static void
do_socket(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	sockid_t r;

	if (sdp->sdr_socket != NULL)
		r = sdp->sdr_socket(m_ptr->m_vfs_lsockdriver_socket.domain,
		    m_ptr->m_vfs_lsockdriver_socket.type,
		    m_ptr->m_vfs_lsockdriver_socket.protocol,
		    m_ptr->m_vfs_lsockdriver_socket.user_endpt);
	else
		r = EOPNOTSUPP;

	send_socket_reply(m_ptr->m_source,
	    m_ptr->m_vfs_lsockdriver_socket.req_id, r, -1);
}

/*
 * Create a pair of connected sockets.  Relevant for UNIX domain sockets only.
 * This call may not be suspended.
 */
static void
do_socketpair(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	sockid_t sockid[2];
	int r;

	if (sdp->sdr_socketpair != NULL)
		r = sdp->sdr_socketpair(m_ptr->m_vfs_lsockdriver_socket.domain,
		    m_ptr->m_vfs_lsockdriver_socket.type,
		    m_ptr->m_vfs_lsockdriver_socket.protocol,
		    m_ptr->m_vfs_lsockdriver_socket.user_endpt, sockid);
	else
		r = EOPNOTSUPP;

	if (r != OK) {
		sockid[0] = r;
		sockid[1] = -1;
	}

	send_socket_reply(m_ptr->m_source,
	    m_ptr->m_vfs_lsockdriver_socket.req_id, sockid[0], sockid[1]);
}

/*
 * Bind a socket to a local address, or connect a socket to a remote address.
 * In both cases, this call may be suspended by the socket driver, in which
 * case sockdriver_reply_generic() must be used to reply later.
 *
 * For bind(2), POSIX is not entirely consistent regarding call suspension: the
 * bind(2) call may return EINPROGRESS for nonblocking sockets, but this also
 * suggests that blocking bind(2) calls may be interrupted by signals (as on
 * MINIX3 they can be), yet EINTR is not defined as a valid return code for it.
 */
static void
do_bind_connect(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	int (*proc)(sockid_t, const struct sockaddr * __restrict, socklen_t,
	    endpoint_t, const struct sockdriver_call * __restrict);
	struct sockdriver_call call;
	char buf[SOCKADDR_MAX];
	sockid_t id;
	cp_grant_id_t grant;
	socklen_t len;
	endpoint_t user_endpt;
	int r, sflags;

	call.sc_endpt = m_ptr->m_source;
	call.sc_req = m_ptr->m_vfs_lsockdriver_addr.req_id;

	id = m_ptr->m_vfs_lsockdriver_addr.sock_id;
	grant = m_ptr->m_vfs_lsockdriver_addr.grant;
	len = m_ptr->m_vfs_lsockdriver_addr.len;
	user_endpt = m_ptr->m_vfs_lsockdriver_addr.user_endpt;
	sflags = m_ptr->m_vfs_lsockdriver_addr.sflags;

	switch (m_ptr->m_type) {
	case SDEV_BIND:		proc = sdp->sdr_bind;		break;
	case SDEV_CONNECT:	proc = sdp->sdr_connect;	break;
	default:		panic("expected bind or connect");
	}

	r = OK;
	if (!GRANT_VALID(grant) || len == 0 || len > sizeof(buf))
		r = EINVAL;
	else
		r = sys_safecopyfrom(m_ptr->m_source, grant, 0, (vir_bytes)buf,
		    len);

	if (r == OK) {
		if (proc != NULL)
			r = proc(id, (struct sockaddr *)buf, len, user_endpt,
			    (sflags & SDEV_NONBLOCK) ? NULL : &call);
		else
			r = EOPNOTSUPP;
	}

	assert(!(sflags & SDEV_NONBLOCK) || (r != SUSPEND && r != EDONTREPLY));

	if (r != SUSPEND && r != EDONTREPLY)
		sockdriver_reply_generic(&call, r);
}

/*
 * Put a socket in listening mode.  This call may not be suspended.
 */
static void
do_listen(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	int r;

	if (sdp->sdr_listen != NULL)
		r = sdp->sdr_listen(m_ptr->m_vfs_lsockdriver_simple.sock_id,
		    m_ptr->m_vfs_lsockdriver_simple.param /*backlog*/);
	else
		r = EOPNOTSUPP;

	send_generic_reply(m_ptr->m_source,
	    m_ptr->m_vfs_lsockdriver_simple.req_id, r);
}

/*
 * Accept a connection on a listening socket, creating a new socket.
 * This call may be suspended by the socket driver, in which case
 * sockdriver_reply_accept() must be used to reply later.
 */
static void
do_accept(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	struct sockdriver_call call;
	char buf[SOCKADDR_MAX];
	struct sockaddr *addr;
	socklen_t len;
	endpoint_t user_endpt;
	int sflags;
	sockid_t r;

	call.sc_endpt = m_ptr->m_source;
	call.sc_req = m_ptr->m_vfs_lsockdriver_addr.req_id;
	call._sc_grant = m_ptr->m_vfs_lsockdriver_addr.grant;
	call._sc_len = m_ptr->m_vfs_lsockdriver_addr.len;

	addr = (struct sockaddr *)buf;
	len = 0;
	user_endpt = m_ptr->m_vfs_lsockdriver_addr.user_endpt;
	sflags = m_ptr->m_vfs_lsockdriver_addr.sflags;

	if (sdp->sdr_accept != NULL)
		r = sdp->sdr_accept(m_ptr->m_vfs_lsockdriver_addr.sock_id,
		    addr, &len, user_endpt,
		    (sflags & SDEV_NONBLOCK) ? NULL : &call);
	else
		r = EOPNOTSUPP;

	assert(!(sflags & SDEV_NONBLOCK) || (r != SUSPEND && r != EDONTREPLY));

	if (r != SUSPEND && r != EDONTREPLY)
		sockdriver_reply_accept(&call, r, addr, len);
}

/*
 * Send regular and/or control data.  This call may be suspended by the socket
 * driver, in which case sockdriver_reply_generic() must be used to reply
 * later.
 */
static void
do_send(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	struct sockdriver_call call;
	struct sockdriver_data data, ctl_data;
	char buf[SOCKADDR_MAX];
	struct sockaddr *addr;
	cp_grant_id_t addr_grant;
	socklen_t addr_len;
	endpoint_t user_endpt;
	sockid_t id;
	int r, flags;

	call.sc_endpt = m_ptr->m_source;
	call.sc_req = m_ptr->m_vfs_lsockdriver_sendrecv.req_id;

	data._sd_grant = m_ptr->m_vfs_lsockdriver_sendrecv.data_grant;
	data._sd_endpt = m_ptr->m_source;
	data._sd_len = m_ptr->m_vfs_lsockdriver_sendrecv.data_len;

	/* The returned size must fit in an 'int'; truncate accordingly. */
	if (data._sd_len > INT_MAX)
		data._sd_len = INT_MAX;

	ctl_data._sd_endpt = m_ptr->m_source;
	ctl_data._sd_grant = m_ptr->m_vfs_lsockdriver_sendrecv.ctl_grant;
	ctl_data._sd_len = m_ptr->m_vfs_lsockdriver_sendrecv.ctl_len;

	id = m_ptr->m_vfs_lsockdriver_sendrecv.sock_id;
	addr_grant = m_ptr->m_vfs_lsockdriver_sendrecv.addr_grant;
	addr_len = m_ptr->m_vfs_lsockdriver_sendrecv.addr_len;
	user_endpt = m_ptr->m_vfs_lsockdriver_sendrecv.user_endpt;
	flags = m_ptr->m_vfs_lsockdriver_sendrecv.flags;

	r = OK;
	if (GRANT_VALID(addr_grant)) {
		if (addr_len == 0 || addr_len > sizeof(buf))
			r = EINVAL;
		else
			r = sys_safecopyfrom(m_ptr->m_source, addr_grant, 0,
			    (vir_bytes)buf, addr_len);
		addr = (struct sockaddr *)buf;
	} else {
		addr = NULL;
		addr_len = 0;
	}

	if (r == OK) {
		if (sdp->sdr_send != NULL)
			r = sdp->sdr_send(id, &data, data._sd_len, &ctl_data,
			    ctl_data._sd_len, addr, addr_len, user_endpt,
			    flags, (flags & MSG_DONTWAIT) ? NULL : &call);
		else
			r = EOPNOTSUPP;
	}

	assert(!(flags & MSG_DONTWAIT) || (r != SUSPEND && r != EDONTREPLY));

	if (r != SUSPEND && r != EDONTREPLY)
		sockdriver_reply_generic(&call, r);
}

/*
 * Receive regular and/or control data.  This call may be suspended by the
 * socket driver, in which case sockdriver_reply_recv() must be used to reply
 * later.
 */
static void
do_recv(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	struct sockdriver_call call;
	struct sockdriver_data data, ctl_data;
	char buf[SOCKADDR_MAX];
	struct sockaddr *addr;
	sockid_t id;
	socklen_t ctl_len, addr_len;
	endpoint_t user_endpt;
	int r, flags;

	call.sc_endpt = m_ptr->m_source;
	call.sc_req = m_ptr->m_vfs_lsockdriver_sendrecv.req_id;
	call._sc_grant = m_ptr->m_vfs_lsockdriver_sendrecv.addr_grant;
	call._sc_len = m_ptr->m_vfs_lsockdriver_sendrecv.addr_len;

	data._sd_endpt = m_ptr->m_source;
	data._sd_grant = m_ptr->m_vfs_lsockdriver_sendrecv.data_grant;
	data._sd_len = m_ptr->m_vfs_lsockdriver_sendrecv.data_len;

	/* The returned size must fit in an 'int'; truncate accordingly. */
	if (data._sd_len > INT_MAX)
		data._sd_len = INT_MAX;

	ctl_data._sd_endpt = m_ptr->m_source;
	ctl_data._sd_grant = m_ptr->m_vfs_lsockdriver_sendrecv.ctl_grant;
	ctl_data._sd_len = m_ptr->m_vfs_lsockdriver_sendrecv.ctl_len;

	id = m_ptr->m_vfs_lsockdriver_sendrecv.sock_id;
	ctl_len = ctl_data._sd_len;
	addr = (struct sockaddr *)buf;
	addr_len = 0; /* the default: no source address */
	user_endpt = m_ptr->m_vfs_lsockdriver_sendrecv.user_endpt;
	flags = m_ptr->m_vfs_lsockdriver_sendrecv.flags;

	if (sdp->sdr_recv != NULL)
		r = sdp->sdr_recv(id, &data, data._sd_len, &ctl_data, &ctl_len,
		    addr, &addr_len, user_endpt, &flags,
		    (flags & MSG_DONTWAIT) ? NULL : &call);
	else
		r = EOPNOTSUPP;

	assert(!(flags & MSG_DONTWAIT) || (r != SUSPEND && r != EDONTREPLY));

	if (r != SUSPEND && r != EDONTREPLY)
		sockdriver_reply_recv(&call, r, ctl_len, addr, addr_len,
		    flags);
}

/*
 * Process an I/O control call.  This call may be suspended by the socket
 * driver, in which case sockdriver_reply_generic() must be used to reply
 * later.
 */
static void
do_ioctl(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	struct sockdriver_call call;
	struct sockdriver_data data;
	sockid_t id;
	unsigned long request;
	endpoint_t user_endpt;
	int r, sflags;

	call.sc_endpt = m_ptr->m_source;
	call.sc_req = m_ptr->m_vfs_lsockdriver_ioctl.req_id;

	id = m_ptr->m_vfs_lsockdriver_ioctl.sock_id;
	request = m_ptr->m_vfs_lsockdriver_ioctl.request;
	user_endpt = m_ptr->m_vfs_lsockdriver_ioctl.user_endpt;
	sflags = m_ptr->m_vfs_lsockdriver_ioctl.sflags;

	data._sd_endpt = m_ptr->m_source;
	data._sd_grant = m_ptr->m_vfs_lsockdriver_ioctl.grant;
	if (_MINIX_IOCTL_BIG(request))
		data._sd_len = _MINIX_IOCTL_SIZE_BIG(request);
	else
		data._sd_len = _MINIX_IOCTL_SIZE(request);

	if (sdp->sdr_ioctl != NULL)
		r = sdp->sdr_ioctl(id, request, &data, user_endpt,
		    (sflags & SDEV_NONBLOCK) ? NULL : &call);
	else
		r = EOPNOTSUPP;

	assert(!(sflags & SDEV_NONBLOCK) || (r != SUSPEND && r != EDONTREPLY));

	if (r != SUSPEND && r != EDONTREPLY)
		sockdriver_reply_generic(&call, r);
}

/*
 * Set socket options.  This call may not be suspended.
 */
static void
do_setsockopt(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	struct sockdriver_data data;
	int r;

	data._sd_endpt = m_ptr->m_source;
	data._sd_grant = m_ptr->m_vfs_lsockdriver_getset.grant;
	data._sd_len = m_ptr->m_vfs_lsockdriver_getset.len;

	if (sdp->sdr_setsockopt != NULL)
		r = sdp->sdr_setsockopt(
		    m_ptr->m_vfs_lsockdriver_getset.sock_id,
		    m_ptr->m_vfs_lsockdriver_getset.level,
		    m_ptr->m_vfs_lsockdriver_getset.name, &data, data._sd_len);
	else
		r = EOPNOTSUPP;

	send_generic_reply(m_ptr->m_source,
	    m_ptr->m_vfs_lsockdriver_getset.req_id, r);
}

/*
 * Retrieve socket options.  This call may not be suspended.
 */
static void
do_getsockopt(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	struct sockdriver_data data;
	socklen_t len;
	int r;

	data._sd_endpt = m_ptr->m_source;
	data._sd_grant = m_ptr->m_vfs_lsockdriver_getset.grant;
	data._sd_len = m_ptr->m_vfs_lsockdriver_getset.len;

	len = data._sd_len;

	if (sdp->sdr_setsockopt != NULL)
		r = sdp->sdr_getsockopt(
		    m_ptr->m_vfs_lsockdriver_getset.sock_id,
		    m_ptr->m_vfs_lsockdriver_getset.level,
		    m_ptr->m_vfs_lsockdriver_getset.name, &data, &len);
	else
		r = EOPNOTSUPP;

	/*
	 * For these requests, the main reply code is used to return the
	 * resulting data length on success.  The length will never large
	 * enough to overflow, and we save on API calls and messages this way.
	 */
	if (r == OK) {
		assert(len <= INT_MAX);

		r = (int)len;
	} else if (r > 0)
		panic("libsockdriver: invalid reply");

	send_generic_reply(m_ptr->m_source,
	    m_ptr->m_vfs_lsockdriver_getset.req_id, r);
}

/*
 * Get local or remote address.  This call may not be suspended.
 */
static void
do_getname(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	int (*proc)(sockid_t, struct sockaddr * __restrict,
	    socklen_t * __restrict);
	char buf[SOCKADDR_MAX];
	socklen_t addr_len, len;
	int r;

	switch (m_ptr->m_type) {
	case SDEV_GETSOCKNAME:	proc = sdp->sdr_getsockname;	break;
	case SDEV_GETPEERNAME:	proc = sdp->sdr_getpeername;	break;
	default:		panic("expected getsockname or getpeername");
	}

	/* The 'name' and 'level' message fields are unused for these calls. */

	addr_len = m_ptr->m_vfs_lsockdriver_getset.len;
	len = 0;

	if (proc != NULL)
		r = proc(m_ptr->m_vfs_lsockdriver_getset.sock_id,
		    (struct sockaddr *)buf, &len);
	else
		r = EOPNOTSUPP;

	if (r == OK) {
		if (len == 0)
			panic("libsockdriver: success but no address given");

		if (addr_len > len)
			addr_len = len;

		/* As above, use the reply code for the resulting length. */
		if (addr_len > 0 && (r = sys_safecopyto(m_ptr->m_source,
		    m_ptr->m_vfs_lsockdriver_getset.grant, 0, (vir_bytes)buf,
		    addr_len)) == OK) {
			assert(addr_len <= INT_MAX);

			/*
			 * The Open Group wording has changed recently, now
			 * suggesting that when truncating the "stored address"
			 * the resulting length should be truncated as well.
			 */
			r = addr_len;
		}
	} else if (r > 0)
		panic("libsockdriver: invalid reply");

	send_generic_reply(m_ptr->m_source,
	    m_ptr->m_vfs_lsockdriver_getset.req_id, r);
}

/*
 * Shut down socket send and receive operations.  This call may not be
 * suspended.
 */
static void
do_shutdown(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	int r;

	if (sdp->sdr_shutdown != NULL)
		r = sdp->sdr_shutdown(
		    m_ptr->m_vfs_lsockdriver_simple.sock_id,
		    m_ptr->m_vfs_lsockdriver_simple.param /*how*/);
	else
		r = EOPNOTSUPP;

	send_generic_reply(m_ptr->m_source,
	    m_ptr->m_vfs_lsockdriver_simple.req_id, r);
}

/*
 * Close a socket.  This call may be suspended by the socket driver, in which
 * case sockdriver_reply_generic() must be used to reply later.  Note that VFS
 * currently does not support blocking close operations, and will mark all
 * close operations as nonblocking.  This will be changed in the future.
 */
static void
do_close(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	struct sockdriver_call call;
	int r, sflags;

	call.sc_endpt = m_ptr->m_source;
	call.sc_req = m_ptr->m_vfs_lsockdriver_simple.req_id;

	sflags = m_ptr->m_vfs_lsockdriver_simple.param;

	if (sdp->sdr_close != NULL)
		r = sdp->sdr_close(m_ptr->m_vfs_lsockdriver_simple.sock_id,
		    (sflags & SDEV_NONBLOCK) ? NULL : &call);
	else
		r = OK; /* exception: this must never fail */

	assert(!(sflags & SDEV_NONBLOCK) || (r != SUSPEND && r != EDONTREPLY));

	if (r != SUSPEND && r != EDONTREPLY)
		sockdriver_reply_generic(&call, r);
}

/*
 * Cancel a previous operation which may currently be suspended.  The cancel
 * operation itself does not have a reply.  Instead, if the provided operation
 * was found to be currently suspended, that operation must be aborted and a
 * reply (typically EINTR) must be sent for it.  If no matching operation was
 * found, no reply must be sent at all.
 */
static void
do_cancel(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	struct sockdriver_call call;

	call.sc_endpt = m_ptr->m_source;
	call.sc_req = m_ptr->m_vfs_lsockdriver_simple.req_id;

	/* The 'param' message field is unused by this request. */

	if (sdp->sdr_cancel != NULL)
		sdp->sdr_cancel(m_ptr->m_vfs_lsockdriver_simple.sock_id,
		    &call);
}

/*
 * Process a select request.  Select requests have their own rules with respect
 * to suspension and later notification.  The basic idea is: an immediate reply
 * is always sent with the subset of requested operations that are ready.  If
 * SDEV_NOTIFY is given, the remaining operations are to be combined with any
 * previous operations requested (with SDEV_NOTIFY) by the calling endpoint.
 * If any of the pending previous operations become ready, a late reply is sent
 * and only those ready operations are forgotten, leaving any other non-ready
 * operations for other late replies.
 */
static void
do_select(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr)
{
	struct sockdriver_select sel;
	sockid_t id;
	int r, ops;

	sel.ss_endpt = m_ptr->m_source;
	id = m_ptr->m_vfs_lsockdriver_select.sock_id;
	ops = m_ptr->m_vfs_lsockdriver_select.ops;

	if (sdp->sdr_select != NULL)
		r = sdp->sdr_select(id, ops,
		    (ops & SDEV_NOTIFY) ? &sel : NULL);
	else
		r = EOPNOTSUPP;

	send_select_reply(&sel, SDEV_SELECT1_REPLY, id, r);
}

/*
 * Return TRUE if the given endpoint may initiate socket requests.
 */
static int
may_request(endpoint_t endpt)
{

	/*
	 * For now, we allow only VFS to initiate socket calls.  In the future,
	 * we may allow networked file systems to call into the network stack
	 * directly.  The sockdriver API has already been designed to allow for
	 * that, but this check will then need to change.  Ideally it would be
	 * using some sort of ACL system.  For now, this check prevents that
	 * network drivers themselves create and use sockets.
	 */
	return (endpt == VFS_PROC_NR);
}

/*
 * Process an incoming message, and (typically) send a reply.
 */
void
sockdriver_process(const struct sockdriver * __restrict sdp,
	const message * __restrict m_ptr, int ipc_status)
{

	/* Handle notifications separately. */
	if (is_ipc_notify(ipc_status)) {
		switch (m_ptr->m_source) {
		case CLOCK:
			if (sdp->sdr_alarm != NULL)
				sdp->sdr_alarm(m_ptr->m_notify.timestamp);
			break;
		default:
			if (sdp->sdr_other != NULL)
				sdp->sdr_other(m_ptr, ipc_status);
		}

		return; /* do not send a reply */
	}

	/* Is this a socket request from an acceptable party? */
	if (!IS_SDEV_RQ(m_ptr->m_type) || !may_request(m_ptr->m_source)) {
		if (sdp->sdr_other != NULL)
			sdp->sdr_other(m_ptr, ipc_status);

		return;	/* do not send a reply */
	}

	/*
	 * Process the request.  If the request is not recognized, we cannot
	 * send a reply either, because we do not know the reply message
	 * format.  Passing the request message to the sdr_other hook serves no
	 * practical purpose either: if the request is legitimate, this library
	 * should know about it.
	 */
	switch (m_ptr->m_type) {
	case SDEV_SOCKET:	do_socket(sdp, m_ptr);		break;
	case SDEV_SOCKETPAIR:	do_socketpair(sdp, m_ptr);	break;
	case SDEV_BIND:		do_bind_connect(sdp, m_ptr);	break;
	case SDEV_CONNECT:	do_bind_connect(sdp, m_ptr);	break;
	case SDEV_LISTEN:	do_listen(sdp, m_ptr);		break;
	case SDEV_ACCEPT:	do_accept(sdp, m_ptr);		break;
	case SDEV_SEND:		do_send(sdp, m_ptr);		break;
	case SDEV_RECV:		do_recv(sdp, m_ptr);		break;
	case SDEV_IOCTL:	do_ioctl(sdp, m_ptr);		break;
	case SDEV_SETSOCKOPT:	do_setsockopt(sdp, m_ptr);	break;
	case SDEV_GETSOCKOPT:	do_getsockopt(sdp, m_ptr);	break;
	case SDEV_GETSOCKNAME:	do_getname(sdp, m_ptr);		break;
	case SDEV_GETPEERNAME:	do_getname(sdp, m_ptr);		break;
	case SDEV_SHUTDOWN:	do_shutdown(sdp, m_ptr);	break;
	case SDEV_CLOSE:	do_close(sdp, m_ptr);		break;
	case SDEV_CANCEL:	do_cancel(sdp, m_ptr);		break;
	case SDEV_SELECT:	do_select(sdp, m_ptr);		break;
	}
}

/*
 * Break out of the main loop after finishing the current request.
 */
void
sockdriver_terminate(void)
{

	running = FALSE;

	sef_cancel();
}

/*
 * Main program of any socket driver.
 */
void
sockdriver_task(const struct sockdriver * sdp)
{
	message m;
	int r, ipc_status;

	/* The main message loop. */
	running = TRUE;

	while (running) {
		if ((r = sef_receive_status(ANY, &m, &ipc_status)) != OK) {
			if (r == EINTR)
				continue;	/* sef_cancel() was called */

			panic("sockdriver: sef_receive_status failed: %d", r);
		}

		sockdriver_process(sdp, &m, ipc_status);
	}
}
