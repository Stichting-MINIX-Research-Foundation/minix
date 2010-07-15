#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>
#include <sys/un.h>

#include <minix/config.h>
#include <minix/const.h>

#define DEBUG 0

static int _tcp_bind(int socket, const struct sockaddr *address,
	socklen_t address_len, nwio_tcpconf_t *tcpconfp);
static int _udp_bind(int socket, const struct sockaddr *address,
	socklen_t address_len, nwio_udpopt_t *udpoptp);
static int _uds_bind(int socket, const struct sockaddr *address,
	socklen_t address_len, struct sockaddr_un *uds_addr);

int bind(int socket, const struct sockaddr *address, socklen_t address_len)
{
	int r;
	nwio_tcpconf_t tcpconf;
	nwio_udpopt_t udpopt;
	struct sockaddr_un uds_addr;

	r= ioctl(socket, NWIOGTCPCONF, &tcpconf);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
			return r;
		r= _tcp_bind(socket, address, address_len, &tcpconf);
#if DEBUG
		if (r == -1)
		{
			int t_errno= errno;
			fprintf(stderr, "bind(tcp) failed: %s\n",
				strerror(errno));
			errno= t_errno;
		}
#endif
		return r;
	}

	r= ioctl(socket, NWIOGUDPOPT, &udpopt);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
			return r;
		return _udp_bind(socket, address, address_len, &udpopt);
	}

	r= ioctl(socket, NWIOGUDSADDR, &uds_addr);
	if (r != -1 || (errno != ENOTTY && errno != EBADIOCTL))
	{
		if (r == -1)
			return r;
		return _uds_bind(socket, address, address_len, &uds_addr);
	}

#if DEBUG
	fprintf(stderr, "bind: not implemented for fd %d\n", socket);
#endif
	errno= ENOSYS;
	return -1;
}

static int _tcp_bind(int socket, const struct sockaddr *address,
	socklen_t address_len, nwio_tcpconf_t *tcpconfp)
{
	int r;
	nwio_tcpconf_t tcpconf;
	struct sockaddr_in *sinp;

	sinp= (struct sockaddr_in *)address;
	if (sinp->sin_family != AF_INET || address_len != sizeof(*sinp))
	{
#if DEBUG
		fprintf(stderr, "bind(tcp): sin_family = %d, len = %d\n",
			sinp->sin_family, address_len);
#endif
		errno= EAFNOSUPPORT;
		return -1;
	}

	if (sinp->sin_addr.s_addr != INADDR_ANY &&
		sinp->sin_addr.s_addr != tcpconfp->nwtc_locaddr)
	{
		errno= EADDRNOTAVAIL;
		return -1;
	}

	tcpconf.nwtc_flags= 0;

	if (sinp->sin_port == 0)
		tcpconf.nwtc_flags |= NWTC_LP_SEL;
	else
	{
		tcpconf.nwtc_flags |= NWTC_LP_SET;
		tcpconf.nwtc_locport= sinp->sin_port;
	}

	r= ioctl(socket, NWIOSTCPCONF, &tcpconf);
	return r;
}

static int _udp_bind(int socket, const struct sockaddr *address,
	socklen_t address_len, nwio_udpopt_t *udpoptp)
{
	int r;
	unsigned long curr_flags;
	nwio_udpopt_t udpopt;
	struct sockaddr_in *sinp;

	sinp= (struct sockaddr_in *)address;
	if (sinp->sin_family != AF_INET || address_len != sizeof(*sinp))
	{
#if DEBUG
		fprintf(stderr, "bind(udp): sin_family = %d, len = %d\n",
			sinp->sin_family, address_len);
#endif
		errno= EAFNOSUPPORT;
		return -1;
	}

	if (sinp->sin_addr.s_addr != INADDR_ANY &&
		sinp->sin_addr.s_addr != udpoptp->nwuo_locaddr)
	{
		errno= EADDRNOTAVAIL;
		return -1;
	}

	udpopt.nwuo_flags= 0;

	if (sinp->sin_port == 0)
		udpopt.nwuo_flags |= NWUO_LP_SEL;
	else
	{
		udpopt.nwuo_flags |= NWUO_LP_SET;
		udpopt.nwuo_locport= sinp->sin_port;
	}

	curr_flags= udpoptp->nwuo_flags;
	if (!(curr_flags & NWUO_ACC_MASK))
		udpopt.nwuo_flags |= NWUO_EXCL;
	if (!(curr_flags & (NWUO_EN_LOC|NWUO_DI_LOC)))
		udpopt.nwuo_flags |= NWUO_EN_LOC;
	if (!(curr_flags & (NWUO_EN_BROAD|NWUO_DI_BROAD)))
		udpopt.nwuo_flags |= NWUO_EN_BROAD;
	if (!(curr_flags & (NWUO_RP_SET|NWUO_RP_ANY)))
		udpopt.nwuo_flags |= NWUO_RP_ANY;
	if (!(curr_flags & (NWUO_RA_SET|NWUO_RA_ANY)))
		udpopt.nwuo_flags |= NWUO_RA_ANY;
	if (!(curr_flags & (NWUO_RWDATONLY|NWUO_RWDATALL)))
		udpopt.nwuo_flags |= NWUO_RWDATALL;
	if (!(curr_flags & (NWUO_EN_IPOPT|NWUO_DI_IPOPT)))
		udpopt.nwuo_flags |= NWUO_DI_IPOPT;

	r= ioctl(socket, NWIOSUDPOPT, &udpopt);
	return r;
}

static int in_group(uid_t uid, gid_t gid)
{
	int r, i;
	int size;
	gid_t *list;

	size = sysconf(_SC_NGROUPS_MAX);
	list = malloc(size * sizeof(gid_t));

	if (list == NULL) {
		return 0;
	}

	r= getgroups(size, list);
	if (r == -1) {
		free(list);
		return 0;
	}

	for (i = 0; i < r; i++) {
		if (gid == list[i]) {
			free(list);
			return 1;
		}
	}

	free(list);
	return 0;
}


static int _uds_bind(int socket, const struct sockaddr *address,
	socklen_t address_len, struct sockaddr_un *uds_addr)
{
	mode_t bits, perm_bits, access_desired;
	struct stat buf;
	uid_t euid;
	gid_t egid;
	char real_sun_path[PATH_MAX+1];
	char *realpath_result;
	int i, r, shift;
	int null_found;
	int did_mknod;

	if (address == NULL) {
		errno = EFAULT;
		return -1;
	}

	/* sun_family is always supposed to be AF_UNIX */
	if (((struct sockaddr_un *) address)->sun_family != AF_UNIX) {
		errno = EAFNOSUPPORT;
		return -1;
	}

	/* an empty path is not supported */
	if (((struct sockaddr_un *) address)->sun_path[0] == '\0') {
		errno = ENOENT;
		return -1;
	}

	/* the path must be a null terminated string for realpath to work */
	for (null_found = i = 0;
		i < sizeof(((struct sockaddr_un *) address)->sun_path); i++) {
		if (((struct sockaddr_un *) address)->sun_path[i] == '\0') {
			null_found = 1;
			break;
		}
	}

	if (!null_found) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * Get the realpath(3) of the socket file.
	 */

	realpath_result = realpath(((struct sockaddr_un *) address)->sun_path,
						real_sun_path);
	if (realpath_result == NULL) {
		return -1;
	}

	if (strlen(real_sun_path) >= UNIX_PATH_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}

	strcpy(((struct sockaddr_un *) address)->sun_path, real_sun_path);

	/*
	 * input parameters look good -- create the socket file on the 
	 * file system
	 */

	did_mknod = 0;

	r = mknod(((struct sockaddr_un *) address)->sun_path,
		S_IFSOCK|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, 0);

	if (r == -1) {
		if (errno == EEXIST) {
			/* file already exists, verify that it is a socket */

			r = stat(((struct sockaddr_un *) address)->sun_path,
								&buf);
			if (r == -1) {
				return -1;
			}

			if (!S_ISSOCK(buf.st_mode)) {
				errno = EADDRINUSE;
				return -1;
			}

			/* check permissions the permissions of the 
			 * socket file.
			 */

			/* read + write access */
			access_desired = R_BIT | W_BIT;

			euid = geteuid();
			egid = getegid();

			if (euid == -1 || egid == -1) {
				errno = EACCES;
				return -1;
			}

			bits = buf.st_mode;

			if (euid == ((uid_t) 0)) {
				perm_bits = R_BIT | W_BIT;
			} else {
				if (euid == buf.st_uid) {
					shift = 6; /* owner */
				} else if (egid == buf.st_gid) {
					shift = 3; /* group */
				} else if (in_group(euid, buf.st_gid)) {
					shift = 3; /* suppl. groups */
				} else {
					shift = 0; /* other */
				}

				perm_bits = 
				(bits >> shift) & (R_BIT | W_BIT | X_BIT);
			}

			if ((perm_bits | access_desired) != perm_bits) {
				errno = EACCES;
				return -1;
			}

			/* if we get here permissions are OK */

		} else {

			return -1;
		}
	} else {
		did_mknod = 1;
	}

	/* perform the bind */
	r= ioctl(socket, NWIOSUDSADDR, (void *) address);

	if (r == -1 && did_mknod) {

		/* bind() failed in pfs, so we roll back the 
		 * file system change
		 */
		unlink(((struct sockaddr_un *) address)->sun_path);
	}

	return r;
}
