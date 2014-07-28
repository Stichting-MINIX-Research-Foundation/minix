/*	devices.c - Handle network devices.
 *							Author: Kees J. Bot
 *								11 Jun 1999
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/asynchio.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <net/gen/ether.h>
#include <net/gen/eth_hdr.h>
#include <net/gen/eth_io.h>
#include <net/gen/ip_hdr.h>
#include <net/gen/ip_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/udp_io.h>
#include <net/gen/dhcp.h>
#include "dhcpd.h"

void get_buf(buf_t **bp)
{
    /* Allocate and return a buffer pointer iff *bp == nil. */
    if (*bp != nil) {
	/* Already has one. */
    } else {
	/* Get one from the heap. */
	buf_t *new= allocate(sizeof(*new));
	new->dhcp= (dhcp_t *) (new->buf + sizeof(eth_hdr_t)
				+ sizeof(ip_hdr_t) + sizeof(udp_hdr_t));
	new->udpio= ((udp_io_hdr_t *) new->dhcp) - 1;
	new->udp= ((udp_hdr_t *) new->dhcp) - 1;
	new->ip= ((ip_hdr_t *) new->udp) - 1;
	new->eth= ((eth_hdr_t *) new->ip) - 1;
	*bp= new;
    }
}

void put_buf(buf_t **bp)
{
    /* Return a buffer to the heap. */
    if (*bp != nil) {
	free(*bp);
	*bp= nil;
    }
}

void give_buf(buf_t **dbp, buf_t **sbp)
{
    /* Hand over a buffer to another variable. */
    put_buf(dbp);
    *dbp= *sbp;
    *sbp= nil;
}

#define N_FDS		16		/* Minix can go async on many fds. */

static fd_t fds[N_FDS];			/* List of open descriptors. */
static struct network *fdwaitq;		/* Queue of nets waiting for fds. */

network_t *newnetwork(void)
{
    /* Create and initialize a network structure. */
    network_t *new;

    new= allocate(sizeof(*new));
    memset(new, 0, sizeof(*new));
    new->hostname= nil;
    new->solicit= NEVER;
    new->sol_ct= -1;
    return new;
}

void closefd(fd_t *fdp)
{
    /* Close a descriptor. */
    if (fdp->fdtype != FT_CLOSED) {
	asyn_close(&asyn, fdp->fd);
	close(fdp->fd);
	fdp->fdtype= FT_CLOSED;
	fdp->since= 0;
	put_buf(&fdp->bp);
	if (debug >= 3) printf("%s: Closed\n", fdp->device);
    }
}

static void timeout(int signum)
{
    /* nothing to do, ioctl will be aborted automatically */
    if (alarm(1) < 0) fatal("alarm(1)");
}

int opendev(network_t *np, fdtype_t fdtype, int compete)
{
    /* Make sure that a network has the proper device open and configured.
     * Return true if this is made so, or false if the device doesn't exist.
     * If compete is true then the caller competes for old descriptors.
     * The errno value is EAGAIN if we're out of descriptors.
     */
    fd_t *fdp, *fdold;
    time_t oldest;
    nwio_ethstat_t ethstat;
    nwio_ethopt_t ethopt;
    nwio_ipopt_t ipopt;
    nwio_udpopt_t udpopt;
    network_t **pqp;
    static char devbytype[][4] = { "", "eth", "ip", "udp", "udp" };

    /* Don't attempt to open higher level devices if not bound. */
    if (!(np->flags & NF_BOUND) && fdtype > FT_ETHERNET) {
	errno= EAGAIN;
	return 0;
    }

    /* Check if already open / Find the oldest descriptor. */
    fdold= nil;
    oldest= NEVER;
    for (fdp= fds; fdp < arraylimit(fds); fdp++) {
	if (fdp->n == np->n && fdp->fdtype == fdtype) {
	    /* Already open. */
	    np->fdp= fdp;
	    return 1;
	}
	if (fdp->since <= oldest) { fdold= fdp; oldest= fdp->since; }
    }

    /* None free?  Then wait for one to get old if so desired. */
    if (fdold->fdtype != FT_CLOSED && !compete) {
	errno= EAGAIN;
	return 0;
    }

    if (!(np->flags & NF_WAIT)) {
	for (pqp= &fdwaitq; *pqp != nil; pqp= &(*pqp)->wait) {}
	*pqp= np;
	np->wait= nil;
	np->flags |= NF_WAIT;
    }

    /* We allow a net to keep a descriptor for half of the fast period. */
    oldest += DELTA_FAST/2;

    if (fdwaitq != np || (fdold->fdtype != FT_CLOSED && oldest > now)) {
	/* This net is not the first in the queue, or the oldest isn't
	 * old enough.  Forget it for now.
	 */
	if (oldest < event) event= oldest;
	errno= EAGAIN;
	return 0;
    }

    /* The oldest is mine. */
    np->flags &= ~NF_WAIT;
    fdwaitq= np->wait;
    closefd(fdold);

    /* Open the proper device in the proper mode. */
    fdp= fdold;
    fdp->n= np->n;
    if (lwip && (fdtype == FT_ETHERNET || fdtype == FT_ICMP))
	    sprintf(fdp->device, "/dev/ip");
    else
	    sprintf(fdp->device, "/dev/%s%d", devbytype[fdtype], np->n);
    np->fdp= fdp;

    if ((fdp->fd= open(fdp->device, O_RDWR)) < 0) {
	if (errno == ENOENT || errno == ENODEV || errno == ENXIO) return 0;
	fatal(fdp->device);
    }

    switch (fdtype) {
    case FT_ETHERNET:
	 if (lwip) {
		 nwio_ipopt_t ipopt;
		 int result;
		 char ethdev[64];
		 int efd;
		 
		 sprintf(ethdev, "/dev/eth%d", np->n);
    
		 if ((efd = open(fdp->device, O_RDWR)) < 0) {
			 if (errno == ENOENT || errno == ENODEV ||
					 errno == ENXIO)
				 return 0;
			 fatal(ethdev);
		 }
	
		 if (ioctl(efd, NWIOGETHSTAT, &ethstat) < 0) {
			 /* Not an Ethernet. */
			 close(efd);
			 return 0;
		 }
		 close(efd);

		 np->eth= ethstat.nwes_addr;

		 ipopt.nwio_flags= NWIO_COPY | NWIO_PROTOSPEC;
		 ipopt.nwio_proto= 17; /* UDP */
		 result= ioctl (fdp->fd, NWIOSIPOPT, &ipopt);
		 if (result<0)
			 perror("ioctl (NWIOSIPOPT)"), exit(1);

		 break;
	 }
	/* Cannot use NWIOGETHSTAT in non-blocking mode due to a race between 
         * the reply from the ethernet driver and the cancel message from VFS
	 * for reaching inet. Hence, a signal is used to interrupt NWIOGETHSTAT
	 * in case the driver isn't ready yet.
	 */
	if (signal(SIGALRM, timeout) == SIG_ERR) fatal("signal(SIGALRM)");
	if (alarm(1) < 0) fatal("alarm(1)");
	if (ioctl(np->fdp->fd, NWIOGETHSTAT, &ethstat) < 0) {
	    /* Not an Ethernet. */
	    close(fdp->fd);
	    return 0;
	}
	if (alarm(0) < 0) fatal("alarm(0)");
	np->eth= ethstat.nwes_addr;
	ethopt.nweo_flags= NWEO_COPY | NWEO_EN_LOC | NWEO_EN_BROAD
			| NWEO_REMANY | NWEO_TYPEANY | NWEO_RWDATALL;

	if (ioctl(fdp->fd, NWIOSETHOPT, &ethopt) < 0) {
	    fprintf(stderr, "%s: %s: Unable to set eth options: %s\n",
		program, fdp->device, strerror(errno));
	    exit(1);
	}
	break;

    case FT_ICMP:
	ipopt.nwio_flags= NWIO_COPY | NWIO_EN_LOC | NWIO_EN_BROAD
			| NWIO_REMANY | NWIO_PROTOSPEC
			| NWIO_HDR_O_SPEC | NWIO_RWDATALL;
	ipopt.nwio_tos= 0;
	ipopt.nwio_ttl= 1;
	ipopt.nwio_df= 0;
	ipopt.nwio_hdropt.iho_opt_siz= 0;
	ipopt.nwio_proto= IPPROTO_ICMP;

	if (ioctl(fdp->fd, NWIOSIPOPT, &ipopt) < 0) {
	    fprintf(stderr, "%s: %s: Unable to set IP options: %s\n",
		program, fdp->device, strerror(errno));
	    exit(1);
	}
	break;

    case FT_BOOTPC:
	if (lwip) {
		struct sockaddr_in si_me;

		close(fdp->fd);
		fdp->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (fdp->fd < 0)
			return 0;
		memset((char *) &si_me, 0, sizeof(si_me));
		si_me.sin_family = AF_INET;
		si_me.sin_addr.s_addr = htonl(INADDR_ANY);
		si_me.sin_port = htons(port_client);
		if (bind(fdp->fd, (struct sockaddr *) &si_me,
					sizeof(si_me)) == -1) {
			close(fdp->fd);
			printf("DHCP : cannot bind client socket to port %d\n",
								port_client);
			return 0;
		}
		break;
	}
	udpopt.nwuo_flags= NWUO_COPY | NWUO_EN_LOC | NWUO_EN_BROAD
			| NWUO_RP_ANY | NWUO_RA_ANY | NWUO_RWDATALL
			| NWUO_DI_IPOPT | NWUO_LP_SET;
	udpopt.nwuo_locport= port_client;
	goto udp;

    case FT_BOOTPS:
	udpopt.nwuo_flags= NWUO_EXCL | NWUO_EN_LOC | NWUO_EN_BROAD
			| NWUO_RP_ANY | NWUO_RA_ANY | NWUO_RWDATALL
			| NWUO_DI_IPOPT | NWUO_LP_SET;
	udpopt.nwuo_locport= port_server;
    udp:
	if (ioctl(fdp->fd, NWIOSUDPOPT, &udpopt) == -1) {
	    fprintf(stderr, "%s: %s: Unable to set UDP options: %s\n",
		program, fdp->device, strerror(errno));
	    exit(1);
	}
	break;

    default:;
    }

    fdp->fdtype= fdtype;
    fdp->since= now;
    if (debug >= 3) printf("%s: Opened\n", fdp->device);
    return 1;
}

void closedev(network_t *np, fdtype_t fdtype)
{
    /* We no longer need a given type of device to be open. */
    fd_t *fdp;

    for (fdp= fds; fdp < arraylimit(fds); fdp++) {
	if (fdp->n == np->n && (fdp->fdtype == fdtype || fdtype == FT_ALL)) {
	    closefd(fdp);
	}
    }
}

char *ipdev(int n)
{
    /* IP device for network #n. */
    static char device[sizeof("/dev/ipNNN")];

    sprintf(device, "/dev/ip%d", n);
    return device;
}

void set_ipconf(char *device, ipaddr_t ip, ipaddr_t mask, unsigned mtu)
{
    /* Set IP address and netmask of an IP device. */
    int fd;
    nwio_ipconf_t ipconf;

    if (test > 0) return;

    if ((fd= open(device, O_RDWR)) < 0) fatal(device);
    ipconf.nwic_flags= NWIC_IPADDR_SET | NWIC_NETMASK_SET;
    ipconf.nwic_ipaddr= ip;
    ipconf.nwic_netmask= mask;
#ifdef NWIC_MTU_SET
    if (mtu != 0) {
	ipconf.nwic_flags |= NWIC_MTU_SET;
	ipconf.nwic_mtu= mtu;
    }
#endif
    if (ioctl(fd, NWIOSIPCONF, &ipconf) < 0) fatal(device);
    close(fd);
}
