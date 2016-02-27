#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>
#include <sys/un.h>

static int _tcp_getpeername(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len, nwio_tcpconf_t *tcpconfp);

static int _udp_getpeername(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len, nwio_udpopt_t *tcpconfp);

static int _uds_getpeername(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len, struct sockaddr_un *uds_addr);

/*
 * Get the remote address of a socket.
 */
static int
__getpeername(int fd, struct sockaddr * __restrict address,
	socklen_t * __restrict address_len)
{
	message m;

	if (address_len == NULL) {
		errno = EFAULT;
		return -1;
	}

	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_sockaddr.fd = fd;
	m.m_lc_vfs_sockaddr.addr = (vir_bytes)address;
	m.m_lc_vfs_sockaddr.addr_len = *address_len;

	if (_syscall(VFS_PROC_NR, VFS_GETPEERNAME, &m) < 0)
		return -1;

	*address_len = m.m_vfs_lc_socklen.len;
	return 0;
}

int getpeername(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len)
{
	int r;
	nwio_tcpconf_t tcpconf;
	nwio_udpopt_t udpopt;
	struct sockaddr_un uds_addr;

	r = __getpeername(sock, address, address_len);
	if (r != -1 || (errno != ENOTSOCK && errno != ENOSYS))
		return r;

	r= ioctl(sock, NWIOGTCPCONF, &tcpconf);
	if (r != -1 || errno != ENOTTY)
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}
		return _tcp_getpeername(sock, address, address_len,
			&tcpconf);
	}

	r= ioctl(sock, NWIOGUDPOPT, &udpopt);
	if (r != -1 || errno != ENOTTY)
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}
		return _udp_getpeername(sock, address, address_len,
			&udpopt);
	}

	r= ioctl(sock, NWIOGUDSPADDR, &uds_addr);
	if (r != -1 || errno != ENOTTY)
	{
		if (r == -1)
		{
			/* Bad file descriptor */
			return -1;
		}
		return _uds_getpeername(sock, address, address_len,
			&uds_addr);
	}

	errno = ENOTSOCK;
	return -1;
}

static int _tcp_getpeername(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len, nwio_tcpconf_t *tcpconfp)
{
	socklen_t len;
	struct sockaddr_in sin;

	if (tcpconfp->nwtc_remaddr == 0 ||
		tcpconfp->nwtc_remport == 0)
	{
		errno= ENOTCONN;
		return -1;
	}

	memset(&sin, '\0', sizeof(sin));
	sin.sin_family= AF_INET;
	sin.sin_addr.s_addr= tcpconfp->nwtc_remaddr;
	sin.sin_port= tcpconfp->nwtc_remport;
	sin.sin_len = sizeof(sin);

	len= *address_len;
	if (len > sizeof(sin))
		len= sizeof(sin);
	memcpy(address, &sin, len);
	*address_len= len;

	return 0;
}

static int _udp_getpeername(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len, nwio_udpopt_t *udpopt)
{
	socklen_t len;
	struct sockaddr_in sin;

	if (udpopt->nwuo_remaddr == 0 ||
		udpopt->nwuo_remport == 0)
	{
		errno= ENOTCONN;
		return -1;
	}

	memset(&sin, '\0', sizeof(sin));
	sin.sin_family= AF_INET;
	sin.sin_addr.s_addr= udpopt->nwuo_remaddr;
	sin.sin_port= udpopt->nwuo_remport;
	sin.sin_len = sizeof(sin);

	len= *address_len;
	if (len > sizeof(sin))
		len= sizeof(sin);
	memcpy(address, &sin, len);
	*address_len= len;

	return 0;
}

static int _uds_getpeername(int sock, struct sockaddr *__restrict address,
	socklen_t *__restrict address_len, struct sockaddr_un *uds_addr)
{
	socklen_t len;

	if (uds_addr->sun_family != AF_UNIX)
	{
		errno= ENOTCONN;
		return -1;
	}

	len= *address_len;
	if (len > sizeof(struct sockaddr_un))
		len = sizeof(struct sockaddr_un);

	memcpy(address, uds_addr, len);
	*address_len= len;

	return 0;
}
