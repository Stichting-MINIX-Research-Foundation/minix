/*
 * This file implements the lower socket layer of VFS: communication with
 * socket drivers.  Socket driver communication evolved out of character driver
 * communication, and the two have many similarities between them.  Most
 * importantly, socket driver communication also has the distinction between
 * short-lived and long-lived requests.
 *
 * Short-lived requests are expected to be replied to by the socket driver
 * immediately in all cases.  For such requests, VFS keeps the worker thread
 * for the calling process alive until the reply arrives.  In contrast,
 * long-lived requests may block.  For such requests, VFS suspends the calling
 * process until a reply comes in, or until a signal interrupts the request.
 * Both short-lived and long-lived requests may be aborted if VFS finds that
 * the corresponding socket driver has died.  Even though long-lived requests
 * may be marked as nonblocking, nonblocking calls are still handled as
 * long-lived in terms of VFS processing.
 *
 * For an overview of the socket driver requests and replies, message layouts,
 * and which requests are long-lived or short-lived (i.e. may suspend or not),
 * please refer to the corresponding table in the libsockdriver source code.
 *
 * For most long-lived socket requests, the main VFS thread processes the reply
 * from the socket driver.  This typically consists of waking up the user
 * process that originally issued the system call on the socket by simply
 * relaying the call's result code.  Some socket calls require a specific reply
 * message and/or additional post-call actions; for those, resume_*() calls are
 * made back into the upper socket layer.
 *
 * If a process is interrupted by a signal, any ongoing long-lived socket
 * request must be canceled.  This is done by sending a one-way cancel request
 * to the socket driver, and waiting for it to reply to the original request.
 * In this case, the reply will be processed from the worker thread that is
 * handling the cancel operation.  Canceling does not imply call failure: the
 * cancellation may result in a partial I/O reply, and a successful reply may
 * cross the cancel request.
 *
 * One main exception is the reply to an accept request.  Once a connection has
 * been accepted, a new socket has to be created for it.  This requires actions
 * that require the ability to block the current thread, and so, a worker
 * thread is spawned for processing successful accept replies, unless the reply
 * was received from a worker thread already (as may be the case if the accept
 * request was being canceled).
 */

#include "fs.h"
#include <sys/socket.h>
#include <minix/callnr.h>

/*
 * Send a short-lived request message to the given socket driver, and suspend
 * the current worker thread until a reply message has been received.  On
 * success, the function will return OK, and the reply message will be stored
 * in the message structure pointed to by 'm_ptr'.  The function may fail if
 * the socket driver dies before sending a reply.  In that case, the function
 * will return a negative error code, and also store the same negative error
 * code in the m_type field of the 'm_ptr' message structure.
 */
static int
sdev_sendrec(struct smap * sp, message * m_ptr)
{
	int r;

	/* Send the request to the driver. */
	if ((r = asynsend3(sp->smap_endpt, m_ptr, AMF_NOREPLY)) != OK)
		panic("VFS: asynsend in sdev_sendrec failed: %d", r);

	/* Suspend this thread until we have received the response. */
	self->w_task = sp->smap_endpt;
	self->w_drv_sendrec = m_ptr;

	worker_wait();

	self->w_task = NONE;
	assert(self->w_drv_sendrec == NULL);

	return (!IS_SDEV_RS(m_ptr->m_type)) ? m_ptr->m_type : OK;
}

/*
 * Suspend the current process for later completion of its system call.
 */
int
sdev_suspend(dev_t dev, cp_grant_id_t grant0, cp_grant_id_t grant1,
	cp_grant_id_t grant2, int fd, vir_bytes buf)
{

	fp->fp_sdev.dev = dev;
	fp->fp_sdev.callnr = job_call_nr;
	fp->fp_sdev.grant[0] = grant0;
	fp->fp_sdev.grant[1] = grant1;
	fp->fp_sdev.grant[2] = grant2;

	if (job_call_nr == VFS_ACCEPT) {
		assert(fd != -1);
		assert(buf == 0);
		fp->fp_sdev.aux.fd = fd;
	} else if (job_call_nr == VFS_RECVMSG) {
		assert(fd == -1);
		/*
		 * TODO: we are not yet consistent enough in dealing with
		 * mapped NULL pages to have an assert(buf != 0) here..
		 */
		fp->fp_sdev.aux.buf = buf;
	} else {
		assert(fd == -1);
		assert(buf == 0);
	}

	suspend(FP_BLOCKED_ON_SDEV);
	return SUSPEND;
}

/*
 * Create a socket or socket pair.  Return OK on success, with the new socket
 * device identifier(s) stored in the 'dev' array.  Return an error code upon
 * failure.
 */
int
sdev_socket(int domain, int type, int protocol, dev_t * dev, int pair)
{
	struct smap *sp;
	message m;
	sockid_t sock_id, sock_id2;
	int r;

	/* We could return EAFNOSUPPORT, but the caller should have checked. */
	if ((sp = get_smap_by_domain(domain)) == NULL)
		panic("VFS: sdev_socket for unknown domain");

	/* Prepare the request message. */
	memset(&m, 0, sizeof(m));
	m.m_type = pair ? SDEV_SOCKETPAIR : SDEV_SOCKET;
	m.m_vfs_lsockdriver_socket.req_id = (sockid_t)who_e;
	m.m_vfs_lsockdriver_socket.domain = domain;
	m.m_vfs_lsockdriver_socket.type = type;
	m.m_vfs_lsockdriver_socket.protocol = protocol;
	m.m_vfs_lsockdriver_socket.user_endpt = who_e;

	/* Send the request, and wait for the reply. */
	if ((r = sdev_sendrec(sp, &m)) != OK)
		return r;	/* socket driver died */

	/* Parse the reply message, and check for protocol errors. */
	if (m.m_type != SDEV_SOCKET_REPLY) {
		printf("VFS: %d sent bad reply type %d for call %d\n",
		    sp->smap_endpt, m.m_type, job_call_nr);
		return EIO;
	}

	sock_id = m.m_lsockdriver_vfs_socket_reply.sock_id;
	sock_id2 = m.m_lsockdriver_vfs_socket_reply.sock_id2;

	/* Check for regular errors.  Upon success, return the socket(s). */
	if (sock_id < 0)
		return sock_id;

	dev[0] = make_smap_dev(sp, sock_id);

	if (pair) {
		/* Okay, one more protocol error. */
		if (sock_id2 < 0) {
			printf("VFS: %d sent bad SOCKETPAIR socket ID %d\n",
			    sp->smap_endpt, sock_id2);
			(void)sdev_close(dev[0], FALSE /*may_suspend*/);
			return EIO;
		}

		dev[1] = make_smap_dev(sp, sock_id2);
	}

	return OK;
}

/*
 * Bind or connect a socket to a particular address.  These calls may block, so
 * suspend the current process instead of making the thread wait for the reply.
 */
static int
sdev_bindconn(dev_t dev, int type, vir_bytes addr, unsigned int addr_len,
	int filp_flags)
{
	struct smap *sp;
	sockid_t sock_id;
	cp_grant_id_t grant;
	message m;
	int r;

	if ((sp = get_smap_by_dev(dev, &sock_id)) == NULL)
		return EIO;

	/* Allocate resources. */
	grant = cpf_grant_magic(sp->smap_endpt, who_e, addr, addr_len,
	    CPF_READ);
	if (!GRANT_VALID(grant))
		panic("VFS: cpf_grant_magic failed");

	/* Prepare the request message. */
	memset(&m, 0, sizeof(m));
	m.m_type = type;
	m.m_vfs_lsockdriver_addr.req_id = (sockid_t)who_e;
	m.m_vfs_lsockdriver_addr.sock_id = sock_id;
	m.m_vfs_lsockdriver_addr.grant = grant;
	m.m_vfs_lsockdriver_addr.len = addr_len;
	m.m_vfs_lsockdriver_addr.user_endpt = who_e;
	m.m_vfs_lsockdriver_addr.sflags =
	    (filp_flags & O_NONBLOCK) ? SDEV_NONBLOCK : 0;

	/* Send the request to the driver. */
	if ((r = asynsend3(sp->smap_endpt, &m, AMF_NOREPLY)) != OK)
		panic("VFS: asynsend in sdev_bindconn failed: %d", r);

	/* Suspend the process until the reply arrives. */
	return sdev_suspend(dev, grant, GRANT_INVALID, GRANT_INVALID, -1, 0);
}

/*
 * Bind a socket to a local address.
 */
int
sdev_bind(dev_t dev, vir_bytes addr, unsigned int addr_len, int filp_flags)
{

	return sdev_bindconn(dev, SDEV_BIND, addr, addr_len, filp_flags);
}

/*
 * Connect a socket to a remote address.
 */
int
sdev_connect(dev_t dev, vir_bytes addr, unsigned int addr_len, int filp_flags)
{

	return sdev_bindconn(dev, SDEV_CONNECT, addr, addr_len, filp_flags);
}

/*
 * Send and receive a "simple" request: listen, shutdown, or close.  Note that
 * while cancel requests use the same request format, they require a different
 * way of handling their replies.
 */
static int
sdev_simple(dev_t dev, int type, int param)
{
	struct smap *sp;
	sockid_t sock_id;
	message m;
	int r;

	assert(type == SDEV_LISTEN || type == SDEV_SHUTDOWN ||
	    type == SDEV_CLOSE);

	if ((sp = get_smap_by_dev(dev, &sock_id)) == NULL)
		return EIO;

	/* Prepare the request message. */
	memset(&m, 0, sizeof(m));
	m.m_type = type;
	m.m_vfs_lsockdriver_simple.req_id = (sockid_t)who_e;
	m.m_vfs_lsockdriver_simple.sock_id = sock_id;
	m.m_vfs_lsockdriver_simple.param = param;

	/* Send the request, and wait for the reply. */
	if ((r = sdev_sendrec(sp, &m)) != OK)
		return r;	/* socket driver died */

	/* Parse and return the reply. */
	if (m.m_type != SDEV_REPLY) {
		printf("VFS: %d sent bad reply type %d for call %d\n",
		    sp->smap_endpt, m.m_type, job_call_nr);
		return EIO;
	}

	return m.m_lsockdriver_vfs_reply.status;
}

/*
 * Put a socket in listening mode.
 */
int
sdev_listen(dev_t dev, int backlog)
{

	assert(backlog >= 0);

	return sdev_simple(dev, SDEV_LISTEN, backlog);
}

/*
 * Accept a new connection on a socket.
 */
int
sdev_accept(dev_t dev, vir_bytes addr, unsigned int addr_len, int filp_flags,
	int listen_fd)
{
	struct smap *sp;
	sockid_t sock_id;
	cp_grant_id_t grant;
	message m;
	int r;

	if ((sp = get_smap_by_dev(dev, &sock_id)) == NULL)
		return EIO;

	/* Allocate resources. */
	if (addr != 0) {
		grant = cpf_grant_magic(sp->smap_endpt, who_e, addr, addr_len,
		    CPF_WRITE);
		if (!GRANT_VALID(grant))
			panic("VFS: cpf_grant_magic failed");
	} else
		grant = GRANT_INVALID;

	/* Prepare the request message. */
	memset(&m, 0, sizeof(m));
	m.m_type = SDEV_ACCEPT;
	m.m_vfs_lsockdriver_addr.req_id = (sockid_t)who_e;
	m.m_vfs_lsockdriver_addr.sock_id = sock_id;
	m.m_vfs_lsockdriver_addr.grant = grant;
	m.m_vfs_lsockdriver_addr.len = addr_len;
	m.m_vfs_lsockdriver_addr.user_endpt = who_e;
	m.m_vfs_lsockdriver_addr.sflags =
	    (filp_flags & O_NONBLOCK) ? SDEV_NONBLOCK : 0;

	/* Send the request to the driver. */
	if ((r = asynsend3(sp->smap_endpt, &m, AMF_NOREPLY)) != OK)
		panic("VFS: asynsend in sdev_accept failed: %d", r);

	/* Suspend the process until the reply arrives. */
	return sdev_suspend(dev, grant, GRANT_INVALID, GRANT_INVALID,
	    listen_fd, 0);
}

/*
 * Send or receive a message on a socket.  All read (read(2), recvfrom(2), and
 * recvmsg(2)) and write (write(2), sendto(2), sendmsg(2)) system calls on
 * sockets pass through this function.  The function is named sdev_readwrite
 * rather than sdev_sendrecv to avoid confusion with sdev_sendrec.
 */
int
sdev_readwrite(dev_t dev, vir_bytes data_buf, size_t data_len,
	vir_bytes ctl_buf, unsigned int ctl_len, vir_bytes addr_buf,
	unsigned int addr_len, int flags, int rw_flag, int filp_flags,
	vir_bytes user_buf)
{
	struct smap *sp;
	sockid_t sock_id;
	cp_grant_id_t data_grant, ctl_grant, addr_grant;
	message m;
	int r, bits;

	if ((sp = get_smap_by_dev(dev, &sock_id)) == NULL)
		return EIO;

	/* Allocate resources. */
	data_grant = GRANT_INVALID;
	ctl_grant = GRANT_INVALID;
	addr_grant = GRANT_INVALID;
	bits = (rw_flag == WRITING) ? CPF_READ : CPF_WRITE;

	/*
	 * Supposedly it is allowed to send or receive zero data bytes, even
	 * though it is a bad idea as the return value will then be zero, which
	 * may also indicate EOF (as per W. Richard Stevens).
	 */
	if (data_buf != 0) {
		data_grant = cpf_grant_magic(sp->smap_endpt, who_e, data_buf,
		    data_len, bits);
		if (!GRANT_VALID(data_grant))
			panic("VFS: cpf_grant_magic failed");
	}

	if (ctl_buf != 0) {
		ctl_grant = cpf_grant_magic(sp->smap_endpt, who_e, ctl_buf,
		    ctl_len, bits);
		if (!GRANT_VALID(ctl_grant))
			panic("VFS: cpf_grant_magic failed");
	}

	if (addr_buf != 0) {
		addr_grant = cpf_grant_magic(sp->smap_endpt, who_e, addr_buf,
		    addr_len, bits);
		if (!GRANT_VALID(addr_grant))
			panic("VFS: cpf_grant_magic failed");
	}

	/* Prepare the request message. */
	memset(&m, 0, sizeof(m));
	m.m_type = (rw_flag == WRITING) ? SDEV_SEND : SDEV_RECV;
	m.m_vfs_lsockdriver_sendrecv.req_id = (sockid_t)who_e;
	m.m_vfs_lsockdriver_sendrecv.sock_id = sock_id;
	m.m_vfs_lsockdriver_sendrecv.data_grant = data_grant;
	m.m_vfs_lsockdriver_sendrecv.data_len = data_len;
	m.m_vfs_lsockdriver_sendrecv.ctl_grant = ctl_grant;
	m.m_vfs_lsockdriver_sendrecv.ctl_len = ctl_len;
	m.m_vfs_lsockdriver_sendrecv.addr_grant = addr_grant;
	m.m_vfs_lsockdriver_sendrecv.addr_len = addr_len;
	m.m_vfs_lsockdriver_sendrecv.user_endpt = who_e;
	m.m_vfs_lsockdriver_sendrecv.flags = flags;
	if (filp_flags & O_NONBLOCK)
		m.m_vfs_lsockdriver_sendrecv.flags |= MSG_DONTWAIT;
	if (rw_flag == WRITING && (filp_flags & O_NOSIGPIPE))
		m.m_vfs_lsockdriver_sendrecv.flags |= MSG_NOSIGNAL;

	/* Send the request to the driver. */
	if ((r = asynsend3(sp->smap_endpt, &m, AMF_NOREPLY)) != OK)
		panic("VFS: asynsend in sdev_readwrite failed: %d", r);

	/* Suspend the process until the reply arrives. */
	return sdev_suspend(dev, data_grant, ctl_grant, addr_grant, -1,
	    user_buf);
}

/*
 * Perform I/O control.
 */
int
sdev_ioctl(dev_t dev, unsigned long request, vir_bytes buf, int filp_flags)
{
	struct smap *sp;
	sockid_t sock_id;
	cp_grant_id_t grant;
	message m;
	int r;

	if ((sp = get_smap_by_dev(dev, &sock_id)) == NULL)
		return EIO;

	/* Allocate resources. */
	grant = make_ioctl_grant(sp->smap_endpt, who_e, buf, request);

	/* Prepare the request message. */
	memset(&m, 0, sizeof(m));
	m.m_type = SDEV_IOCTL;
	m.m_vfs_lsockdriver_ioctl.req_id = (sockid_t)who_e;
	m.m_vfs_lsockdriver_ioctl.sock_id = sock_id;
	m.m_vfs_lsockdriver_ioctl.request = request;
	m.m_vfs_lsockdriver_ioctl.grant = grant;
	m.m_vfs_lsockdriver_ioctl.user_endpt = who_e;
	m.m_vfs_lsockdriver_ioctl.sflags =
	    (filp_flags & O_NONBLOCK) ? SDEV_NONBLOCK : 0;

	/* Send the request to the driver. */
	if ((r = asynsend3(sp->smap_endpt, &m, AMF_NOREPLY)) != OK)
		panic("VFS: asynsend in sdev_ioctl failed: %d", r);

	/* Suspend the process until the reply arrives. */
	return sdev_suspend(dev, grant, GRANT_INVALID, GRANT_INVALID, -1, 0);
}

/*
 * Set socket options.
 */
int
sdev_setsockopt(dev_t dev, int level, int name, vir_bytes addr,
	unsigned int len)
{
	struct smap *sp;
	sockid_t sock_id;
	cp_grant_id_t grant;
	message m;
	int r;

	if ((sp = get_smap_by_dev(dev, &sock_id)) == NULL)
		return EIO;

	/* Allocate resources. */
	grant = cpf_grant_magic(sp->smap_endpt, who_e, addr, len, CPF_READ);
	if (!GRANT_VALID(grant))
		panic("VFS: cpf_grant_magic failed");

	/* Prepare the request message. */
	memset(&m, 0, sizeof(m));
	m.m_type = SDEV_SETSOCKOPT;
	m.m_vfs_lsockdriver_getset.req_id = (sockid_t)who_e;
	m.m_vfs_lsockdriver_getset.sock_id = sock_id;
	m.m_vfs_lsockdriver_getset.level = level;
	m.m_vfs_lsockdriver_getset.name = name;
	m.m_vfs_lsockdriver_getset.grant = grant;
	m.m_vfs_lsockdriver_getset.len = len;

	/* Send the request, and wait for the reply. */
	r = sdev_sendrec(sp, &m);

	/* Free resources. */
	(void)cpf_revoke(grant);

	if (r != OK)
		return r;	/* socket driver died */

	/* Parse and return the reply. */
	if (m.m_type != SDEV_REPLY) {
		printf("VFS: %d sent bad reply type %d for call %d\n",
		    sp->smap_endpt, m.m_type, job_call_nr);
		return EIO;
	}

	return m.m_lsockdriver_vfs_reply.status;
}

/*
 * Send and receive a "get" request: getsockopt, getsockname, or getpeername.
 */
static int
sdev_get(dev_t dev, int type, int level, int name, vir_bytes addr,
	unsigned int * len)
{
	struct smap *sp;
	sockid_t sock_id;
	cp_grant_id_t grant;
	message m;
	int r;

	assert(type == SDEV_GETSOCKOPT || type == SDEV_GETSOCKNAME ||
	    type == SDEV_GETPEERNAME);

	if ((sp = get_smap_by_dev(dev, &sock_id)) == NULL)
		return EIO;

	/* Allocate resources. */
	grant = cpf_grant_magic(sp->smap_endpt, who_e, addr, *len, CPF_WRITE);
	if (!GRANT_VALID(grant))
		panic("VFS: cpf_grant_magic failed");

	/* Prepare the request message. */
	memset(&m, 0, sizeof(m));
	m.m_type = type;
	m.m_vfs_lsockdriver_getset.req_id = (sockid_t)who_e;
	m.m_vfs_lsockdriver_getset.sock_id = sock_id;
	m.m_vfs_lsockdriver_getset.level = level;
	m.m_vfs_lsockdriver_getset.name = name;
	m.m_vfs_lsockdriver_getset.grant = grant;
	m.m_vfs_lsockdriver_getset.len = *len;

	/* Send the request, and wait for the reply. */
	r = sdev_sendrec(sp, &m);

	/* Free resources. */
	(void)cpf_revoke(grant);

	if (r != OK)
		return r;	/* socket driver died */

	/* Parse and return the reply. */
	if (m.m_type != SDEV_REPLY) {
		printf("VFS: %d sent bad reply type %d for call %d\n",
		    sp->smap_endpt, m.m_type, job_call_nr);
		return EIO;
	}

	if ((r = m.m_lsockdriver_vfs_reply.status) < 0)
		return r;

	*len = (unsigned int)r;
	return OK;
}

/*
 * Get socket options.
 */
int
sdev_getsockopt(dev_t dev, int level, int name, vir_bytes addr,
	unsigned int * len)
{

	return sdev_get(dev, SDEV_GETSOCKOPT, level, name, addr, len);
}

/*
 * Get the local address of a socket.
 */
int
sdev_getsockname(dev_t dev, vir_bytes addr, unsigned int * addr_len)
{

	return sdev_get(dev, SDEV_GETSOCKNAME, 0, 0, addr, addr_len);
}

/*
 * Get the remote address of a socket.
 */
int
sdev_getpeername(dev_t dev, vir_bytes addr, unsigned int * addr_len)
{

	return sdev_get(dev, SDEV_GETPEERNAME, 0, 0, addr, addr_len);
}

/*
 * Shut down socket send and receive operations.
 */
int
sdev_shutdown(dev_t dev, int how)
{

	assert(how == SHUT_RD || how == SHUT_WR || how == SHUT_RDWR);

	return sdev_simple(dev, SDEV_SHUTDOWN, how);
}

/*
 * Close the socket identified by the given socket device number.
 */
int
sdev_close(dev_t dev, int may_suspend)
{
	struct smap *sp;
	sockid_t sock_id;
	message m;
	int r;

	/*
	 * Originally, all close requests were blocking the calling thread, but
	 * the new support for SO_LINGER has changed that.  In a very strictly
	 * limited subset of cases - namely, the user process calling close(2),
	 * we suspend the close request and handle it asynchronously.  In all
	 * other cases, including close-on-exit, close-on-exec, and even dup2,
	 * the close is issued as a thread-synchronous request instead.
	 */
	if (may_suspend) {
		if ((sp = get_smap_by_dev(dev, &sock_id)) == NULL)
			return EIO;

		/* Prepare the request message. */
		memset(&m, 0, sizeof(m));
		m.m_type = SDEV_CLOSE;
		m.m_vfs_lsockdriver_simple.req_id = (sockid_t)who_e;
		m.m_vfs_lsockdriver_simple.sock_id = sock_id;
		m.m_vfs_lsockdriver_simple.param = 0;

		/* Send the request to the driver. */
		if ((r = asynsend3(sp->smap_endpt, &m, AMF_NOREPLY)) != OK)
			panic("VFS: asynsend in sdev_bindconn failed: %d", r);

		/* Suspend the process until the reply arrives. */
		return sdev_suspend(dev, GRANT_INVALID, GRANT_INVALID,
		    GRANT_INVALID, -1, 0);
	} else
		/* Block the calling thread until the socket is closed. */
		return sdev_simple(dev, SDEV_CLOSE, SDEV_NONBLOCK);
}

/*
 * Initiate a select call on a socket device.  Return OK iff the request was
 * sent, without suspending the process.
 */
int
sdev_select(dev_t dev, int ops)
{
	struct smap *sp;
	sockid_t sock_id;
	message m;
	int r;

	if ((sp = get_smap_by_dev(dev, &sock_id)) == NULL)
		return EIO;

	/* Prepare the request message. */
	memset(&m, 0, sizeof(m));
	m.m_type = SDEV_SELECT;
	m.m_vfs_lsockdriver_select.sock_id = sock_id;
	m.m_vfs_lsockdriver_select.ops = ops;

	/* Send the request to the driver. */
	if ((r = asynsend3(sp->smap_endpt, &m, AMF_NOREPLY)) != OK)
		panic("VFS: asynsend in sdev_select failed: %d", r);

	return OK;
}

/*
 * A reply has arrived for a previous socket accept request, and the reply
 * indicates that a socket has been accepted.  A status is also returned;
 * usually, this status is OK, but if not, the newly accepted socket must be
 * closed immediately again.  Process the low-level aspects of the reply, and
 * call resume_accept() to let the upper socket layer handle the rest.  This
 * function is always called from a worker thread, and may thus block.
 */
static void
sdev_finish_accept(struct fproc * rfp, message * m_ptr)
{
	struct smap *sp;
	sockid_t sock_id;
	dev_t dev;
	unsigned int len;
	int status;

	assert(rfp->fp_sdev.callnr == VFS_ACCEPT);
	assert(m_ptr->m_type == SDEV_ACCEPT_REPLY);
	assert(m_ptr->m_lsockdriver_vfs_accept_reply.sock_id >= 0);

	/* Free resources.  Accept requests use up to one grant. */
	if (GRANT_VALID(rfp->fp_sdev.grant[0]))
		cpf_revoke(rfp->fp_sdev.grant[0]);
	assert(!GRANT_VALID(rfp->fp_sdev.grant[1]));
	assert(!GRANT_VALID(rfp->fp_sdev.grant[2]));

	sock_id = m_ptr->m_lsockdriver_vfs_accept_reply.sock_id;
	status = m_ptr->m_lsockdriver_vfs_accept_reply.status;
	len = m_ptr->m_lsockdriver_vfs_accept_reply.len;

	/*
	 * We do not want the upper socket layer (socket.c) to deal with smap
	 * and socket ID details, so we construct the new socket device number
	 * here.  We won't use the saved listen FD to determine the smap entry
	 * here, since that involves file pointers and other upper-layer-only
	 * stuff.  So we have to look it up by the source endpoint.  As a
	 * result, we detect some driver deaths here (but not all: see below).
	 */
	if ((sp = get_smap_by_endpt(m_ptr->m_source)) != NULL) {
		/* Leave 'status' as is, regardless of whether it is OK. */
		dev = make_smap_dev(sp, sock_id);
	} else {
		/*
		 * The driver must have died while the thread was blocked on
		 * activation.  Extremely rare, but theoretically possible.
		 * Some driver deaths are indicated only by a driver-up
		 * announcement though; resume_accept() will detect this by
		 * checking that the listening socket has not been invalidated.
		 */
		status = EIO;
		dev = NO_DEV;
	}

	/* Let the upper socket layer handle the rest. */
	resume_accept(rfp, status, dev, len, rfp->fp_sdev.aux.fd);
}

/*
 * Worker thread stub for finishing successful accept requests.
 */
static void
do_accept_reply(void)
{

	sdev_finish_accept(fp, &job_m_in);
}

/*
 * With the exception of successful accept requests, this function is called
 * whenever a reply is received for a socket driver request for which the
 * corresponding user process was suspended (as opposed to requests which just
 * suspend the worker thread), i.e., for long-lasting socket calls.  This
 * function is also called if the socket driver has died during a long-lasting
 * socket call, in which case the given message's m_type is a negative error
 * code.
 *
 * The division between the upper socket layer (socket.c) and the lower socket
 * layer (this file) here is roughly: if resuming the system call involves no
 * more than a simple replycode() call, do that here; otherwise call into the
 * upper socket layer to handle the details.  In any case, do not ever let the
 * upper socket layer deal with reply message parsing or suspension state.
 *
 * This function may or may not be called from a worker thread; as such, it
 * MUST NOT block its calling thread.  This function is called for failed
 * accept requests; successful accept requests have their replies routed
 * through sdev_finish_accept() instead, because those require a worker thread.
 */
static void
sdev_finish(struct fproc * rfp, message * m_ptr)
{
	unsigned int ctl_len, addr_len;
	int callnr, status, flags;

	/* The suspension status must just have been cleared by the caller. */
	assert(rfp->fp_blocked_on == FP_BLOCKED_ON_NONE);

	/*
	 * Free resources.  Every suspending call sets all grant fields, so we
	 * can safely revoke all of them without testing the original call.
	 */
	if (GRANT_VALID(rfp->fp_sdev.grant[0]))
		cpf_revoke(rfp->fp_sdev.grant[0]);
	if (GRANT_VALID(rfp->fp_sdev.grant[1]))
		cpf_revoke(rfp->fp_sdev.grant[1]);
	if (GRANT_VALID(rfp->fp_sdev.grant[2]))
		cpf_revoke(rfp->fp_sdev.grant[2]);

	/*
	 * Now that the socket driver call has finished (or been stopped due to
	 * driver death), we need to finish the corresponding system call from
	 * the user process.  The action to take depends on the system call.
	 */
	callnr = rfp->fp_sdev.callnr;

	switch (callnr) {
	case VFS_BIND:
	case VFS_CONNECT:
	case VFS_WRITE:
	case VFS_SENDTO:
	case VFS_SENDMSG:
	case VFS_IOCTL:
	case VFS_CLOSE:
		/*
		 * These calls all use the same SDEV_REPLY reply type and only
		 * need to reply an OK-or-error status code back to userland.
		 */
		if (m_ptr->m_type == SDEV_REPLY) {
			status = m_ptr->m_lsockdriver_vfs_reply.status;

			/*
			 * For close(2) calls, the return value must indicate
			 * that the file descriptor has been closed.
			 */
			if (callnr == VFS_CLOSE &&
			    status != OK && status != EINPROGRESS)
				status = OK;
		} else if (m_ptr->m_type < 0) {
			status = m_ptr->m_type;
		} else {
			printf("VFS: %d sent bad reply type %d for call %d\n",
			    m_ptr->m_source, m_ptr->m_type, callnr);
			status = EIO;
		}
		replycode(rfp->fp_endpoint, status);
		break;

	case VFS_READ:
	case VFS_RECVFROM:
	case VFS_RECVMSG:
		/*
		 * These calls use SDEV_RECV_REPLY.  The action to take depends
		 * on the exact call.
		 */
		ctl_len = addr_len = 0;
		flags = 0;
		if (m_ptr->m_type == SDEV_RECV_REPLY) {
			status = m_ptr->m_lsockdriver_vfs_recv_reply.status;
			ctl_len = m_ptr->m_lsockdriver_vfs_recv_reply.ctl_len;
			addr_len =
			    m_ptr->m_lsockdriver_vfs_recv_reply.addr_len;
			flags = m_ptr->m_lsockdriver_vfs_recv_reply.flags;
		} else if (m_ptr->m_type < 0) {
			status = m_ptr->m_type;
		} else {
			printf("VFS: %d sent bad reply type %d for call %d\n",
			    m_ptr->m_source, m_ptr->m_type, callnr);
			status = EIO;
		}

		switch (callnr) {
		case VFS_READ:
			replycode(rfp->fp_endpoint, status);
			break;
		case VFS_RECVFROM:
			resume_recvfrom(rfp, status, addr_len);
			break;
		case VFS_RECVMSG:
			resume_recvmsg(rfp, status, ctl_len, addr_len, flags,
			    rfp->fp_sdev.aux.buf);
			break;
		}
		break;

	case VFS_ACCEPT:
		/*
		 * This call uses SDEV_ACCEPT_REPLY.  We only get here if the
		 * accept call has failed without creating a new socket, in
		 * which case we can simply call replycode() with the error.
		 * For nothing other than consistency, we let resume_accept()
		 * handle this case too.
		 */
		addr_len = 0;
		if (m_ptr->m_type == SDEV_ACCEPT_REPLY) {
			assert(m_ptr->m_lsockdriver_vfs_accept_reply.sock_id <
			    0);
			status = m_ptr->m_lsockdriver_vfs_accept_reply.status;
			addr_len = m_ptr->m_lsockdriver_vfs_accept_reply.len;
		} else if (m_ptr->m_type < 0) {
			status = m_ptr->m_type;
		} else {
			printf("VFS: %d sent bad reply type %d for call %d\n",
			    m_ptr->m_source, m_ptr->m_type, callnr);
			status = EIO;
		}
		/*
		 * Quick rundown of m_lsockdriver_vfs_accept_reply cases:
		 *
		 * - sock_id >= 0, status == OK: new socket accepted
		 * - sock_id >= 0, status != OK: new socket must be closed
		 * - sock_id < 0, status != OK: failure accepting socket
		 * - sock_id < 0, status == OK: invalid, covered right here
		 *
		 * See libsockdriver for why there are two reply fields at all.
		 */
		if (status >= 0) {
			printf("VFS: %d sent bad status %d for call %d\n",
			    m_ptr->m_source, status, callnr);
			status = EIO;
		}
		resume_accept(rfp, status, NO_DEV, addr_len,
		    rfp->fp_sdev.aux.fd);
		break;

	default:
		/*
		 * Ultimately, enumerating all system calls that may cause
		 * socket I/O may prove too cumbersome.  In that case, the
		 * callnr field could be replaced by a field that stores the
		 * combination of the expected reply type and the action to
		 * take, for example.
		 */
		panic("VFS: socket reply %d for unknown call %d from %d",
		    m_ptr->m_type, callnr, rfp->fp_endpoint);
	}
}

/*
 * Abort the suspended socket call for the given process, because the
 * corresponding socket driver has died.
 */
void
sdev_stop(struct fproc * rfp)
{
	message m;

	assert(rfp->fp_blocked_on == FP_BLOCKED_ON_SDEV);

	rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;

	/*
	 * We use one single approach both here and when stopping worker
	 * threads: the reply message's m_type is set to an error code (always
	 * EIO for now) instead of an actual SDEV_ reply code.  We test for
	 * this case in non-suspending calls as well as in sdev_finish().
	 */
	m.m_type = EIO;
	sdev_finish(rfp, &m);
}

/*
 * Cancel the ongoing long-lasting socket call, because the calling process has
 * received a caught or terminating signal.  This function is always called
 * from a worker thread (as part of PM) work, with 'fp' set to the process that
 * issued the original system call.  The calling function has just unsuspended
 * the process out of _SDEV blocking state.  The job of this function is to
 * issue a cancel request and then block until a reply comes in; the reply may
 * indicate success, in which case it must be handled accordingly.
 */
void
sdev_cancel(void)
{
	struct smap *sp;
	message m;
	sockid_t sock_id;

	/* The suspension status must just have been cleared by the caller. */
	assert(fp->fp_blocked_on == FP_BLOCKED_ON_NONE);

	if ((sp = get_smap_by_dev(fp->fp_sdev.dev, &sock_id)) != NULL) {
		/* Prepare the request message. */
		memset(&m, 0, sizeof(m));
		m.m_type = SDEV_CANCEL;
		m.m_vfs_lsockdriver_simple.req_id = (sockid_t)who_e;
		m.m_vfs_lsockdriver_simple.sock_id = sock_id;

		/*
		 * Send the cancel request, and wait for a reply.  The reply
		 * will be for the original request and must be processed
		 * accordingly.  It is possible that the original request
		 * actually succeeded, because 1) the cancel request resulted
		 * in partial success or 2) the original reply and the cancel
		 * request crossed each other.  It is because of the second
		 * case that a socket driver must not respond at all to a
		 * cancel operation for an unknown request.
		 */
		sdev_sendrec(sp, &m);
	} else
		m.m_type = EIO;

	/*
	 * Successful accept requests require special processing, but since we
	 * are already operating from a working thread here, we need not spawn
	 * an additional worker thread for this case.
	 */
	if (m.m_type == SDEV_ACCEPT_REPLY &&
	    m.m_lsockdriver_vfs_accept_reply.sock_id >= 0)
		sdev_finish_accept(fp, &m);
	else
		sdev_finish(fp, &m);
}

/*
 * A socket driver has sent a reply to a socket request.  Process it, by either
 * waking up an active worker thread, finishing the system call from here, or
 * (in the exceptional case of accept calls) spawning a new worker thread to
 * process the reply.  This function MUST NOT block its calling thread.
 */
void
sdev_reply(void)
{
	struct fproc *rfp;
	struct smap *sp;
	struct worker_thread *wp;
	sockid_t req_id = -1;
	dev_t dev;
	int slot;

	if ((sp = get_smap_by_endpt(who_e)) == NULL) {
		printf("VFS: ignoring sock dev reply from unknown driver %d\n",
		    who_e);
		return;
	}

	switch (call_nr) {
	case SDEV_REPLY:
		req_id = m_in.m_lsockdriver_vfs_reply.req_id;
		break;
	case SDEV_SOCKET_REPLY:
		req_id = m_in.m_lsockdriver_vfs_socket_reply.req_id;
		break;
	case SDEV_ACCEPT_REPLY:
		req_id = m_in.m_lsockdriver_vfs_accept_reply.req_id;
		break;
	case SDEV_RECV_REPLY:
		req_id = m_in.m_lsockdriver_vfs_recv_reply.req_id;
		break;
	case SDEV_SELECT1_REPLY:
		dev = make_smap_dev(sp,
		    m_in.m_lsockdriver_vfs_select_reply.sock_id);
		select_sdev_reply1(dev,
		    m_in.m_lsockdriver_vfs_select_reply.status);
		return;
	case SDEV_SELECT2_REPLY:
		dev = make_smap_dev(sp,
		    m_in.m_lsockdriver_vfs_select_reply.sock_id);
		select_sdev_reply2(dev,
		    m_in.m_lsockdriver_vfs_select_reply.status);
		return;
	default:
		printf("VFS: ignoring unknown sock dev reply %d from %d\n",
		    call_nr, who_e);
		return;
	}

	if (isokendpt((endpoint_t)req_id, &slot) != OK) {
		printf("VFS: ignoring sock dev reply from %d for unknown %d\n",
		    who_e, req_id);
		return;
	}

	rfp = &fproc[slot];
	wp = rfp->fp_worker;
	if (wp != NULL && wp->w_task == who_e && wp->w_drv_sendrec != NULL) {
		assert(!fp_is_blocked(rfp));
		*wp->w_drv_sendrec = m_in;
		wp->w_drv_sendrec = NULL;
		worker_signal(wp);	/* resume suspended thread */
		/*
		 * It is up to the worker thread to 1) check that the reply is
		 * of the right type for the request, and 2) keep in mind that
		 * the reply type may be EIO in case the socket driver died.
		 */
	} else if (rfp->fp_blocked_on != FP_BLOCKED_ON_SDEV ||
	    get_smap_by_dev(rfp->fp_sdev.dev, NULL) != sp) {
		printf("VFS: ignoring sock dev reply, %d not blocked on %d\n",
		    rfp->fp_endpoint, who_e);
		return;
	} else if (call_nr == SDEV_ACCEPT_REPLY &&
	    m_in.m_lsockdriver_vfs_accept_reply.sock_id >= 0) {
		/*
		 * For accept replies that return a new socket, we need to
		 * spawn a worker thread, because accept calls may block (so
		 * there will no longer be a worker thread) and processing the
		 * reply requires additional blocking calls (which we cannot
		 * issue from the main thread).  This is tricky.  Under no
		 * circumstances may we "lose" a legitimate reply, because this
		 * would lead to resource leaks in the socket driver.  To this
		 * end, we rely on the current worker thread model to
		 * prioritize regular work over PM work.  Still, sdev_cancel()
		 * may end up receiving the accept reply if it was already
		 * blocked waiting for the reply message, and it must then
		 * perform the same tasks.
		 */
		/*
		 * It is possible that if all threads are in use, there is a
		 * "gap" between starting the thread and its activation.  The
		 * main problem for this case is that the socket driver dies
		 * within that gap.  For accepts, we address this with no less
		 * than two checks: 1) in this file, by looking up the smap
		 * entry by the reply source endpoint again - if the entry is
		 * no longer valid, the socket driver must have died; 2) in
		 * socket.c, by revalidating the original listening socket - if
		 * the listening socket has been invalidated, the driver died.
		 *
		 * Since we unsuspend the process now, a socket driver sending
		 * two accept replies in a row may never cause VFS to attempt
		 * spawning two threads; the second reply should be ignored.
		 */
		assert(fp->fp_func == NULL);

		worker_start(rfp, do_accept_reply, &m_in, FALSE /*use_spare*/);

		/*
		 * TODO: I just introduced the notion of not using the fp_u
		 * union across yields after unsuspension, but for socket calls
		 * we have a lot of socket state to carry over, so I'm now
		 * immediately violating my own rule again here.  Possible
		 * solutions: 1) introduce another blocking state just to mark
		 * the fp_u union in use (this has side effects though), 2)
		 * introduce a pseudo message type which covers both the accept
		 * reply fields and the fp_u state (do_pending_pipe does this),
		 * or 3) add a fp_flags flag for this purpose.  In any case,
		 * the whole point is that we catch any attempts to reuse fp_u
		 * for other purposes and thus cause state corruption. This
		 * should not happen anyway, but it's too dangerous to leave
		 * entirely unchecked.  --dcvmoole
		 */
		rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;
	} else {
		rfp->fp_blocked_on = FP_BLOCKED_ON_NONE;

		sdev_finish(rfp, &m_in);
	}
}
