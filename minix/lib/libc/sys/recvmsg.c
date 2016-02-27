#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/ioc_net.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#define DEBUG 0

static ssize_t _uds_recvmsg_conn(int sock, struct msghdr *msg, int flags);
static ssize_t _uds_recvmsg_dgram(int sock, struct msghdr *msg, int flags);

/*
 * Receive a message from a socket using a message structure.
 */
static ssize_t
__recvmsg(int fd, struct msghdr * msg, int flags)
{
	struct iovec iov;
	struct msghdr msg2, *msgp;
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
		    _VECTORIO_READ)) < 0)
			return -1;

		iov.iov_base = ptr;
		iov.iov_len = r;

		memcpy(&msg2, msg, sizeof(msg2));
		msg2.msg_iov = &iov;
		msg2.msg_iovlen = 1;
		msgp = &msg2;
	} else
		msgp = msg;

	/* Issue the actual system call. */
	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_sockmsg.fd = fd;
	m.m_lc_vfs_sockmsg.msgbuf = (vir_bytes)msgp;
	m.m_lc_vfs_sockmsg.flags = flags;

	r = _syscall(VFS_PROC_NR, VFS_RECVMSG, &m);

	/* If we coalesced the vector, clean up and copy back the results. */
	if (msgp != msg) {
		_vectorio_cleanup(msg->msg_iov, msg->msg_iovlen, ptr, r,
		    _VECTORIO_READ);

		if (r >= 0)
			memcpy(msg, &msg2, sizeof(msg2));
	}

	return r;
}

ssize_t recvmsg(int sock, struct msghdr *msg, int flags)
{
	int r;
	int uds_sotype;

	r = __recvmsg(sock, msg, flags);
	if (r != -1 || (errno != ENOTSOCK && errno != ENOSYS))
		return r;

	if (msg == NULL) {
		errno= EFAULT;
		return -1;
	}

	r= ioctl(sock, NWIOGUDSSOTYPE, &uds_sotype);
	if (r != -1 || errno != ENOTTY) {
		if (r == -1) {
			return r;
		}

		if (uds_sotype == SOCK_DGRAM) {
			return _uds_recvmsg_dgram(sock, msg, flags);
		} else {
			return _uds_recvmsg_conn(sock, msg, flags);
		}
	}

	errno = ENOTSOCK;
	return -1;
}

static ssize_t _uds_recvmsg_conn(int sock, struct msghdr *msg, int flags)
{
	int r, rc;

	if (flags != 0) {
#if DEBUG
		fprintf(stderr, "recvmsg(uds): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
	}

	r= readv(sock, msg->msg_iov, msg->msg_iovlen);

	if (r >= 0 && msg->msg_name && msg->msg_namelen > 0) {
		getpeername(sock, msg->msg_name, &msg->msg_namelen);
	}

	/* get control data */
	if (r >= 0 && msg->msg_control && msg->msg_controllen > 0) {
		struct msg_control msg_ctrl;

		memset(&msg_ctrl, '\0', sizeof(struct msg_control));
		msg_ctrl.msg_controllen = msg->msg_controllen;
		rc = ioctl(sock, NWIOGUDSCTRL, &msg_ctrl);
		if (rc == -1) {
			return rc;
		}

		if (msg_ctrl.msg_controllen <= msg->msg_controllen) {
			memcpy(msg->msg_control, msg_ctrl.msg_control,
						msg_ctrl.msg_controllen);
			msg->msg_controllen = msg_ctrl.msg_controllen;
		}
	}

	msg->msg_flags = 0;

	return r;
}

static ssize_t _uds_recvmsg_dgram(int sock, struct msghdr *msg, int flags)
{
	int r, rc;

	if (flags != 0) {
#if DEBUG
		fprintf(stderr, "recvmsg(uds): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
	}

	r= readv(sock, msg->msg_iov, msg->msg_iovlen);

	if (r >= 0 && msg->msg_name &&
				msg->msg_namelen >= sizeof(struct sockaddr_un))
	{
		rc= ioctl(sock, NWIOGUDSFADDR, msg->msg_name);
		if (rc == -1) {
			return rc;
		}
		msg->msg_namelen= sizeof(struct sockaddr_un);
	}

	/* get control data */
	if (r >= 0 && msg->msg_control && msg->msg_controllen > 0) {
		struct msg_control msg_ctrl;

		memset(&msg_ctrl, '\0', sizeof(struct msg_control));
		msg_ctrl.msg_controllen = msg->msg_controllen;
		rc = ioctl(sock, NWIOGUDSCTRL, &msg_ctrl);
		if (rc == -1) {
			return rc;
		}

		if (msg_ctrl.msg_controllen <= msg->msg_controllen) {
			memcpy(msg->msg_control, msg_ctrl.msg_control,
						msg_ctrl.msg_controllen);
			msg->msg_controllen = msg_ctrl.msg_controllen;
		}
	}

	msg->msg_flags = 0;

	return r;
}
