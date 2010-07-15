#undef NDEBUG

#include <errno.h>
#include <net/ioctl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#define DEBUG 0

static ssize_t _uds_recvmsg_conn(int socket, struct msghdr *msg, int flags);
static ssize_t _uds_recvmsg_dgram(int socket, struct msghdr *msg, int flags);

ssize_t recvmsg(int socket, struct msghdr *msg, int flags)
{
	int r;
	int uds_sotype;

	if (msg == NULL) {
		errno= EFAULT;
		return -1;
	}

	r= ioctl(socket, NWIOGUDSSOTYPE, &uds_sotype);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1) {
			return r;
		}

		if (uds_sotype == SOCK_DGRAM) {
			return _uds_recvmsg_dgram(socket, msg, flags);
		} else {
			return _uds_recvmsg_conn(socket, msg, flags);
		}
	}

#if DEBUG
	fprintf(stderr, "recvmsg: not implemented for fd %d\n", socket);
#endif

	errno= ENOSYS;
	return -1;
}

static ssize_t _uds_recvmsg_conn(int socket, struct msghdr *msg, int flags)
{
	int r;

	if (flags != 0) {
#if DEBUG
		fprintf(stderr, "recvmsg(uds): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
	}

	r = readv(socket, msg->msg_iov, msg->msg_iovlen);

	if (r >= 0 && msg->msg_name && msg->msg_namelen > 0)
	{
		getpeername(socket, msg->msg_name, &msg->msg_namelen);
	}

	msg->msg_flags = 0;

	return r;
}

static ssize_t _uds_recvmsg_dgram(int socket, struct msghdr *msg, int flags)
{
	int r;

	if (flags != 0) {
#if DEBUG
		fprintf(stderr, "recvmsg(uds): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;
	}

	r = readv(socket, msg->msg_iov, msg->msg_iovlen);

	if (r >= 0 && msg->msg_name && msg->msg_namelen > 0)
	{
		ioctl(socket, NWIOGUDSFADDR, msg->msg_name);
		msg->msg_namelen= sizeof(struct sockaddr_un);

	}

	msg->msg_flags = 0;

	return r;
}
