#include <sys/cdefs.h>
#include "namespace.h"

#include <errno.h>
#include <stdlib.h>
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

ssize_t sendmsg(int sock, const struct msghdr *msg, int flags)
{
	int r;
	int uds_sotype;

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
			return _uds_sendmsg_dgram(sock, msg, flags);
		} else {
			return _uds_sendmsg_conn(sock, msg, flags);
		}

	}

#if DEBUG
	fprintf(stderr, "sendmsg: not implemented for fd %d\n", sock);
#endif

	errno= ENOSYS;
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
