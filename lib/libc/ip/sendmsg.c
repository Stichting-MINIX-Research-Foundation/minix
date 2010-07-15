#undef NDEBUG

#include <errno.h>
#include <net/ioctl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#define DEBUG 0

static ssize_t _uds_sendmsg_conn(int socket, const struct msghdr *msg, 
	int flags);
static ssize_t _uds_sendmsg_dgram(int socket, const struct msghdr *msg, 
	int flags);

ssize_t sendmsg(int socket, const struct msghdr *msg, int flags)
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
			return _uds_sendmsg_dgram(socket, msg, flags);
		} else {
			return _uds_sendmsg_conn(socket, msg, flags);
		}

	}

#if DEBUG
	fprintf(stderr, "sendmsg: not implemented for fd %d\n", socket);
#endif

	errno= ENOSYS;
	return -1;
}

static ssize_t _uds_sendmsg_conn(int socket, const struct msghdr *msg, 
	int flags)
{

	if (flags != 0) {
#if DEBUG
		fprintf(stderr, "sendmsg(uds): flags not implemented\n");
#endif
		errno= ENOSYS;
		return -1;

	}

	/* Silently ignore destination, if given. */

	return writev(socket, msg->msg_iov, msg->msg_iovlen);
}

static ssize_t _uds_sendmsg_dgram(int socket, const struct msghdr *msg, 
	int flags)
{
	char real_sun_path[PATH_MAX+1];
	char *realpath_result;
	char *dest_addr;
	int null_found;
	int i, r;

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

	/* sun_family is always supposed to be AF_UNIX */
	if (((struct sockaddr_un *) dest_addr)->sun_family != AF_UNIX) {
		errno = EAFNOSUPPORT;
		return -1;
	}

	/* an empty path is not supported */
	if (((struct sockaddr_un *) dest_addr)->sun_path[0] == '\0') {
		errno = ENOENT;
		return -1;
	}

	/* the path must be a null terminated string for realpath to work */
	for (null_found = i = 0;
		i < sizeof(((struct sockaddr_un *) dest_addr)->sun_path); i++) {
		if (((struct sockaddr_un *) dest_addr)->sun_path[i] == '\0') {
			null_found = 1;
			break;
		}
	}

	if (!null_found) {
		errno = EINVAL;
		return -1;
	}

	realpath_result = realpath(
		((struct sockaddr_un *) dest_addr)->sun_path, real_sun_path);

	if (realpath_result == NULL) {
		return -1;
	}

	if (strlen(real_sun_path) >= UNIX_PATH_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}

	/* set the target address */
	r= ioctl(socket, NWIOSUDSTADDR, (void *) dest_addr);
	if (r == -1) {
		return r;
	}

	/* do the send */
	return writev(socket, msg->msg_iov, msg->msg_iovlen);
}
