#include <sys/cdefs.h>
#include "namespace.h"

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <net/netlib.h>
#include <sys/ioc_net.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#define DEBUG 0

static int _uds_socketpair(int type, int protocol, int sv[2]);

/*
 * Create a pair of connected sockets
 */
int socketpair(int domain, int type, int protocol, int sv[2]) {

#if DEBUG
	fprintf(stderr, "socketpair: domain %d, type %d, protocol %d\n",
		domain, type, protocol);
#endif

	if (domain != AF_UNIX)
	{
		errno = EAFNOSUPPORT;
		return -1;
	}

	if (domain == AF_UNIX &&
			(type == SOCK_STREAM || type == SOCK_SEQPACKET))
		return _uds_socketpair(type, protocol, sv);

#if DEBUG
	fprintf(stderr,
		"socketpair: nothing for domain %d, type %d, protocol %d\n",
		domain, type, protocol);
#endif

	errno= EPROTOTYPE;
	return -1;
}

static int _uds_socketpair(int type, int protocol, int sv[2])
{
	dev_t dev;
	int r, i;
	struct stat sbuf;

	if (protocol != 0)
	{
#if DEBUG
		fprintf(stderr, "socketpair(uds): bad protocol %d\n", protocol);
#endif
		errno= EPROTONOSUPPORT;
		return -1;
	}

	/* in this 'for' loop two unconnected sockets are created */
	for (i = 0; i < 2; i++) {
		sv[i]= open(UDS_DEVICE, O_RDWR);
		if (sv[i] == -1) {
			int open_errno = errno;

			if (i == 1) {
				/* if we failed to open() the 2nd 
				 * socket, we need to close the 1st
				 */
				close(sv[0]);
				errno = open_errno;
			}

			return -1;
		}

		/* set the type for the socket via ioctl
		 * (SOCK_STREAM, SOCK_SEQPACKET, etc)
		 */
		r= ioctl(sv[i], NWIOSUDSTYPE, &type);
		if (r == -1) {
			int ioctl_errno;

			/* if that failed rollback socket creation */
			ioctl_errno= errno;
			close(sv[i]);

			if (i == 1) {
				/* if we just closed the 2nd socket, we 
				 * need to close the 1st
				 */
				close(sv[0]);
			}

			/* return the error thrown by the call to ioctl */
			errno= ioctl_errno;
			return -1;
		}
	}

	r= fstat(sv[1], &sbuf);
	if (r == -1) {
		int fstat_errno;

		/* if that failed rollback socket creation */
		fstat_errno= errno;

		close(sv[0]);
		close(sv[1]);

		/* return the error thrown by the call to fstat */
		errno= fstat_errno;
		return -1;
	}

	dev = sbuf.st_dev;

	/* connect the sockets sv[0] and sv[1] */
	r= ioctl(sv[0], NWIOSUDSPAIR, &dev);
	if (r == -1) {
		int ioctl_errno;

		/* if that failed rollback socket creation */
		ioctl_errno= errno;

		close(sv[0]);
		close(sv[1]);

		/* return the error thrown by the call to ioctl */
		errno= ioctl_errno;
		return -1;
	}


	return 0;
}
