/*
 * IMPORTANT NOTICE: THIS FILE CONTAINS STUBS ONLY RIGHT NOW, TO ENABLE A
 * SEAMLESS TRANSITION TO THE NEW API FOR PROGRAMS STATICALLY LINKED TO LIBC!
 *
 * This file implements the upper socket layer of VFS: the BSD socket system
 * calls, and any associated file descriptor, file pointer, vnode, and file
 * system processing.  In most cases, this layer will call into the lower
 * socket layer in order to send the request to a socket driver.  Generic file
 * calls (e.g., read, write, ioctl, and select) are not implemented here, and
 * will directly call into the lower socket layer as well.
 *
 * The following table shows the system call numbers implemented in this file,
 * along with their request and reply message types.  Each request layout
 * message type is prefixed with "m_lc_vfs_".  Each reply layout message type
 * is prefixed with "m_vfs_lc_".  For requests without a specific reply layout,
 * only the "m_type" message field is used in the reply message.
 *
 * Type			Request layout		Reply layout
 * ----			--------------		------------
 * VFS_SOCKET		socket
 * VFS_SOCKETPAIR	socket			fdpair
 * VFS_BIND		sockaddr
 * VFS_CONNECT		sockaddr
 * VFS_LISTEN		listen
 * VFS_ACCEPT		sockaddr		socklen
 * VFS_SENDTO		sendrecv
 * VFS_RECVFROM		sendrecv		socklen
 * VFS_SENDMSG		sockmsg
 * VFS_RECVMSG		sockmsg
 * VFS_SETSOCKOPT	sockopt
 * VFS_GETSOCKOPT	sockopt			socklen
 * VFS_GETSOCKNAME	sockaddr		socklen
 * VFS_GETPEERNAME	sockaddr		socklen
 * VFS_SHUTDOWN		shutdown
 */

#include "fs.h"

#include <sys/socket.h>

/*
 * Create a socket.
 */
int
do_socket(void)
{

	return EAFNOSUPPORT;
}

/*
 * Create a pair of connected sockets.
 */
int
do_socketpair(void)
{

	return EAFNOSUPPORT;
}

/*
 * Bind a socket to a local address.
 */
int
do_bind(void)
{

	return ENOTSOCK;
}

/*
 * Connect a socket to a remote address.
 */
int
do_connect(void)
{

	return ENOTSOCK;
}

/*
 * Put a socket in listening mode.
 */
int
do_listen(void)
{

	return ENOTSOCK;
}

/*
 * Accept a connection on a listening socket, creating a new socket.
 */
int
do_accept(void)
{

	return ENOTSOCK;
}

/*
 * Send a message on a socket.
 */
int
do_sendto(void)
{

	return ENOTSOCK;
}

/*
 * Receive a message from a socket.
 */
int
do_recvfrom(void)
{

	return ENOTSOCK;
}

/*
 * Send or receive a message on a socket using a message structure.
 */
int
do_sockmsg(void)
{

	return ENOTSOCK;
}

/*
 * Set socket options.
 */
int
do_setsockopt(void)
{

	return ENOTSOCK;
}

/*
 * Get socket options.
 */
int
do_getsockopt(void)
{

	return ENOTSOCK;
}

/*
 * Get the local address of a socket.
 */
int
do_getsockname(void)
{

	return ENOTSOCK;
}

/*
 * Get the remote address of a socket.
 */
int
do_getpeername(void)
{

	return ENOTSOCK;
}

/*
 * Shut down socket send and receive operations.
 */
int
do_shutdown(void)
{

	return ENOTSOCK;
}
