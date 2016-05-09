#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/ioc_net.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#define DEBUG 0

static ssize_t _uds_sendmsg_conn(int sock, const struct msghdr *msg, 
	int flags);
static ssize_t _uds_sendmsg_dgram(int sock, const struct msghdr *msg, 
	int flags);

/*
 * Send a message on a socket using a message structure.
 */
static ssize_t
__sendmsg(int fd, const struct msghdr * msg, int flags)
{
	struct iovec iov;
	const struct msghdr *msgp;
	struct msghdr msg2;
	char *ptr;
	message m;
	ssize_t r;

	/*
	 * Currently, MINIX3 does not support vector I/O operations.  Like in
	 * the readv and writev implementations, we coalesce the data vector
	 * into a single buffer used for I/O.  For future ABI compatibility, we
	 * then supply this buffer as a single vector element.  This involves
	 * supplying a modified copy of the message header, as well as extra
	 * pre-checks.  Once true vector I/O support has been added, the checks
	 * and vector I/O coalescing can be removed from here, leaving just the
	 * system call.  Nothing will change at the system call ABI level.
	 */
	if (msg == NULL || (msg->msg_iovlen > 1 && msg->msg_iov == NULL)) {
		errno = EFAULT;
		return -1;
	}

	if (msg->msg_iovlen < 0 || msg->msg_iovlen > IOV_MAX) {
		errno = EMSGSIZE;	/* different from readv/writev */
		return -1;
	}

	if (msg->msg_iovlen > 1) {
		if ((r = _vectorio_setup(msg->msg_iov, msg->msg_iovlen, &ptr,
		    _VECTORIO_WRITE)) < 0)
			return -1;

		iov.iov_base = ptr;
		iov.iov_len = r;

		memcpy(&msg2, msg, sizeof(msg2));
		msg2.msg_iov = &iov;
		msg2.msg_iovlen = 1;
		msgp = &msg2;
	} else
		msgp = msg;

	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_sockmsg.fd = fd;
	m.m_lc_vfs_sockmsg.msgbuf = (vir_bytes)msgp;
	m.m_lc_vfs_sockmsg.flags = flags;

	r = _syscall(VFS_PROC_NR, VFS_SENDMSG, &m);

	/* If we coalesced the vector, clean up. */
	if (msgp != msg) {
		_vectorio_cleanup(msg->msg_iov, msg->msg_iovlen, ptr, r,
		    _VECTORIO_WRITE);
	}

	return r;
}

ssize_t sendmsg(int sock, const struct msghdr *msg, int flags)
{
	int r;
	int uds_sotype;

	r = __sendmsg(sock, msg, flags);
	if (r != -1 || (errno != ENOTSOCK && errno != ENOSYS))
		return r;

	if (msg == NULL) {
		errno= EFAULT;
		return -1;
	}

	/* For old socket driver implementations, this flag is the default. */
	flags &= ~MSG_NOSIGNAL;

	r= ioctl(sock, NWIOGUDSSOTYPE, &uds_sotype);
	if (r != -1 || errno != ENOTTY) {
		if (r == -1) {
			return r;
		}

		if (uds_sotype == SOCK_DGRAM) {
			return _uds_sendmsg_dgram(sock, msg, flags);
		} else {
			return _uds_sendmsg_conn(sock, msg, flags);
		}

	}

	errno = ENOTSOCK;
	return -1;
}

static ssize_t _uds_sendmsg_conn(int sock, const struct msghdr *msg, 
	int flags)
{
	struct msg_control msg_ctrl;
	int r;

	if (flags != 0) {
#if DEBUG
		fprintf(stderr, "sendmsg(uds): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;

	}

	/* grab the control data */
	memset(&msg_ctrl, '\0', sizeof(struct msg_control));
	if (msg->msg_controllen > MSG_CONTROL_MAX) {
		errno = ENOMEM;
		return -1;
	} else if (msg->msg_controllen > 0) {
		memcpy(&msg_ctrl.msg_control, msg->msg_control,
							msg->msg_controllen);
	}
	msg_ctrl.msg_controllen = msg->msg_controllen;

	/* send the control data to PFS */
	r= ioctl(sock, NWIOSUDSCTRL, (void *) &msg_ctrl);
	if (r == -1) {
		return r;
	}

	/* Silently ignore destination, if given. */

	return writev(sock, msg->msg_iov, msg->msg_iovlen);
}

static ssize_t _uds_sendmsg_dgram(int sock, const struct msghdr *msg, 
	int flags)
{
	struct msg_control msg_ctrl;
	struct sockaddr_un *dest_addr;
	int r;

	if (flags != 0) {
#if DEBUG
		fprintf(stderr, "sendmsg(uds): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;

	}

	dest_addr = msg->msg_name;
	if (dest_addr == NULL) {
		errno= EFAULT;
		return -1;
	}

	/* set the target address */
	r= ioctl(sock, NWIOSUDSTADDR, (void *) dest_addr);
	if (r == -1) {
		return r;
	}

	/* grab the control data */
	memset(&msg_ctrl, '\0', sizeof(struct msg_control));
	if (msg->msg_controllen > MSG_CONTROL_MAX) {
		errno = ENOMEM;
		return -1;
	} else if (msg->msg_controllen > 0) {
		memcpy(&msg_ctrl.msg_control, msg->msg_control,
							msg->msg_controllen);
	}
	msg_ctrl.msg_controllen = msg->msg_controllen;

	/* send the control data to PFS */
	r= ioctl(sock, NWIOSUDSCTRL, (void *) &msg_ctrl);
	if (r == -1) {
		return r;
	}

	/* do the send */
	return writev(sock, msg->msg_iov, msg->msg_iovlen);
}
