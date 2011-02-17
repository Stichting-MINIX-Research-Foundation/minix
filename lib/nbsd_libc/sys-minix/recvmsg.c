#include <sys/cdefs.h>
#include "namespace.h"

#undef NDEBUG

#include <errno.h>
#include <sys/ioctl.h>
#include <sys/ioc_net.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#define DEBUG 0

static ssize_t _uds_recvmsg_conn(int sock, struct msghdr *msg, int flags);
static ssize_t _uds_recvmsg_dgram(int sock, struct msghdr *msg, int flags);

ssize_t recvmsg(int sock, struct msghdr *msg, int flags)
{
	int r;
	int uds_sotype;

	if (msg == NULL) {
		errno= EFAULT;
		return -1;
	}

	r= ioctl(sock, NWIOGUDSSOTYPE, &uds_sotype);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL)) {
		if (r == -1) {
			return r;
		}

		if (uds_sotype == SOCK_DGRAM) {
			return _uds_recvmsg_dgram(sock, msg, flags);
		} else {
			return _uds_recvmsg_conn(sock, msg, flags);
		}
	}

#if DEBUG
	fprintf(stderr, "recvmsg: not implemented for fd %d\n", sock);
#endif

	errno= ENOSYS;
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
