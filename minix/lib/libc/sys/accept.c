#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>

static int _tcp_accept(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len);

static int _uds_accept(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len);

/*
 * Accept a connection on a listening socket, creating a new socket.
 */
static int
__accept(int fd, struct sockaddr * __restrict address,
	socklen_t * __restrict address_len)
{
	message m;
	int r;

	if (address != NULL && address_len == NULL) {
		errno = EFAULT;
		return -1;
	}

	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_sockaddr.fd = fd;
	m.m_lc_vfs_sockaddr.addr = (vir_bytes)address;
	m.m_lc_vfs_sockaddr.addr_len = (address != NULL) ? *address_len : 0;

	if ((r = _syscall(VFS_PROC_NR, VFS_ACCEPT, &m)) < 0)
		return -1;

	if (address != NULL)
		*address_len = m.m_vfs_lc_socklen.len;
	return r;
}

int accept(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len)
{
	int r;
	nwio_udpopt_t udpopt;

	r = __accept(sock, address, address_len);
	if (r != -1 || (errno != ENOTSOCK && errno != ENOSYS))
		return r;

	r= _tcp_accept(sock, address, address_len);
	if (r != -1 || errno != ENOTTY)
		return r;

	r= _uds_accept(sock, address, address_len);
	if (r != -1 || errno != ENOTTY)
		return r;

	/* Unfortunately, we have to return EOPNOTSUPP for a socket that
	 * does not support accept (such as a UDP socket) and ENOTSOCK for
	 * filedescriptors that do not refer to a socket.
	 */
	r= ioctl(sock, NWIOGUDPOPT, &udpopt);
	if (r == 0 || (r == -1 && errno != ENOTTY)) {
		/* UDP socket */
		errno= EOPNOTSUPP;
		return -1;
	}

	errno = ENOTSOCK;
	return -1;
}

static int _tcp_accept(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len)
{
	int r, s1, t_errno;
	tcp_cookie_t cookie;

	s1= open(TCP_DEVICE, O_RDWR);
	if (s1 == -1)
		return s1;
	r= ioctl(s1, NWIOGTCPCOOKIE, &cookie);
	if (r == -1)
	{
		t_errno= errno;
		close(s1);
		errno= t_errno;
		return -1;
	}
	r= ioctl(sock, NWIOTCPACCEPTTO, &cookie);
	if (r == -1)
	{
		t_errno= errno;
		close(s1);
		errno= t_errno;
		return -1;
	}
	if (address != NULL)
		getpeername(s1, address, address_len);
	return s1;
}

static int _uds_accept(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len)
{
	int s1;
	int r;
	struct sockaddr_un uds_addr;
	socklen_t len;

	memset(&uds_addr, '\0', sizeof(struct sockaddr_un));

	r= ioctl(sock, NWIOGUDSADDR, &uds_addr);
	if (r == -1) {
		return r;
	}

	if (uds_addr.sun_family != AF_UNIX) {
		errno= EINVAL;
		return -1;
	}

	len= *address_len;
	if (len > sizeof(struct sockaddr_un))
		len = sizeof(struct sockaddr_un);

	memcpy(address, &uds_addr, len);
	*address_len= len;

	s1= open(UDS_DEVICE, O_RDWR);
	if (s1 == -1)
		return s1;

	/* Copy file descriptor flags from the listening socket. */
	fcntl(s1, F_SETFL, fcntl(sock, F_GETFL));

	r= ioctl(s1, NWIOSUDSACCEPT, address);
	if (r == -1) {
		int ioctl_errno = errno;
		close(s1);
		errno = ioctl_errno;
		return r;
	}

	return s1;
}
