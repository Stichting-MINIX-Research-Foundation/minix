/*
 * Unix Domain Sockets Implementation (PF_UNIX, PF_LOCAL)
 * This code handles ioctl(2) commands to implement the socket API.
 * Some helper functions are also present.
 */

#include "uds.h"

static int
perform_connection(devminor_t minorx, devminor_t minory,
	struct sockaddr_un *addr)
{
	/*
	 * There are several places were a connection is established, the
	 * initiating call being one of accept(2), connect(2), socketpair(2).
	 */
	dprintf(("UDS: perform_connection(%d, %d)\n", minorx, minory));

	/*
	 * Only connection-oriented types are acceptable and only equal
	 * types can connect to each other.
	 */
	if ((uds_fd_table[minorx].type != SOCK_SEQPACKET &&
	    uds_fd_table[minorx].type != SOCK_STREAM) ||
	    uds_fd_table[minorx].type != uds_fd_table[minory].type)
		return EINVAL;

	/* Connect the pair of sockets. */
	uds_fd_table[minorx].peer = minory;
	uds_fd_table[minory].peer = minorx;

	/* Set the address of both sockets */
	memcpy(&uds_fd_table[minorx].addr, addr, sizeof(struct sockaddr_un));
	memcpy(&uds_fd_table[minory].addr, addr, sizeof(struct sockaddr_un));

	return OK;
}

static int
do_accept(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	devminor_t minorparent; /* minor number of parent (server) */
	devminor_t minorpeer;
	int rc, i;
	struct sockaddr_un addr;

	dprintf(("UDS: do_accept(%d)\n", minor));

	/*
	 * Somewhat weird logic is used in this function, so here's an
	 * overview... The minor number is the server's client socket
	 * (the socket to be returned by accept()). The data waiting
	 * for us in the IO Grant is the address that the server is
	 * listening on. This function uses the address to find the
	 * server's descriptor. From there we can perform the
	 * connection or suspend and wait for a connect().
	 */

	/* This IOCTL must be called on a 'fresh' socket. */
	if (uds_fd_table[minor].type != -1)
		return EINVAL;

	/* Get the server's address */
	if ((rc = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &addr,
	    sizeof(struct sockaddr_un))) != OK)
		return rc;

	/* Locate the server socket. */
	for (i = 0; i < NR_FDS; i++) {
		if (uds_fd_table[i].addr.sun_family == AF_UNIX &&
		    !strncmp(addr.sun_path, uds_fd_table[i].addr.sun_path,
		    sizeof(uds_fd_table[i].addr.sun_path)) &&
			uds_fd_table[i].listening == 1)
			break;
	}

	if (i == NR_FDS)
		return EINVAL;

	minorparent = i; /* parent */

	/* We are the parent's child. */
	uds_fd_table[minorparent].child = minor;

	/*
	 * The peer has the same type as the parent. we need to be that
	 * type too.
	 */
	uds_fd_table[minor].type = uds_fd_table[minorparent].type;

	/* Locate the peer to accept in the parent's backlog. */
	minorpeer = -1;
	for (i = 0; i < uds_fd_table[minorparent].backlog_size; i++) {
		if (uds_fd_table[minorparent].backlog[i] != -1) {
			minorpeer = uds_fd_table[minorparent].backlog[i];
			uds_fd_table[minorparent].backlog[i] = -1;
			break;
		}
	}

	if (minorpeer == -1) {
		dprintf(("UDS: do_accept(%d): suspend\n", minor));

		/*
		 * There are no peers in the backlog, suspend and wait for one
		 * to show up.
		 */
		uds_fd_table[minor].suspended = UDS_SUSPENDED_ACCEPT;

		return EDONTREPLY;
	}

	dprintf(("UDS: connecting %d to %d -- parent is %d\n", minor,
	    minorpeer, minorparent));

	if ((rc = perform_connection(minor, minorpeer, &addr)) != OK) {
		dprintf(("UDS: do_accept(%d): connection failed\n", minor));

		return rc;
	}

	uds_fd_table[minorparent].child = -1;

	/* If the peer is blocked on connect() or write(), revive the peer. */
	if (uds_fd_table[minorpeer].suspended == UDS_SUSPENDED_CONNECT ||
	    uds_fd_table[minorpeer].suspended == UDS_SUSPENDED_WRITE) {
		dprintf(("UDS: do_accept(%d): revive %d\n", minor, minorpeer));
		uds_unsuspend(minorpeer);
	}

	/* See if we can satisfy an ongoing select. */
	if ((uds_fd_table[minorpeer].sel_ops & CDEV_OP_WR) &&
	    uds_fd_table[minorpeer].size < UDS_BUF) {
		/* A write on the peer is possible now. */
		chardriver_reply_select(uds_fd_table[minorpeer].sel_endpt,
		    minorpeer, CDEV_OP_WR);
		uds_fd_table[minorpeer].sel_ops &= ~CDEV_OP_WR;
	}

	return OK;
}

static int
do_connect(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	int child, peer;
	struct sockaddr_un addr;
	int rc, i, j;

	dprintf(("UDS: do_connect(%d)\n", minor));

	/* Only connection oriented sockets can connect. */
	if (uds_fd_table[minor].type != SOCK_STREAM &&
	    uds_fd_table[minor].type != SOCK_SEQPACKET)
		return EINVAL;

	/* The socket must not be connecting or connected already. */
	peer = uds_fd_table[minor].peer;
	if (peer != -1) {
		if (uds_fd_table[peer].peer == -1)
			return EALREADY;	/* connecting */
		else
			return EISCONN;		/* connected */
	}

	if ((rc = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &addr,
	    sizeof(struct sockaddr_un))) != OK)
		return rc;

	if ((rc = checkperms(uds_fd_table[minor].owner, addr.sun_path,
	    sizeof(addr.sun_path))) != OK)
		return rc;

	/*
	 * Look for a socket of the same type that is listening on the
	 * address we want to connect to.
	 */
	for (i = 0; i < NR_FDS; i++) {
		if (uds_fd_table[minor].type != uds_fd_table[i].type)
			continue;
		if (!uds_fd_table[i].listening)
			continue;
		if (uds_fd_table[i].addr.sun_family != AF_UNIX)
			continue;
		if (strncmp(addr.sun_path, uds_fd_table[i].addr.sun_path,
		    sizeof(uds_fd_table[i].addr.sun_path)))
			continue;

		/* Found a matching socket. */
		break;
	}

	if (i == NR_FDS)
		return ECONNREFUSED;

	/* If the server is blocked on an accept, perform the connection. */
	if ((child = uds_fd_table[i].child) != -1) {
		rc = perform_connection(minor, child, &addr);

		if (rc != OK)
			return rc;

		uds_fd_table[i].child = -1;

		dprintf(("UDS: do_connect(%d): revive %d\n", minor, child));

		/* Wake up the accepting party. */
		uds_unsuspend(child);

		return OK;
	}

	dprintf(("UDS: adding %d to %d's backlog\n", minor, i));

	/* Look for a free slot in the backlog. */
	rc = -1;
	for (j = 0; j < uds_fd_table[i].backlog_size; j++) {
		if (uds_fd_table[i].backlog[j] == -1) {
			uds_fd_table[i].backlog[j] = minor;

			rc = 0;
			break;
		}
	}

	if (rc == -1)
		return ECONNREFUSED;	/* backlog is full */

	/* See if the server is blocked on select(). */
	if (uds_fd_table[i].sel_ops & CDEV_OP_RD) {
		/* Satisfy a read-type select on the server. */
		chardriver_reply_select(uds_fd_table[i].sel_endpt, i,
		    CDEV_OP_RD);

		uds_fd_table[i].sel_ops &= ~CDEV_OP_RD;
	}

	/* We found our server. */
	uds_fd_table[minor].peer = i;

	memcpy(&uds_fd_table[minor].addr, &addr, sizeof(struct sockaddr_un));

	dprintf(("UDS: do_connect(%d): suspend\n", minor));

	/* Suspend until the server side accepts the connection. */
	uds_fd_table[minor].suspended = UDS_SUSPENDED_CONNECT;

	return EDONTREPLY;
}

static int
do_listen(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	int rc;
	int backlog_size;

	dprintf(("UDS: do_listen(%d)\n", minor));

	/* Ensure the socket has a type and is bound. */
	if (uds_fd_table[minor].type == -1 ||
	    uds_fd_table[minor].addr.sun_family != AF_UNIX)
		return EINVAL;

	/* listen(2) supports only two socket types. */
	if (uds_fd_table[minor].type != SOCK_STREAM &&
	    uds_fd_table[minor].type != SOCK_SEQPACKET)
		return EOPNOTSUPP;

	/*
	 * The POSIX standard doesn't say what to do if listen() has
	 * already been called.  Well, there isn't an errno.  We silently
	 * let it happen, but if listen() has already been called, we
	 * don't allow the backlog to shrink.
	 */
	if ((rc = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &backlog_size,
	    sizeof(backlog_size))) != OK)
		return rc;

	if (uds_fd_table[minor].listening == 0) {
		/* Set the backlog size to a reasonable value. */
		if (backlog_size <= 0 || backlog_size > UDS_SOMAXCONN)
			backlog_size = UDS_SOMAXCONN;

		uds_fd_table[minor].backlog_size = backlog_size;
	} else {
		/* Allow the user to expand the backlog size. */
		if (backlog_size > uds_fd_table[minor].backlog_size &&
		    backlog_size < UDS_SOMAXCONN)
			uds_fd_table[minor].backlog_size = backlog_size;

		/*
		 * Don't let the user shrink the backlog_size, as we might
		 * have clients waiting in those slots.
		 */
	}

	/* This socket is now listening. */
	uds_fd_table[minor].listening = 1;

	return OK;
}

static int
do_socket(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	int rc, type;

	dprintf(("UDS: do_socket(%d)\n", minor));

	/* The socket type can only be set once. */
	if (uds_fd_table[minor].type != -1)
		return EINVAL;

	/* Get the requested type. */
	if ((rc = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &type,
	    sizeof(type))) != OK)
		return rc;

	/* Assign the type if it is valid only. */
	switch (type) {
	case SOCK_STREAM:
	case SOCK_DGRAM:
	case SOCK_SEQPACKET:
		uds_fd_table[minor].type = type;
		return OK;

	default:
		return EINVAL;
	}
}

static int
do_bind(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	struct sockaddr_un addr;
	int rc, i;

	dprintf(("UDS: do_bind(%d)\n", minor));

	/* If the type hasn't been set by do_socket() yet, OR an attempt
	 * to re-bind() a non-SOCK_DGRAM socket is made, fail the call.
	 */
	if ((uds_fd_table[minor].type == -1) ||
	    (uds_fd_table[minor].addr.sun_family == AF_UNIX &&
	    uds_fd_table[minor].type != SOCK_DGRAM))
		return EINVAL;

	if ((rc = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &addr,
	    sizeof(struct sockaddr_un))) != OK)
		return rc;

	/* Do some basic sanity checks on the address. */
	if (addr.sun_family != AF_UNIX)
		return EAFNOSUPPORT;

	if (addr.sun_path[0] == '\0')
		return ENOENT;

	if ((rc = checkperms(uds_fd_table[minor].owner, addr.sun_path,
		sizeof(addr.sun_path))) != OK)
		return rc;

	/* Make sure the address isn't already in use by another socket. */
	for (i = 0; i < NR_FDS; i++) {
		if (uds_fd_table[i].addr.sun_family == AF_UNIX &&
		    !strncmp(addr.sun_path, uds_fd_table[i].addr.sun_path,
		    sizeof(uds_fd_table[i].addr.sun_path))) {
			/* Another socket is bound to this sun_path. */
			return EADDRINUSE;
		}
	}

	/* Looks good, perform the bind(). */
	memcpy(&uds_fd_table[minor].addr, &addr, sizeof(struct sockaddr_un));

	return OK;
}

static int
do_getsockname(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	dprintf(("UDS: do_getsockname(%d)\n", minor));

	/*
	 * Unconditionally send the address we have assigned to this socket.
	 * The POSIX standard doesn't say what to do if the address hasn't been
	 * set.  If the address isn't currently set, then the user will get
	 * NULL bytes.  Note: libc depends on this behavior.
	 */
	return sys_safecopyto(endpt, grant, 0,
	    (vir_bytes) &uds_fd_table[minor].addr, sizeof(struct sockaddr_un));
}

static int
do_getpeername(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	int peer_minor;

	dprintf(("UDS: do_getpeername(%d)\n", minor));

	/* Check that the socket is connected with a valid peer. */
	if (uds_fd_table[minor].peer != -1) {
		peer_minor = uds_fd_table[minor].peer;

		/* Copy the address from the peer. */
		return sys_safecopyto(endpt, grant, 0,
		    (vir_bytes) &uds_fd_table[peer_minor].addr,
		    sizeof(struct sockaddr_un));
	} else if (uds_fd_table[minor].err == ECONNRESET) {
		uds_fd_table[minor].err = 0;

		return ECONNRESET;
	} else
		return ENOTCONN;
}

static int
do_shutdown(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	int rc, how;

	dprintf(("UDS: do_shutdown(%d)\n", minor));

	/* The socket must be connection oriented. */
	if (uds_fd_table[minor].type != SOCK_STREAM &&
	    uds_fd_table[minor].type != SOCK_SEQPACKET)
		return EINVAL;

	if (uds_fd_table[minor].peer == -1) {
		/* shutdown(2) is only valid for connected sockets. */
		if (uds_fd_table[minor].err == ECONNRESET)
			return ECONNRESET;
		else
			return ENOTCONN;
	}

	/* Get the 'how' parameter from the caller. */
	if ((rc = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &how,
	    sizeof(how))) != OK)
		return rc;

	switch (how) {
	case SHUT_RD:		/* Take away read permission. */
		uds_fd_table[minor].mode &= ~UDS_R;
		break;

	case SHUT_WR:		/* Take away write permission. */
		uds_fd_table[minor].mode &= ~UDS_W;
		break;

	case SHUT_RDWR:		/* Shut down completely. */
		uds_fd_table[minor].mode = 0;
		break;

	default:
		return EINVAL;
	}

	return OK;
}

static int
do_socketpair(devminor_t minorx, endpoint_t endpt, cp_grant_id_t grant)
{
	int rc;
	dev_t minorin;
	devminor_t minory;
	struct sockaddr_un addr;

	dprintf(("UDS: do_socketpair(%d)\n", minorx));

	/* The ioctl argument is the minor number of the second socket. */
	if ((rc = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &minorin,
	    sizeof(minorin))) != OK)
		return rc;

	minory = minor(minorin);

	dprintf(("UDS: socketpair(%d, %d,)\n", minorx, minory));

	/* Security check: both sockets must have the same owner endpoint. */
	if (uds_fd_table[minorx].owner != uds_fd_table[minory].owner)
		return EPERM;

	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = 'X';
	addr.sun_path[1] = '\0';

	return perform_connection(minorx, minory, &addr);
}

static int
do_getsockopt_sotype(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	dprintf(("UDS: do_getsockopt_sotype(%d)\n", minor));

	/* If the type hasn't been set yet, we fail the call. */
	if (uds_fd_table[minor].type == -1)
		return EINVAL;

	return sys_safecopyto(endpt, grant, 0,
	    (vir_bytes) &uds_fd_table[minor].type, sizeof(int));
}

static int
do_getsockopt_peercred(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	int peer_minor;
	int rc;
	struct uucred cred;

	dprintf(("UDS: do_getsockopt_peercred(%d)\n", minor));

	if (uds_fd_table[minor].peer == -1) {
		if (uds_fd_table[minor].err == ECONNRESET) {
			uds_fd_table[minor].err = 0;

			return ECONNRESET;
		} else
			return ENOTCONN;
	}

	peer_minor = uds_fd_table[minor].peer;

	/* Obtain the peer's credentials and copy them out. */
	if ((rc = getnucred(uds_fd_table[peer_minor].owner, &cred)) < 0)
		return rc;

	return sys_safecopyto(endpt, grant, 0, (vir_bytes) &cred,
	    sizeof(struct uucred));
}

static int
do_getsockopt_sndbuf(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	size_t sndbuf = UDS_BUF;

	dprintf(("UDS: do_getsockopt_sndbuf(%d)\n", minor));

	return sys_safecopyto(endpt, grant, 0, (vir_bytes) &sndbuf,
	    sizeof(sndbuf));
}

static int
do_setsockopt_sndbuf(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	int rc;
	size_t sndbuf;

	dprintf(("UDS: do_setsockopt_sndbuf(%d)\n", minor));

	if ((rc = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &sndbuf,
	    sizeof(sndbuf))) != OK)
		return rc;

	/* The send buffer is limited to 32KB at the moment. */
	if (sndbuf > UDS_BUF)
		return ENOSYS;

	/* FIXME: actually shrink the buffer. */
	return OK;
}

static int
do_getsockopt_rcvbuf(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	size_t rcvbuf = UDS_BUF;

	dprintf(("UDS: do_getsockopt_rcvbuf(%d)\n", minor));

	return sys_safecopyto(endpt, grant, 0, (vir_bytes) &rcvbuf,
	    sizeof(rcvbuf));
}

static int
do_setsockopt_rcvbuf(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	int rc;
	size_t rcvbuf;

	dprintf(("UDS: do_setsockopt_rcvbuf(%d)\n", minor));

	if ((rc = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &rcvbuf,
	    sizeof(rcvbuf))) != OK)
		return rc;

	/* The receive buffer is limited to 32KB at the moment. */
	if (rcvbuf > UDS_BUF)
		return ENOSYS;

	/* FIXME: actually shrink the buffer. */
	return OK;
}

static int
do_sendto(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	int rc;
	struct sockaddr_un addr;

	dprintf(("UDS: do_sendto(%d)\n", minor));

	/* This IOCTL is only for SOCK_DGRAM sockets. */
	if (uds_fd_table[minor].type != SOCK_DGRAM)
		return EINVAL;

	if ((rc = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &addr,
	    sizeof(struct sockaddr_un))) != OK)
		return rc;

	/* Do some basic sanity checks on the address. */
	if (addr.sun_family != AF_UNIX || addr.sun_path[0] == '\0')
		return EINVAL;

	if ((rc = checkperms(uds_fd_table[minor].owner, addr.sun_path,
	    sizeof(addr.sun_path))) != OK)
		return rc;

	memcpy(&uds_fd_table[minor].target, &addr, sizeof(struct sockaddr_un));

	return OK;
}

static int
do_recvfrom(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	dprintf(("UDS: do_recvfrom(%d)\n", minor));

	return sys_safecopyto(endpt, grant, 0,
	    (vir_bytes) &uds_fd_table[minor].source,
	    sizeof(struct sockaddr_un));
}

static int
send_fds(devminor_t minor, struct msg_control *msg_ctrl,
	struct ancillary *data)
{
	int i, rc, nfds, totalfds;
	endpoint_t from_ep;
	struct msghdr msghdr;
	struct cmsghdr *cmsg = NULL;

	dprintf(("UDS: send_fds(%d)\n", minor));

	from_ep = uds_fd_table[minor].owner;

	/* Obtain this socket's credentials. */
	if ((rc = getnucred(from_ep, &data->cred)) < 0)
		return rc;

	dprintf(("UDS: minor=%d cred={%d,%d,%d}\n", minor, data->cred.pid,
	    data->cred.uid, data->cred.gid));

	totalfds = data->nfiledes;

	memset(&msghdr, '\0', sizeof(struct msghdr));
	msghdr.msg_control = msg_ctrl->msg_control;
	msghdr.msg_controllen = msg_ctrl->msg_controllen;

	for (cmsg = CMSG_FIRSTHDR(&msghdr); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msghdr, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET ||
		    cmsg->cmsg_type != SCM_RIGHTS)
			continue;

		nfds = MIN((cmsg->cmsg_len-CMSG_LEN(0))/sizeof(int), OPEN_MAX);

		for (i = 0; i < nfds; i++) {
			if (totalfds == OPEN_MAX)
				return EOVERFLOW;

			data->fds[totalfds] = ((int *) CMSG_DATA(cmsg))[i];
			dprintf(("UDS: minor=%d fd[%d]=%d\n", minor, totalfds,
			    data->fds[totalfds]));
			totalfds++;
		}
	}

	for (i = data->nfiledes; i < totalfds; i++) {
		if ((rc = copyfd(from_ep, data->fds[i], COPYFD_FROM)) < 0) {
			printf("UDS: copyfd(COPYFD_FROM) failed: %d\n", rc);

			/* Revert the successful copyfd() calls made so far. */
			for (i--; i >= data->nfiledes; i--)
				close(data->fds[i]);

			return rc;
		}

		dprintf(("UDS: send_fds(): %d -> %d\n", data->fds[i], rc));

		data->fds[i] = rc;	/* this is now the local FD */
	}

	data->nfiledes = totalfds;

	return OK;
}

/*
 * This function calls close() for all of the FDs in flight.  This is used
 * when a Unix Domain Socket is closed and there exists references to file
 * descriptors that haven't been received with recvmsg().
 */
int
uds_clear_fds(devminor_t minor, struct ancillary *data)
{
	int i;

	dprintf(("UDS: uds_clear_fds(%d)\n", minor));

	for (i = 0; i < data->nfiledes; i++) {
		dprintf(("UDS: uds_clear_fds() => %d\n", data->fds[i]));

		close(data->fds[i]);

		data->fds[i] = -1;
	}

	data->nfiledes = 0;

	return OK;
}

static int
recv_fds(devminor_t minor, struct ancillary *data,
	struct msg_control *msg_ctrl)
{
	int rc, i, j, fds[OPEN_MAX];
	struct msghdr msghdr;
	struct cmsghdr *cmsg;
	endpoint_t to_ep;

	dprintf(("UDS: recv_fds(%d)\n", minor));

	msghdr.msg_control = msg_ctrl->msg_control;
	msghdr.msg_controllen = msg_ctrl->msg_controllen;

	cmsg = CMSG_FIRSTHDR(&msghdr);
	cmsg->cmsg_len = CMSG_LEN(sizeof(int) * data->nfiledes);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;

	to_ep = uds_fd_table[minor].owner;

	/* Copy to the target endpoint. */
	for (i = 0; i < data->nfiledes; i++) {
		if ((rc = copyfd(to_ep, data->fds[i], COPYFD_TO)) < 0) {
			printf("UDS: copyfd(COPYFD_TO) failed: %d\n", rc);

			/* Revert the successful copyfd() calls made so far. */
			for (i--; i >= 0; i--)
				(void) copyfd(to_ep, fds[i], COPYFD_CLOSE);

			return rc;
		}

		fds[i] = rc;		/* this is now the remote FD */
	}

	/* Close the local copies only once the entire procedure succeeded. */
	for (i = 0; i < data->nfiledes; i++) {
		dprintf(("UDS: recv_fds(): %d -> %d\n", data->fds[i], fds[i]));

		((int *)CMSG_DATA(cmsg))[i] = fds[i];

		close(data->fds[i]);

		data->fds[i] = -1;
	}

	data->nfiledes = 0;

	return OK;
}

static int
recv_cred(devminor_t minor, struct ancillary *data,
	struct msg_control *msg_ctrl)
{
	struct msghdr msghdr;
	struct cmsghdr *cmsg;

	dprintf(("UDS: recv_cred(%d)\n", minor));

	msghdr.msg_control = msg_ctrl->msg_control;
	msghdr.msg_controllen = msg_ctrl->msg_controllen;

	cmsg = CMSG_FIRSTHDR(&msghdr);
	if (cmsg->cmsg_len > 0)
		cmsg = CMSG_NXTHDR(&msghdr, cmsg);

	cmsg->cmsg_len = CMSG_LEN(sizeof(struct uucred));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_CREDS;
	memcpy(CMSG_DATA(cmsg), &data->cred, sizeof(struct uucred));

	return OK;
}

static int
do_sendmsg(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	int peer, rc, i;
	struct msg_control msg_ctrl;

	dprintf(("UDS: do_sendmsg(%d)\n", minor));

	memset(&msg_ctrl, '\0', sizeof(struct msg_control));

	if ((rc = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &msg_ctrl,
	    sizeof(struct msg_control))) != OK)
		return rc;

	/* Locate the peer. */
	peer = -1;
	if (uds_fd_table[minor].type == SOCK_DGRAM) {
		if (uds_fd_table[minor].target.sun_path[0] == '\0' ||
		    uds_fd_table[minor].target.sun_family != AF_UNIX)
			return EDESTADDRREQ;

		for (i = 0; i < NR_FDS; i++) {
			/*
			 * Look for a SOCK_DGRAM socket that is bound on the
			 * target address.
			 */
			if (uds_fd_table[i].type == SOCK_DGRAM &&
			    uds_fd_table[i].addr.sun_family == AF_UNIX &&
			    !strncmp(uds_fd_table[minor].target.sun_path,
			    uds_fd_table[i].addr.sun_path,
			    sizeof(uds_fd_table[i].addr.sun_path))) {
				peer = i;
				break;
			}
		}

		if (peer == -1)
			return ENOENT;
	} else {
		peer = uds_fd_table[minor].peer;
		if (peer == -1)
			return ENOTCONN;
	}

	dprintf(("UDS: sendmsg(%d) -- peer=%d\n", minor, peer));

	/*
	 * Note: it's possible that there is already some file descriptors in
	 * ancillary_data if the peer didn't call recvmsg() yet.  That's okay.
	 * The receiver will get the current file descriptors plus the new
	 * ones.
	 */
	return send_fds(minor, &msg_ctrl, &uds_fd_table[peer].ancillary_data);
}

static int
do_recvmsg(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	int rc;
	struct msg_control msg_ctrl;
	socklen_t clen_avail = 0;
	socklen_t clen_needed = 0;
	socklen_t clen_desired = 0;

	dprintf(("UDS: do_recvmsg(%d)\n", minor));
	dprintf(("UDS: minor=%d credentials={pid:%d,uid:%d,gid:%d}\n", minor,
	    uds_fd_table[minor].ancillary_data.cred.pid,
	    uds_fd_table[minor].ancillary_data.cred.uid,
	    uds_fd_table[minor].ancillary_data.cred.gid));

	memset(&msg_ctrl, '\0', sizeof(struct msg_control));

	/*
	 * Get the msg_control from the user.  It will include the
	 * amount of space the user has allocated for control data.
	 */
	if ((rc = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &msg_ctrl,
	    sizeof(struct msg_control))) != OK)
		return rc;

	clen_avail = MIN(msg_ctrl.msg_controllen, MSG_CONTROL_MAX);

	if (uds_fd_table[minor].ancillary_data.nfiledes > 0) {
		clen_needed = CMSG_SPACE(sizeof(int) *
		    uds_fd_table[minor].ancillary_data.nfiledes);
	}

	/* if there is room we also include credentials */
	clen_desired = clen_needed + CMSG_SPACE(sizeof(struct uucred));

	if (clen_needed > clen_avail)
		return EOVERFLOW;

	if (uds_fd_table[minor].ancillary_data.nfiledes > 0) {
		if ((rc = recv_fds(minor, &uds_fd_table[minor].ancillary_data,
		    &msg_ctrl)) != OK)
			return rc;
	}

	if (clen_desired <= clen_avail) {
		rc = recv_cred(minor, &uds_fd_table[minor].ancillary_data,
		    &msg_ctrl);
		if (rc != OK)
			return rc;
		msg_ctrl.msg_controllen = clen_desired;
	} else
		msg_ctrl.msg_controllen = clen_needed;

	/* Send the control data to the user. */
	return sys_safecopyto(endpt, grant, 0, (vir_bytes) &msg_ctrl,
	    sizeof(struct msg_control));
}

static int
do_fionread(devminor_t minor, endpoint_t endpt, cp_grant_id_t grant)
{
	int rc;

	rc = uds_perform_read(minor, NONE, GRANT_INVALID, UDS_BUF, 1);

	/* What should we do on error?  Just set to zero for now. */
	if (rc < 0)
		rc = 0;

	return sys_safecopyto(endpt, grant, 0, (vir_bytes) &rc, sizeof(rc));
}

int
uds_do_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant)
{
	int rc;

	switch (request) {
	case NWIOSUDSCONN:
		/* Connect to a listening socket -- connect(). */
		rc = do_connect(minor, endpt, grant);

		break;

	case NWIOSUDSACCEPT:
		/* Accept an incoming connection -- accept(). */
		rc = do_accept(minor, endpt, grant);

		break;

	case NWIOSUDSBLOG:
		/*
		 * Set the backlog_size and put the socket into the listening
		 * state -- listen().
		 */
		rc = do_listen(minor, endpt, grant);

		break;

	case NWIOSUDSTYPE:
		/* Set the SOCK_ type for this socket -- socket(). */
		rc = do_socket(minor, endpt, grant);

		break;

	case NWIOSUDSADDR:
		/* Set the address for this socket -- bind(). */
		rc = do_bind(minor, endpt, grant);

		break;

	case NWIOGUDSADDR:
		/* Get the address for this socket -- getsockname(). */
		rc = do_getsockname(minor, endpt, grant);

		break;

	case NWIOGUDSPADDR:
		/* Get the address for the peer -- getpeername(). */
		rc = do_getpeername(minor, endpt, grant);

		break;

	case NWIOSUDSSHUT:
		/*
		 * Shut down a socket for reading, writing, or both --
		 * shutdown().
		 */
		rc = do_shutdown(minor, endpt, grant);

		break;

	case NWIOSUDSPAIR:
		/* Connect two sockets -- socketpair(). */
		rc = do_socketpair(minor, endpt, grant);

		break;

	case NWIOGUDSSOTYPE:
		/* Get socket type -- getsockopt(SO_TYPE). */
		rc = do_getsockopt_sotype(minor, endpt, grant);

		break;

	case NWIOGUDSPEERCRED:
		/* Get peer endpoint -- getsockopt(SO_PEERCRED). */
		rc = do_getsockopt_peercred(minor, endpt, grant);

		break;

	case NWIOSUDSTADDR:
		/* Set target address -- sendto(). */
		rc = do_sendto(minor, endpt, grant);

		break;

	case NWIOGUDSFADDR:
		/* Get from address -- recvfrom(). */
		rc = do_recvfrom(minor, endpt, grant);

		break;

	case NWIOGUDSSNDBUF:
		/* Get the send buffer size -- getsockopt(SO_SNDBUF). */
		rc = do_getsockopt_sndbuf(minor, endpt, grant);

		break;

	case NWIOSUDSSNDBUF:
		/* Set the send buffer size -- setsockopt(SO_SNDBUF). */
		rc = do_setsockopt_sndbuf(minor, endpt, grant);

		break;

	case NWIOGUDSRCVBUF:
		/* Get the send buffer size -- getsockopt(SO_SNDBUF). */
		rc = do_getsockopt_rcvbuf(minor, endpt, grant);

		break;

	case NWIOSUDSRCVBUF:
		/* Set the send buffer size -- setsockopt(SO_SNDBUF). */
		rc = do_setsockopt_rcvbuf(minor, endpt, grant);

		break;

	case NWIOSUDSCTRL:
		/* Set the control data -- sendmsg(). */
		rc = do_sendmsg(minor, endpt, grant);

		break;

	case NWIOGUDSCTRL:
		/* Set the control data -- recvmsg(). */
		rc = do_recvmsg(minor, endpt, grant);

		break;

	case FIONREAD:
		/*
		 * Get the number of bytes immediately available for reading.
		 */
		rc = do_fionread(minor, endpt, grant);

		break;

	default:
		/*
		 * The IOCTL command is not valid for /dev/uds -- this happens
		 * a lot and is normal.  A lot of libc functions determine the
		 * socket type with IOCTLs.  Any unrecognized requests simply
		 * get an ENOTTY response.
		 */

		rc = ENOTTY;
	}

	return rc;
}
