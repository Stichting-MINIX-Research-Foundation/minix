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
 *   perform_connection:     performs the connection of two descriptors.
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
PUBLIC void uds_init(void)
{
	/*
	 * Setting everything to NULL implicitly sets the 
	 * state to UDS_FREE.
	 */
	memset(uds_fd_table, '\0', sizeof(uds_fd_t) * NR_FDS);
}

PUBLIC int perform_connection(message *dev_m_in, message *dev_m_out, 
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
		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

		return EINVAL;
	}

	/* connect the pair of sockets */
	uds_fd_table[minorx].peer = minory;
	uds_fd_table[minory].peer = minorx;

	/* Set the address of both sockets */
	memcpy(&(uds_fd_table[minorx].addr), addr, sizeof(struct sockaddr_un));
	memcpy(&(uds_fd_table[minory].addr), addr, sizeof(struct sockaddr_un));

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}


PUBLIC int do_accept(message *dev_m_in, message *dev_m_out)
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
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
			 (cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

		return EINVAL;
	}

	/* Get the server's address */
	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &addr, sizeof(struct sockaddr_un),
		D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	/* locate server socket */
	rc = -1; /* to trap error */

	for (i = 0; i < NR_FDS; i++) {

		if (uds_fd_table[i].addr.sun_family == AF_UNIX &&
				!strncmp(addr.sun_path, 
				uds_fd_table[i].addr.sun_path,
				UNIX_PATH_MAX)) {

			rc = 0;
			break;
		}
	}

	if (rc == -1) {

		/* there is no server listening on addr. Maybe someone 
		 * screwed up the ioctl()?
		 */

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT, 
				(cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

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

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, SUSPEND);

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
		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, errno);

		return errno;
	}

	uds_fd_table[minorparent].child = -1;

	/* if peer is blocked on connect() revive peer */
	if (uds_fd_table[minorpeer].suspended) {
#if DEBUG == 1
		printf("(uds) [%d] {do_accept} revive %d", minor, minorpeer);
#endif
		uds_fd_table[minorpeer].ready_to_revive = 1;
		notify(dev_m_in->m_source);
	}

	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT, 
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}

PUBLIC int do_connect(message *dev_m_in, message *dev_m_out)
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

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

		return EINVAL;
	}

	if (uds_fd_table[minor].peer != -1) {

		/* socket is already connected */
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
			(cp_grant_id_t) dev_m_in->IO_GRANT, EISCONN);

		return EISCONN;
	}

	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
				(vir_bytes) 0, (vir_bytes) &addr, 
				sizeof(struct sockaddr_un), D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
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
			printf("(uds) [%d] {do_connect} revive %d", minor, i);
#endif

					/* wake the parent (server) */
					uds_fd_table[i].ready_to_revive = 1;
					notify(dev_m_in->m_source);

					uds_fd_table[minor].syscall_done = 1;

					uds_set_reply(dev_m_out, TASK_REPLY,
						dev_m_in->IO_ENDPT,
					(cp_grant_id_t) dev_m_in->IO_GRANT,
						OK);

					return OK;
				} else {

					uds_fd_table[minor].syscall_done = 1;

					uds_set_reply(dev_m_out, TASK_REPLY,
						dev_m_in->IO_ENDPT,
					(cp_grant_id_t) dev_m_in->IO_GRANT,
						rc);

					return rc;
				}

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

						notify(
						uds_fd_table[i].select_proc
						);
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
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
			(cp_grant_id_t) dev_m_in->IO_GRANT, ECONNREFUSED);

		return ECONNREFUSED;
	}

#if DEBUG == 1
	printf("(uds) [%d] {do_connect} suspend", minor);
#endif

	/* suspend until the server side completes the connection with accept()
	 */

	uds_fd_table[minor].suspended = UDS_SUSPENDED_CONNECT;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
			(cp_grant_id_t) dev_m_in->IO_GRANT, SUSPEND);

	return SUSPEND;
}

PUBLIC int do_listen(message *dev_m_in, message *dev_m_out)
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
		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
			(cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

		return EINVAL;
	}

	/* the two supported types for listen(2) are SOCK_STREAM and 
	 * SOCK_SEQPACKET
	 */
	if (uds_fd_table[minor].type != SOCK_STREAM &&
			uds_fd_table[minor].type != SOCK_SEQPACKET) {

		/* probably trying to call listen() with a SOCK_DGRAM */
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT, 
			(cp_grant_id_t) dev_m_in->IO_GRANT, EOPNOTSUPP);

		return EOPNOTSUPP;
	}

	/* The POSIX standard doesn't say what to do if listen() has 
	 * already been called. Well, there isn't an errno. we silently 
	 * let it happen, but if listen() has already been called, we 
	 * don't allow the backlog to shrink
	 */
	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &backlog_size, sizeof(int), 
		D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
			(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	if (uds_fd_table[minor].listening == 0) {

		/* See if backlog_size is between 0 and UDS_SOMAXCONN */
		if (backlog_size >= 0 || backlog_size < UDS_SOMAXCONN) {

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

	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
			(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}

PUBLIC int do_socket(message *dev_m_in, message *dev_m_out)
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
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

		return EINVAL;
	}

	/* get the requested type */
	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &(uds_fd_table[minor].type),
		sizeof(int), D);

	if (rc != OK) {

		/* something went wrong and we couldn't get the type */
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	/* validate the type */
	switch (uds_fd_table[minor].type) {
		case SOCK_STREAM:
		case SOCK_DGRAM:
		case SOCK_SEQPACKET:

			/* the type is one of the 3 valid socket types */
			uds_fd_table[minor].syscall_done = 1;

			uds_set_reply(dev_m_out, TASK_REPLY,
				dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, 
				OK);

			return OK;

		default:

			/* if the type isn't one of the 3 valid socket 
			 * types, then it must be invalid.
			 */

			/* set the type back to '-1' (no type set) */
			uds_fd_table[minor].type = -1;

			uds_fd_table[minor].syscall_done = 1;

			uds_set_reply(dev_m_out, TASK_REPLY,
				dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT,
				EINVAL);

			return EINVAL;
	}
}

PUBLIC int do_bind(message *dev_m_in, message *dev_m_out)
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
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

		return EINVAL;
	}

	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &addr, sizeof(struct sockaddr_un), 
		D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
			(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	/* do some basic sanity checks on the address */
	if (addr.sun_family != AF_UNIX || addr.sun_path[0] == '\0') {

		/* bad address */
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

		return EINVAL;
	}

	/* make sure the address isn't already in use by another socket. */
	for (i = 0; i < NR_FDS; i++) {
		if ((uds_fd_table[i].addr.sun_family == AF_UNIX) &&
			!strncmp(addr.sun_path, 
			uds_fd_table[i].addr.sun_path, UNIX_PATH_MAX)) {

			/* another socket is bound to this sun_path */
			uds_fd_table[minor].syscall_done = 1;

			uds_set_reply(dev_m_out, TASK_REPLY,
				dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT,
				EADDRINUSE);

			return EADDRINUSE;
		}
	}

	/* looks good, perform the bind() */
	memcpy(&(uds_fd_table[minor].addr), &addr, sizeof(struct sockaddr_un));

	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
			(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}

PUBLIC int do_getsockname(message *dev_m_in, message *dev_m_out)
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
		sizeof(struct sockaddr_un), D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}

PUBLIC int do_getpeername(message *dev_m_in, message *dev_m_out)
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
			sizeof(struct sockaddr_un), D);

		if (rc != OK) {

			uds_fd_table[minor].syscall_done = 1;

			uds_set_reply(dev_m_out, TASK_REPLY,
				dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

			return EIO;
		}

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

		return OK;
	} else {

		int err;

		if (uds_fd_table[minor].err == ECONNRESET) {
			err = ECONNRESET;
			uds_fd_table[minor].err = 0;
		} else {
			err = ENOTCONN;
		}

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, err);

		return err;
	}
}

PUBLIC int do_shutdown(message *dev_m_in, message *dev_m_out)
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
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

		return EINVAL;
	}

	if (uds_fd_table[minor].peer == -1) {

		int err;

		if (uds_fd_table[minor].err == ECONNRESET) {
			err = ECONNRESET;
		} else {
			err = ENOTCONN;
		}

		/* shutdown(2) is only valid for connected sockets */
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, err);

		return err;
	}

	/* get the 'how' parameter from the process */
	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
			(vir_bytes) 0, (vir_bytes) &how, sizeof(int), D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

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
			uds_fd_table[minor].syscall_done = 1;

			uds_set_reply(dev_m_out, TASK_REPLY,
				dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

			return EINVAL;
	}


	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}

PUBLIC int do_socketpair(message *dev_m_in, message *dev_m_out)
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
			(vir_bytes) 0, (vir_bytes) &minorin, sizeof(dev_t), D);

	if (rc != OK) {

		uds_fd_table[minorx].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	minory = (minor(minorin) & BYTE);

	/* security check - both sockets must have the same endpoint (owner) */
	if (uds_fd_table[minorx].endpoint != uds_fd_table[minory].endpoint) {

		/* we won't allow you to magically connect your socket to
		 * someone elses socket
		 */
		uds_fd_table[minorx].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EPERM);

		return EPERM;
	}

	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = 'X';
	addr.sun_path[1] = '\0';

	uds_fd_table[minorx].syscall_done = 1;
	return perform_connection(dev_m_in, dev_m_out, &addr, minorx, minory);
}

PUBLIC int do_getsockopt_sotype(message *dev_m_in, message *dev_m_out)
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
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

		return EINVAL;
	}

	rc = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &(uds_fd_table[minor].type), 
		sizeof(int), D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}

PUBLIC int do_getsockopt_peercred(message *dev_m_in, message *dev_m_out)
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

		int err;

		if (uds_fd_table[minor].err == ECONNRESET) {
			err = ECONNRESET;
			uds_fd_table[minor].err = 0;
		} else {
			err = ENOTCONN;
		}

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, err);

		return err;
	}

	peer_minor = uds_fd_table[minor].peer;

	/* obtain the peer's credentials */
	rc = getnucred(uds_fd_table[peer_minor].owner, &cred);
	if (rc == -1) {

		/* likely error: invalid endpoint / proc doesn't exist */
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, errno);

		return errno;
	}

	rc = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &cred, sizeof(struct ucred), D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}

int do_getsockopt_sndbuf(message *dev_m_in, message *dev_m_out) 
{
	int minor;
	int rc;
	size_t sndbuf = PIPE_BUF;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_getsockopt_sndbuf() call_count=%d\n",
				uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);

	rc = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &(sndbuf), 
		sizeof(size_t), D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}

int do_setsockopt_sndbuf(message *dev_m_in, message *dev_m_out) 
{
	int minor;
	int rc;
	size_t sndbuf;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_setsockopt_rcvbuf() call_count=%d\n",
				uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);


	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
				(vir_bytes) 0, (vir_bytes) &sndbuf, 
				sizeof(size_t), D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	if (sndbuf > PIPE_BUF) {

		/* The send buffer is limited to 32K at the moment. */

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, ENOSYS);

		return ENOSYS;
	}

	/* There is no way to reduce the send buffer, do we have to
	 * let this call fail for smaller buffers?
	 */
	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}

int do_getsockopt_rcvbuf(message *dev_m_in, message *dev_m_out) 
{
	int minor;
	int rc;
	size_t rcvbuf = PIPE_BUF;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_getsockopt_rcvbuf() call_count=%d\n",
				uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);

	rc = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &(rcvbuf), 
		sizeof(size_t), D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}

int do_setsockopt_rcvbuf(message *dev_m_in, message *dev_m_out)
{
	int minor;
	int rc;
	size_t rcvbuf;

#if DEBUG == 1
	static int call_count = 0;
	printf("(uds) [%d] do_setsockopt_rcvbuf() call_count=%d\n",
				uds_minor(dev_m_in), ++call_count);
#endif

	minor = uds_minor(dev_m_in);


	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
				(vir_bytes) 0, (vir_bytes) &rcvbuf, 
				sizeof(size_t), D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	if (rcvbuf > PIPE_BUF) {

		/* The send buffer is limited to 32K at the moment. */

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, ENOSYS);

		return ENOSYS;
	}

	/* There is no way to reduce the send buffer, do we have to
	 * let this call fail for smaller buffers?
	 */
	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}


PUBLIC int do_sendto(message *dev_m_in, message *dev_m_out)
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
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

		return EINVAL;
	}

	rc = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) dev_m_in->IO_GRANT,
		(vir_bytes) 0, (vir_bytes) &addr, sizeof(struct sockaddr_un),
		D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	/* do some basic sanity checks on the address */
	if (addr.sun_family != AF_UNIX || addr.sun_path[0] == '\0') {

		/* bad address */
		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EINVAL);

		return EINVAL;
	}

	memcpy(&(uds_fd_table[minor].target), &addr,
					sizeof(struct sockaddr_un));

	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}

PUBLIC int do_recvfrom(message *dev_m_in, message *dev_m_out)
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
		sizeof(struct sockaddr_un), D);

	if (rc != OK) {

		uds_fd_table[minor].syscall_done = 1;

		uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, EIO);

		return EIO;
	}

	uds_fd_table[minor].syscall_done = 1;

	uds_set_reply(dev_m_out, TASK_REPLY, dev_m_in->IO_ENDPT,
				(cp_grant_id_t) dev_m_in->IO_GRANT, OK);

	return OK;
}
