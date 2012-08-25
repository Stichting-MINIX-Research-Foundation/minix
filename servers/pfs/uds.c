/*
 * Unix Domain Sockets Implementation (PF_UNIX, PF_LOCAL)
 * This code handles ioctl(2) commands to implement the socket API.
 * Some helper functions are also present.
 *
 * The entry points into this file are...
 *
 *   uds_init:               initialize the descriptor table.
 *   do_accept:              handles the      accept(2) syscall.
 *   do_connect:             handles the     connect(2) syscall.
 *   do_listen:              handles the      listen(2) syscall.
 *   do_socket:              handles the      socket(2) syscall.
 *   do_bind:                handles the        bind(2) syscall.
 *   do_getsockname:         handles the getsockname(2) syscall.
 *   do_getpeername:         handles the getpeername(2) syscall.
 *   do_shutdown:            handles the    shutdown(2) syscall.
 *   do_socketpair:          handles the  socketpair(2) syscall.
 *   do_getsockopt_sotype:   handles the  getsockopt(2) syscall.
 *   do_getsockopt_peercred: handles the  getsockopt(2) syscall.
 *   do_getsockopt_sndbuf:   handles the  getsockopt(2) syscall.
 *   do_setsockopt_sndbuf:   handles the  setsockopt(2) syscall.
 *   do_getsockopt_rcvbuf:   handles the  getsockopt(2) syscall.
 *   do_setsockopt_rcvbuf:   handles the  setsockopt(2) syscall.
 *   do_sendto:              handles the      sendto(2) syscall.
 *   do_recvfrom:            handles the    recvfrom(2) syscall.
 *   do_sendmsg:             handles the     sendmsg(2) syscall.
 *   do_recvmsg:             handles the     recvmsg(2) syscall.
 *   perform_connection:     performs the connection of two descriptors.
 *   clear_fds:              calls put_filp for undelivered FDs.
 *
 * Also see...
 *
 *   table.c, dev_uds.c, uds.h
 */

#define DEBUG 0

#include "inc.h"
#include "const.h"
#include "glo.h"
#include "uds.h"

/* File Descriptor Table */
uds_fd_t uds_fd_table[NR_FDS];

/* initialize the descriptor table */
void uds_init(void)
{
	/*
	 * Setting everything to NULL implicitly sets the
	 * state to UDS_FREE.
	 */
	memset(uds_fd_table, '\0', sizeof(uds_fd_t) * NR_FDS);
}

/* check the permissions of a socket file */
static int check_perms(int minor, struct sockaddr_un *addr)
{
	int rc;
	message vfs_m;
	cp_grant_id_t grant_id;

	grant_id = cpf_grant_direct(VFS_PROC_NR, (vir_bytes) addr->sun_path,
					UNIX_PATH_MAX, CPF_READ | CPF_WRITE);

	/* ask the VFS to verify the permissions */
	memset(&vfs_m, '\0', sizeof(message));

	vfs_m.m_type = PFS_REQ_CHECK_PERMS;
	vfs_m.USER_ENDPT = uds_fd_table[minor].owner;
	vfs_m.IO_GRANT = (char *) grant_id;
	vfs_m.COUNT = UNIX_PATH_MAX;

	rc = sendrec(VFS_PROC_NR, &vfs_m);
	cpf_revoke(grant_id);
	if (OK != rc) {
                printf("(uds) sendrec error... req_nr: %d err: %d\n",
			vfs_m.m_type, rc);

		return EIO;
	}

#if DEBUG == 1
	printf("(uds) VFS reply => %d\n", vfs_m.m_type);
	printf("(uds) Canonical Path => %s\n", addr->sun_path);
#endif

	return vfs_m.m_type; /* return reply code OK, ELOOP, etc. */
}

static filp_id_t verify_fd(endpoint_t ep, int fd)
{
	int rc;
	message vfs_m;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) verify_fd(%d,%d) call_count=%d\n", ep, fd,
							++call_count);
#endif

	memset(&vfs_m, '\0', sizeof(message));

	vfs_m.m_type = PFS_REQ_VERIFY_FD;
	vfs_m.USER_ENDPT = ep;
	vfs_m.COUNT = fd;

	rc = sendrec(VFS_PROC_NR, &vfs_m);
	if (OK != rc) {
                printf("(uds) sendrec error... req_nr: %d err: %d\n",
			vfs_m.m_type, rc);
		return NULL;
	}

#if DEBUG == 1
	printf("(uds) VFS reply => %d\n", vfs_m.m_type);
#endif

	return vfs_m.ADDRESS;
}

static int set_filp(filp_id_t sfilp)
{
	int rc;
	message vfs_m;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) set_filp(0x%x) call_count=%d\n", sfilp, ++call_count);
#endif

	memset(&vfs_m, '\0', sizeof(message));

	vfs_m.m_type = PFS_REQ_SET_FILP;
	vfs_m.ADDRESS = sfilp;

	rc = sendrec(VFS_PROC_NR, &vfs_m);
	if (OK != rc) {
                printf("(uds) sendrec error... req_nr: %d err: %d\n",
			vfs_m.m_type, rc);
		return EIO;
	}

#if DEBUG == 1
	printf("(uds) VFS reply => %d\n", vfs_m.m_type);
#endif
	return vfs_m.m_type; /* return reply code OK, ELOOP, etc. */
}

static int copy_filp(endpoint_t to_ep, filp_id_t cfilp)
{
	int rc;
	message vfs_m;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) copy_filp(%d, 0x%x) call_count=%d\n",to_ep, cfilp,
							++call_count);
#endif

	memset(&vfs_m, '\0', sizeof(message));

	vfs_m.m_type = PFS_REQ_COPY_FILP;
	vfs_m.USER_ENDPT = to_ep;
	vfs_m.ADDRESS = cfilp;

	rc = sendrec(VFS_PROC_NR, &vfs_m);
	if (OK != rc) {
                printf("(uds) sendrec error... req_nr: %d err: %d\n",
			vfs_m.m_type, rc);
		return EIO;
	}

#if DEBUG == 1
	printf("(uds) VFS reply => %d\n", vfs_m.m_type);
#endif
	return vfs_m.m_type;
}

static int put_filp(filp_id_t pfilp)
{
	int rc;
	message vfs_m;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) put_filp(0x%x) call_count=%d\n", pfilp, ++call_count);
#endif

	memset(&vfs_m, '\0', sizeof(message));

	vfs_m.m_type = PFS_REQ_PUT_FILP;
	vfs_m.ADDRESS = pfilp;

	rc = sendrec(VFS_PROC_NR, &vfs_m);
	if (OK != rc) {
                printf("(uds) sendrec error... req_nr: %d err: %d\n",
			vfs_m.m_type, rc);
		return EIO;
	}

#if DEBUG == 1
	printf("(uds) VFS reply => %d\n", vfs_m.m_type);
#endif
	return vfs_m.m_type; /* return reply code OK, ELOOP, etc. */
}

static int cancel_fd(endpoint_t ep, int fd)
{
	int rc;
	message vfs_m;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) cancel_fd(%d,%d) call_count=%d\n", ep, fd, ++call_count);
#endif

	memset(&vfs_m, '\0', sizeof(message));

	vfs_m.m_type = PFS_REQ_CANCEL_FD;
	vfs_m.USER_ENDPT = ep;
	vfs_m.COUNT = fd;

	rc = sendrec(VFS_PROC_NR, &vfs_m);
	if (OK != rc) {
                printf("(uds) sendrec error... req_nr: %d err: %d\n",
			vfs_m.m_type, rc);
		return EIO;
	}

#if DEBUG == 1
	printf("(uds) VFS reply => %d\n", vfs_m.m_type);
#endif
	return vfs_m.m_type; /* return reply code OK, ELOOP, etc. */
}

int perform_connection(message *dev_m_in, message *dev_m_out,
			struct sockaddr_un *addr, int minorx, int minory)
{
	/* there are several places were a connection is established. */
	/* accept(2), connect(2), uds_status(2), socketpair(2)        */
	/* This is a helper function to make sure it is done in the   */
	/* same way in each place with the same validation checks.    */

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] perform_connection() call_count=%d\n",
					uds_minor(dev_m_in), ++call_count);
#endif

	/* only connection oriented types are acceptable and only like
	 * types can connect to each other
	 */
	if ((uds_fd_table[minorx].type != SOCK_SEQPACKET &&
		uds_fd_table[minorx].type != SOCK_STREAM) ||
		uds_fd_table[minorx].type != uds_fd_table[minory].type) {

		/* sockets are not in a valid state */
		return EINVAL;
	}

	/* connect the pair of sockets */
	uds_fd_table[minorx].peer = minory;
	uds_fd_table[minory].peer = minorx;

	/* Set the address of both sockets */
	memcpy(&(uds_fd_table[minorx].addr), addr, sizeof(struct sockaddr_un));
	memcpy(&(uds_fd_table[minory].addr), addr, sizeof(struct sockaddr_un));

	return OK;
}


int do_accept(message *dev_m_in, message *dev_m_out)
{
	int minor;
	int minorparent; /* minor number of parent (server) */
	int minorpeer;
	int rc, i;
	struct sockaddr_un addr;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_accept() call_count=%d\n",
					uds_minor(dev_m_in), ++call_count);
#endif

	/* Somewhat weird logic is used in this function, so here's an
	 * overview... The minor number is the server's client socket
	 * (the socket to be returned by accept()). The data waiting
	 * for us in the IO Grant is the address that the server is
	 * listening on. This function uses the address to find the
	 * server's descriptor. From there we can perform the
	 * connection or suspend and wait for a connect().
	 */

	minor = uds_minor(dev_m_in);

	if (uds_fd_table[minor].type != -1) {
		/* this IOCTL must be called on a 'fresh' socket */
		return EINVAL;
	}

	/* Get the server's address */
	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &addr, sizeof(struct sockaddr_un));

	if (rc != OK) {
		return EIO;
	}

	/* locate server socket */
	rc = -1; /* to trap error */

	for (i = 0; i < NR_FDS; i++) {

		if (uds_fd_table[i].addr.sun_family == AF_UNIX &&
				!strncmp(addr.sun_path,
				uds_fd_table[i].addr.sun_path,
				UNIX_PATH_MAX) &&
				uds_fd_table[i].listening == 1) {

			rc = 0;
			break;
		}
	}

	if (rc == -1) {
		/* there is no server listening on addr. Maybe someone
		 * screwed up the ioctl()?
		 */
		return EINVAL;
	}

	minorparent = i; /* parent */

	/* we are the parent's child */
	uds_fd_table[minorparent].child = minor;

	/* the peer has the same type as the parent. we need to be that
	 * type too.
	 */
	uds_fd_table[minor].type = uds_fd_table[minorparent].type;

	/* locate peer to accept in the parent's backlog */
	minorpeer = -1; /* to trap error */
	for (i = 0; i < uds_fd_table[minorparent].backlog_size; i++) {
		if (uds_fd_table[minorparent].backlog[i] != -1) {
			minorpeer = uds_fd_table[minorparent].backlog[i];
			uds_fd_table[minorparent].backlog[i] = -1;
			rc = 0;
			break;
		}
	}

	if (minorpeer == -1) {

#if DEBUG == 1
		printf("(uds) [%d] {do_accept} suspend\n", minor);
#endif

		/* there are no peers in the backlog, suspend and wait
		 * for some to show up
		 */
		uds_fd_table[minor].suspended = UDS_SUSPENDED_ACCEPT;

		return SUSPEND;
	}

#if DEBUG == 1
	printf("(uds) [%d] connecting to %d -- parent is %d\n", minor,
						minorpeer, minorparent);
#endif

	rc = perform_connection(dev_m_in, dev_m_out, &addr, minor, minorpeer);
	if (rc != OK) {
#if DEBUG == 1
		printf("(uds) [%d] {do_accept} connection not performed\n",
								minor);
#endif
		return rc;
	}

	uds_fd_table[minorparent].child = -1;

	/* if peer is blocked on connect() revive peer */
	if (uds_fd_table[minorpeer].suspended) {
#if DEBUG == 1
		printf("(uds) [%d] {do_accept} revive %d\n", minor,
								minorpeer);
#endif
		uds_fd_table[minorpeer].ready_to_revive = 1;
		uds_unsuspend(dev_m_in->m_source, minorpeer);
	}

	return OK;
}

int do_connect(message *dev_m_in, message *dev_m_out)
{
	int minor;
	struct sockaddr_un addr;
	int rc, i, j;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_connect() call_count=%d\n", uds_minor(dev_m_in),
								++call_count);
#endif

	minor = uds_minor(dev_m_in);

	/* only connection oriented sockets can connect */
	if (uds_fd_table[minor].type != SOCK_STREAM &&
			uds_fd_table[minor].type != SOCK_SEQPACKET) {
		return EINVAL;
	}

	if (uds_fd_table[minor].peer != -1) {
		/* socket is already connected */
		return EISCONN;
	}

	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
				(vir_bytes) 0, (vir_bytes) &addr,
				sizeof(struct sockaddr_un));

	if (rc != OK) {
		return EIO;
	}

	rc = check_perms(minor, &addr);
	if (rc != OK) {
		/* permission denied, socket file doesn't exist, etc. */
		return rc;
	}

	/* look for a socket of the same type that is listening on the
	 * address we want to connect to
	 */
	for (i = 0; i < NR_FDS; i++) {

		if (uds_fd_table[minor].type == uds_fd_table[i].type &&
			uds_fd_table[i].listening &&
			uds_fd_table[i].addr.sun_family == AF_UNIX &&
			!strncmp(addr.sun_path, uds_fd_table[i].addr.sun_path,
			UNIX_PATH_MAX)) {

			if (uds_fd_table[i].child != -1) {

				/* the server is blocked on accept(2) --
				 * perform connection to the child
				 */

				rc = perform_connection(dev_m_in, dev_m_out,
					&addr, minor, uds_fd_table[i].child);

				if (rc == OK) {

					uds_fd_table[i].child = -1;

#if DEBUG == 1
			printf("(uds) [%d] {do_connect} revive %d\n", minor, i);
#endif

					/* wake the parent (server) */
					uds_fd_table[i].ready_to_revive = 1;
					uds_unsuspend(dev_m_in->m_source, i);
				}

				return rc;

			} else {

#if DEBUG == 1
				printf("(uds) [%d] adding to %d's backlog\n",
								minor, i);
#endif

				/* tell the server were waiting to be served */

				/* look for a free slot in the backlog */
				rc = -1; /* to trap error */
				for (j = 0; j < uds_fd_table[i].backlog_size;
					j++) {

					if (uds_fd_table[i].backlog[j] == -1) {

						uds_fd_table[i].backlog[j] =
							minor;

						rc = 0;
						break;
					}
				}

				if (rc == -1) {

					/* backlog is full */
					break;
				}

				/* see if the server is blocked on select() */
				if (uds_fd_table[i].selecting == 1) {

					/* if the server wants to know
					 * about data ready to read and
					 * it doesn't know about it
					 * already, then let the server
					 * know we have data for it.
					 */
					if ((uds_fd_table[i].sel_ops_in &
						SEL_RD) &&
						!(uds_fd_table[i].sel_ops_out &
						SEL_RD)) {

						uds_fd_table[i].sel_ops_out |=
							SEL_RD;
						uds_fd_table[i].status_updated
							= 1;

						uds_unsuspend(
						dev_m_in->m_source, i);
					}
				}

				/* we found our server */
				uds_fd_table[minor].peer = i;

				/* set the address */
				memcpy(&(uds_fd_table[minor].addr), &addr,
					sizeof(struct sockaddr_un));

				break;
			}
		}
	}

	if (uds_fd_table[minor].peer == -1) {
		/* could not find another open socket listening on the
		 * specified address with room in the backlog
		 */
		return ECONNREFUSED;
	}

#if DEBUG == 1
	printf("(uds) [%d] {do_connect} suspend\n", minor);
#endif

	/* suspend until the server side completes the connection with accept()
	 */

	uds_fd_table[minor].suspended = UDS_SUSPENDED_CONNECT;

	return SUSPEND;
}

int do_listen(message *dev_m_in, message *dev_m_out)
{
	int minor;
	int rc;
	int backlog_size;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_listen() call_count=%d\n", uds_minor(dev_m_in),
							++call_count);
#endif

	minor = uds_minor(dev_m_in);

	/* ensure the socket has a type and is bound */
	if (uds_fd_table[minor].type == -1 ||
		uds_fd_table[minor].addr.sun_family != AF_UNIX) {

		/* probably trying to call listen() before bind() */
		return EINVAL;
	}

	/* the two supported types for listen(2) are SOCK_STREAM and
	 * SOCK_SEQPACKET
	 */
	if (uds_fd_table[minor].type != SOCK_STREAM &&
			uds_fd_table[minor].type != SOCK_SEQPACKET) {

		/* probably trying to call listen() with a SOCK_DGRAM */
		return EOPNOTSUPP;
	}

	/* The POSIX standard doesn't say what to do if listen() has
	 * already been called. Well, there isn't an errno. we silently
	 * let it happen, but if listen() has already been called, we
	 * don't allow the backlog to shrink
	 */
	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &backlog_size, sizeof(int));

	if (rc != OK) {
		return EIO;
	}

	if (uds_fd_table[minor].listening == 0) {

		/* See if backlog_size is between 0 and UDS_SOMAXCONN */
		if (backlog_size >= 0 && backlog_size < UDS_SOMAXCONN) {

			/* use the user provided backlog_size */
			uds_fd_table[minor].backlog_size = backlog_size;

		} else {

			/* the user gave an invalid size, use
			 * UDS_SOMAXCONN instead
			 */
			uds_fd_table[minor].backlog_size = UDS_SOMAXCONN;
		}
	} else {

		/* See if the user is trying to expand the backlog_size */
		if (backlog_size > uds_fd_table[minor].backlog_size &&
			backlog_size < UDS_SOMAXCONN) {

			/* expand backlog_size */
			uds_fd_table[minor].backlog_size = backlog_size;
		}

		/* Don't let the user shrink the backlog_size (we might
		 * have clients waiting in those slots
		 */
	}

	/* perform listen(2) */
	uds_fd_table[minor].listening = 1;

	return OK;
}

int do_socket(message *dev_m_in, message *dev_m_out)
{
	int rc;
	int minor;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_socket() call_count=%d\n", uds_minor(dev_m_in),
							++call_count);
#endif

	minor = uds_minor(dev_m_in);

	/* see if this socket already has a type */
	if (uds_fd_table[minor].type != -1) {
		/* socket type can only be set once */
		return EINVAL;
	}

	/* get the requested type */
	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &(uds_fd_table[minor].type),
		sizeof(int));

	if (rc != OK) {

		/* something went wrong and we couldn't get the type */
		return EIO;
	}

	/* validate the type */
	switch (uds_fd_table[minor].type) {
		case SOCK_STREAM:
		case SOCK_DGRAM:
		case SOCK_SEQPACKET:

			/* the type is one of the 3 valid socket types */
			return OK;

		default:

			/* if the type isn't one of the 3 valid socket
			 * types, then it must be invalid.
			 */

			/* set the type back to '-1' (no type set) */
			uds_fd_table[minor].type = -1;

			return EINVAL;
	}
}

int do_bind(message *dev_m_in, message *dev_m_out)
{
	int minor;
	struct sockaddr_un addr;
	int rc, i;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_bind() call_count=%d\n", uds_minor(dev_m_in),
							++call_count);
#endif

	minor = uds_minor(dev_m_in);

	if ((uds_fd_table[minor].type == -1) ||
		(uds_fd_table[minor].addr.sun_family == AF_UNIX &&
		uds_fd_table[minor].type != SOCK_DGRAM)) {

		/* the type hasn't been set by do_socket() yet OR attempting
		 * to re-bind() a non-SOCK_DGRAM socket
		 */
		return EINVAL;
	}

	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &addr, sizeof(struct sockaddr_un));

	if (rc != OK) {
		return EIO;
	}

	/* do some basic sanity checks on the address */
	if (addr.sun_family != AF_UNIX) {

		/* bad family */
		return EAFNOSUPPORT;
	}

	if (addr.sun_path[0] == '\0') {

		/* bad address */
		return ENOENT;
	}

	rc = check_perms(minor, &addr);
	if (rc != OK) {
		/* permission denied, socket file doesn't exist, etc. */
		return rc;
	}

	/* make sure the address isn't already in use by another socket. */
	for (i = 0; i < NR_FDS; i++) {
		if ((uds_fd_table[i].addr.sun_family == AF_UNIX) &&
			!strncmp(addr.sun_path,
			uds_fd_table[i].addr.sun_path, UNIX_PATH_MAX)) {

			/* another socket is bound to this sun_path */
			return EADDRINUSE;
		}
	}

	/* looks good, perform the bind() */
	memcpy(&(uds_fd_table[minor].addr), &addr, sizeof(struct sockaddr_un));

	return OK;
}

int do_getsockname(message *dev_m_in, message *dev_m_out)
{
	int minor;
	int rc;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_getsockname() call_count=%d\n",
					uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);

	/* Unconditionally send the address we have assigned to this socket.
	 * The POSIX standard doesn't say what to do if the address
	 * hasn't been set. If the address isn't currently set, then
	 * the user will get NULL bytes. Note: libc depends on this
	 * behavior.
	 */
	rc = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &(uds_fd_table[minor].addr),
		sizeof(struct sockaddr_un));

	return rc ? EIO : OK;
}

int do_getpeername(message *dev_m_in, message *dev_m_out)
{
	int minor;
	int rc;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_getpeername() call_count=%d\n",
				uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);

	/* check that the socket is connected with a valid peer */
	if (uds_fd_table[minor].peer != -1) {
		int peer_minor;

		peer_minor = uds_fd_table[minor].peer;

		/* copy the address from the peer */
		rc = sys_safecopyto(VFS_PROC_NR,
			(cp_grant_id_t) dev_m_in->IO_GRANT, (vir_bytes) 0,
			(vir_bytes) &(uds_fd_table[peer_minor].addr),
			sizeof(struct sockaddr_un));

		return rc ? EIO : OK;
	} else {
		if (uds_fd_table[minor].err == ECONNRESET) {
			uds_fd_table[minor].err = 0;

			return ECONNRESET;
		} else {
			return ENOTCONN;
		}
	}
}

int do_shutdown(message *dev_m_in, message *dev_m_out)
{
	int minor;
	int rc, how;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_shutdown() call_count=%d\n",
					uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);

	if (uds_fd_table[minor].type != SOCK_STREAM &&
			uds_fd_table[minor].type != SOCK_SEQPACKET) {

		/* socket must be a connection oriented socket */
		return EINVAL;
	}

	if (uds_fd_table[minor].peer == -1) {
		/* shutdown(2) is only valid for connected sockets */
		if (uds_fd_table[minor].err == ECONNRESET) {
			return ECONNRESET;
		} else {
			return ENOTCONN;
		}
	}

	/* get the 'how' parameter from the process */
	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
			(vir_bytes) 0, (vir_bytes) &how, sizeof(int));

	if (rc != OK) {
		return EIO;
	}

	switch (how) {
		case SHUT_RD:
			/* take away read permission */
			uds_fd_table[minor].mode =
				uds_fd_table[minor].mode ^ S_IRUSR;
			break;

		case SHUT_WR:
			/* take away write permission */
			uds_fd_table[minor].mode =
				uds_fd_table[minor].mode ^ S_IWUSR;
			break;

		case SHUT_RDWR:
			/* completely shutdown */
			uds_fd_table[minor].mode = 0;
			break;

		default:
			/* the 'how' parameter is invalid */
			return EINVAL;
	}

	return OK;
}

int do_socketpair_old(message *dev_m_in, message *dev_m_out)
{
	int rc;
	short minorin;
	int minorx, minory;
	struct sockaddr_un addr;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_socketpair() call_count=%d\n",
				uds_minor(dev_m_in), ++call_count);
#endif

	/* first ioctl param is the first socket */
	minorx = uds_minor_old(dev_m_in);

	/* third ioctl param is the minor number of the second socket */
	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
			(vir_bytes) 0, (vir_bytes) &minorin, sizeof(short));

	if (rc != OK) {
		return EIO;
	}

	minory = minor(minorin);

#if DEBUG == 1
	printf("socketpair() %d - %d\n", minorx, minory);
#endif

	/* security check - both sockets must have the same endpoint (owner) */
	if (uds_fd_table[minorx].owner != uds_fd_table[minory].owner) {

		/* we won't allow you to magically connect your socket to
		 * someone elses socket
		 */
		return EPERM;
	}

	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = 'X';
	addr.sun_path[1] = '\0';

	uds_fd_table[minorx].syscall_done = 1;
	return perform_connection(dev_m_in, dev_m_out, &addr, minorx, minory);
}

int do_socketpair(message *dev_m_in, message *dev_m_out)
{
	int rc;
	dev_t minorin;
	int minorx, minory;
	struct sockaddr_un addr;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_socketpair() call_count=%d\n",
				uds_minor(dev_m_in), ++call_count);
#endif

	/* first ioctl param is the first socket */
	minorx = uds_minor(dev_m_in);

	/* third ioctl param is the minor number of the second socket */
	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
			(vir_bytes) 0, (vir_bytes) &minorin, sizeof(dev_t));

	if (rc != OK) {
		return EIO;
	}

	minory = minor(minorin);

#if DEBUG == 1
	printf("socketpair() %d - %d\n", minorx, minory);
#endif

	/* security check - both sockets must have the same endpoint (owner) */
	if (uds_fd_table[minorx].owner != uds_fd_table[minory].owner) {

		/* we won't allow you to magically connect your socket to
		 * someone elses socket
		 */
		return EPERM;
	}

	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = 'X';
	addr.sun_path[1] = '\0';

	uds_fd_table[minorx].syscall_done = 1;
	return perform_connection(dev_m_in, dev_m_out, &addr, minorx, minory);
}

int do_getsockopt_sotype(message *dev_m_in, message *dev_m_out)
{
	int minor;
	int rc;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_getsockopt_sotype() call_count=%d\n",
				uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);

	if (uds_fd_table[minor].type == -1) {

		/* the type hasn't been set yet. instead of returning an
		 * invalid type, we fail with EINVAL
		 */
		return EINVAL;
	}

	rc = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &(uds_fd_table[minor].type),
		sizeof(int));

	return rc ? EIO : OK;
}

int do_getsockopt_peercred(message *dev_m_in, message *dev_m_out)
{
	int minor;
	int peer_minor;
	int rc;
	struct ucred cred;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_getsockopt_peercred() call_count=%d\n",
					uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);

	if (uds_fd_table[minor].peer == -1) {

		if (uds_fd_table[minor].err == ECONNRESET) {
			uds_fd_table[minor].err = 0;

			return ECONNRESET;
		} else {
			return ENOTCONN;
		}
	}

	peer_minor = uds_fd_table[minor].peer;

	/* obtain the peer's credentials */
	rc = getnucred(uds_fd_table[peer_minor].owner, &cred);
	if (rc == -1) {
		/* likely error: invalid endpoint / proc doesn't exist */
		return errno;
	}

	rc = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &cred, sizeof(struct ucred));

	return rc ? EIO : OK;
}

int do_getsockopt_peercred_old(message *dev_m_in, message *dev_m_out)
{
	int minor;
	int peer_minor;
	int rc;
	struct ucred cred;
	struct ucred_old cred_old;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_getsockopt_peercred() call_count=%d\n",
					uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);

	if (uds_fd_table[minor].peer == -1) {

		if (uds_fd_table[minor].err == ECONNRESET) {
			uds_fd_table[minor].err = 0;

			return ECONNRESET;
		} else {
			return ENOTCONN;
		}
	}

	peer_minor = uds_fd_table[minor].peer;

	/* obtain the peer's credentials */
	rc = getnucred(uds_fd_table[peer_minor].owner, &cred);
	if (rc == -1) {
		/* likely error: invalid endpoint / proc doesn't exist */
		return errno;
	}

	/* copy to old structure */
	cred_old.pid = cred.pid;
	cred_old.uid = (short) cred.uid;
	cred_old.gid = (char) cred.gid;

	rc = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &cred_old, sizeof(struct ucred_old));

	return rc ? EIO : OK;
}

int do_getsockopt_sndbuf(message *dev_m_in, message *dev_m_out)
{
	int rc;
	size_t sndbuf = PIPE_BUF;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_getsockopt_sndbuf() call_count=%d\n",
				uds_minor(dev_m_in), ++call_count);
#endif

	rc = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &(sndbuf), sizeof(size_t));

	return rc ? EIO : OK;
}

int do_setsockopt_sndbuf(message *dev_m_in, message *dev_m_out)
{
	int rc;
	size_t sndbuf;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_setsockopt_rcvbuf() call_count=%d\n",
				uds_minor(dev_m_in), ++call_count);
#endif

	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
				(vir_bytes) 0, (vir_bytes) &sndbuf,
				sizeof(size_t));

	if (rc != OK) {
		return EIO;
	}

	if (sndbuf > PIPE_BUF) {
		/* The send buffer is limited to 32K at the moment. */
		return ENOSYS;
	}

	/* There is no way to reduce the send buffer, do we have to
	 * let this call fail for smaller buffers?
	 */
	return OK;
}

int do_getsockopt_rcvbuf(message *dev_m_in, message *dev_m_out)
{
	int rc;
	size_t rcvbuf = PIPE_BUF;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_getsockopt_rcvbuf() call_count=%d\n",
				uds_minor(dev_m_in), ++call_count);
#endif

	rc = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &(rcvbuf), sizeof(size_t));

	return rc ? EIO : OK;
}

int do_setsockopt_rcvbuf(message *dev_m_in, message *dev_m_out)
{
	int rc;
	size_t rcvbuf;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_setsockopt_rcvbuf() call_count=%d\n",
				uds_minor(dev_m_in), ++call_count);
#endif

	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
				(vir_bytes) 0, (vir_bytes) &rcvbuf,
				sizeof(size_t));

	if (rc != OK) {
		return EIO;
	}

	if (rcvbuf > PIPE_BUF) {
		/* The send buffer is limited to 32K at the moment. */
		return ENOSYS;
	}

	/* There is no way to reduce the send buffer, do we have to
	 * let this call fail for smaller buffers?
	 */
	return OK;
}


int do_sendto(message *dev_m_in, message *dev_m_out)
{
	int minor;
	int rc;
	struct sockaddr_un addr;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_sendto() call_count=%d\n", uds_minor(dev_m_in),
							++call_count);
#endif

	minor = uds_minor(dev_m_in);

	if (uds_fd_table[minor].type != SOCK_DGRAM) {
		/* This IOCTL is only for SOCK_DGRAM sockets */
		return EINVAL;
	}

	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &addr, sizeof(struct sockaddr_un));

	if (rc != OK) {
		return EIO;
	}

	/* do some basic sanity checks on the address */
	if (addr.sun_family != AF_UNIX || addr.sun_path[0] == '\0') {
		/* bad address */
		return EINVAL;
	}

	rc = check_perms(minor, &addr);
	if (rc != OK) {
		return rc;
	}

	memcpy(&(uds_fd_table[minor].target), &addr,
					sizeof(struct sockaddr_un));

	return OK;
}

int do_recvfrom(message *dev_m_in, message *dev_m_out)
{
	int minor;
	int rc;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_recvfrom() call_count=%d\n",
					uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);

	rc = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &(uds_fd_table[minor].source),
		sizeof(struct sockaddr_un));

	return rc ? EIO : OK;
}

int msg_control_read(struct msg_control *msg_ctrl, struct ancillary *data,
							int minor)
{
	int rc;
	struct msghdr msghdr;
	struct cmsghdr *cmsg = NULL;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] msg_control_read() call_count=%d\n", minor,
							++call_count);
#endif

	data->nfiledes = 0;

	memset(&msghdr, '\0', sizeof(struct msghdr));
	msghdr.msg_control = msg_ctrl->msg_control;
	msghdr.msg_controllen = msg_ctrl->msg_controllen;

	for(cmsg = CMSG_FIRSTHDR(&msghdr); cmsg != NULL;
					cmsg = CMSG_NXTHDR(&msghdr, cmsg)) {

		if (cmsg->cmsg_level == SOL_SOCKET &&
					cmsg->cmsg_type == SCM_RIGHTS) {

			int i;
			int nfds =
				MIN((cmsg->cmsg_len-CMSG_LEN(0))/sizeof(int),
								OPEN_MAX);

			for (i = 0; i < nfds; i++) {
				if (data->nfiledes == OPEN_MAX) {
					return EOVERFLOW;
				}

				data->fds[data->nfiledes] =
					((int *) CMSG_DATA(cmsg))[i];
#if DEBUG == 1
				printf("(uds) [%d] fd[%d]=%d\n", minor,
				data->nfiledes, data->fds[data->nfiledes]);
#endif
				data->nfiledes++;
			}
		}
	}

	/* obtain this socket's credentials */
	rc = getnucred(uds_fd_table[minor].owner, &(data->cred));
	if (rc == -1) {
		return errno;
	}
#if DEBUG == 1
	printf("(uds) [%d] cred={%d,%d,%d}\n", minor,
		data->cred.pid, data->cred.uid,
		data->cred.gid);
#endif
	return OK;
}

static int send_fds(int minor, struct ancillary *data)
{
	int rc, i, j;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] send_fds() call_count=%d\n", minor, ++call_count);
#endif

	/* verify the file descriptors and get their filps. */
	for (i = 0; i < data->nfiledes; i++) {
		data->filps[i] = verify_fd(uds_fd_table[minor].owner,
						data->fds[i]);

		if (data->filps[i] == NULL) {
			return EINVAL;
		}
	}

	/* set them as in-flight */
	for (i = 0; i < data->nfiledes; i++) {
		rc = set_filp(data->filps[i]);
		if (rc != OK) {
			/* revert set_filp() calls */
			for (j = i; j >= 0; j--) {
				put_filp(data->filps[j]);
			}
			return rc;
		}
	}

	return OK;
}

int clear_fds(int minor, struct ancillary *data)
{
/* This function calls put_filp() for all of the FDs in data.
 * This is used when a Unix Domain Socket is closed and there
 * exists references to file descriptors that haven't been received
 * with recvmsg().
 */
	int i;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] recv_fds() call_count=%d\n", minor,
							++call_count);
#endif

	for (i = 0; i < data->nfiledes; i++) {
		put_filp(data->filps[i]);
#if DEBUG == 1
		printf("(uds) clear_fds() => %d\n", data->fds[i]);
#endif
		data->fds[i] = -1;
		data->filps[i] = NULL;
	}

	data->nfiledes = 0;

	return OK;
}

static int recv_fds(int minor, struct ancillary *data,
					struct msg_control *msg_ctrl)
{
	int rc, i, j;
	struct msghdr msghdr;
	struct cmsghdr *cmsg;
	endpoint_t to_ep;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] recv_fds() call_count=%d\n", minor,
							++call_count);
#endif

	msghdr.msg_control = msg_ctrl->msg_control;
	msghdr.msg_controllen = msg_ctrl->msg_controllen;

	cmsg = CMSG_FIRSTHDR(&msghdr);
	cmsg->cmsg_len = CMSG_LEN(sizeof(int) * data->nfiledes);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;

	to_ep = uds_fd_table[minor].owner;

	/* copy to the target endpoint */
	for (i = 0; i < data->nfiledes; i++) {
		rc = copy_filp(to_ep, data->filps[i]);
		if (rc < 0) {
			/* revert set_filp() calls */
			for (j = 0; j < data->nfiledes; j++) {
				put_filp(data->filps[j]);
			}
			/* revert copy_filp() calls */
			for (j = i; j >= 0; j--) {
				cancel_fd(to_ep, data->fds[j]);
			}
			return rc;
		}
		data->fds[i] = rc; /* data->fds[i] now has the new FD */
	}

	for (i = 0; i < data->nfiledes; i++) {
		put_filp(data->filps[i]);
#if DEBUG == 1
		printf("(uds) recv_fds() => %d\n", data->fds[i]);
#endif
		((int *)CMSG_DATA(cmsg))[i] = data->fds[i];
		data->fds[i] = -1;
		data->filps[i] = NULL;
	}

	data->nfiledes = 0;

	return OK;
}

static int recv_cred(int minor, struct ancillary *data,
					struct msg_control *msg_ctrl)
{
	struct msghdr msghdr;
	struct cmsghdr *cmsg;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] recv_cred() call_count=%d\n", minor,
							++call_count);
#endif

	msghdr.msg_control = msg_ctrl->msg_control;
	msghdr.msg_controllen = msg_ctrl->msg_controllen;

	cmsg = CMSG_FIRSTHDR(&msghdr);
	if (cmsg->cmsg_len > 0) {
		cmsg = CMSG_NXTHDR(&msghdr, cmsg);
	}

	cmsg->cmsg_len = CMSG_LEN(sizeof(struct ucred));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_CREDENTIALS;
	memcpy(CMSG_DATA(cmsg), &(data->cred), sizeof(struct ucred));

	return OK;
}

int do_sendmsg(message *dev_m_in, message *dev_m_out)
{
	int minor, peer, rc, i;
	struct msg_control msg_ctrl;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_sendmsg() call_count=%d\n",
					uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);

	memset(&msg_ctrl, '\0', sizeof(struct msg_control));

	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
					(vir_bytes) 0, (vir_bytes) &msg_ctrl,
					sizeof(struct msg_control));

	if (rc != OK) {
		return EIO;
	}

	/* locate peer */
	peer = -1;
	if (uds_fd_table[minor].type == SOCK_DGRAM) {
		if (uds_fd_table[minor].target.sun_path[0] == '\0' ||
			uds_fd_table[minor].target.sun_family != AF_UNIX) {

			return EDESTADDRREQ;
		}

		for (i = 0; i < NR_FDS; i++) {

			/* look for a SOCK_DGRAM socket that is bound on
			 * the target address
			 */
			if (uds_fd_table[i].type == SOCK_DGRAM &&
				uds_fd_table[i].addr.sun_family == AF_UNIX &&
				!strncmp(uds_fd_table[minor].target.sun_path,
				uds_fd_table[i].addr.sun_path, UNIX_PATH_MAX)){

				peer = i;
				break;
			}
		}

		if (peer == -1) {
			return ENOENT;
		}
	} else {
		peer = uds_fd_table[minor].peer;
		if (peer == -1) {
			return ENOTCONN;
		}
	}

#if DEBUG == 1
	printf("(uds) [%d] sendmsg() -- peer=%d\n", minor, peer);
#endif
	/* note: it's possible that there is already some file
	 * descriptors in ancillary_data if the peer didn't call
	 * recvmsg() yet. That's okay. The receiver will
	 * get the current file descriptors plus the new ones.
	 */
	rc = msg_control_read(&msg_ctrl, &uds_fd_table[peer].ancillary_data,
								minor);
	if (rc != OK) {
		return rc;
	}

	return send_fds(minor, &uds_fd_table[peer].ancillary_data);
}

int do_recvmsg(message *dev_m_in, message *dev_m_out)
{
	int minor;
	int rc;
	struct msg_control msg_ctrl;
	socklen_t controllen_avail = 0;
	socklen_t controllen_needed = 0;
	socklen_t controllen_desired = 0;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_sendmsg() call_count=%d\n",
					uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);


#if DEBUG == 1
	printf("(uds) [%d] CREDENTIALS {pid:%d,uid:%d,gid:%d}\n", minor,
				uds_fd_table[minor].ancillary_data.cred.pid,
				uds_fd_table[minor].ancillary_data.cred.uid,
				uds_fd_table[minor].ancillary_data.cred.gid);
#endif

	memset(&msg_ctrl, '\0', sizeof(struct msg_control));

	/* get the msg_control from the user, it will include the
	 * amount of space the user has allocated for control data.
	 */
	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
					(vir_bytes) 0, (vir_bytes) &msg_ctrl,
					sizeof(struct msg_control));

	if (rc != OK) {
		return EIO;
	}

	controllen_avail = MIN(msg_ctrl.msg_controllen, MSG_CONTROL_MAX);

	if (uds_fd_table[minor].ancillary_data.nfiledes > 0) {
		controllen_needed = CMSG_LEN(sizeof(int) *
				(uds_fd_table[minor].ancillary_data.nfiledes));
	}

	/* if there is room we also include credentials */
	controllen_desired = controllen_needed +
				CMSG_LEN(sizeof(struct ucred));

	if (controllen_needed > controllen_avail) {
		return EOVERFLOW;
	}

	rc = recv_fds(minor, &uds_fd_table[minor].ancillary_data, &msg_ctrl);
	if (rc != OK) {
		return rc;
	}

	if (controllen_desired <= controllen_avail) {
		rc = recv_cred(minor, &uds_fd_table[minor].ancillary_data,
								&msg_ctrl);
		if (rc != OK) {
			return rc;
		}
	}

	/* send the user the control data */
	rc = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &msg_ctrl,
		sizeof(struct msg_control));

	return rc ? EIO : OK;
}
