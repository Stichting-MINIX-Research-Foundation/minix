/*

   getsockname()

   from socket emulation library for Minix 2.0.x

*/


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


#define DEBUG 1

/*
   getsockname...
*/
int getsockname(int fd, struct sockaddr *_RESTRICT address, 
   socklen_t *_RESTRICT address_len)
{
	nwio_tcpconf_t tcpconf;
	socklen_t len;
	struct sockaddr_in sin;

#ifdef DEBUG
	fprintf(stderr,"mnx_getsockname: ioctl fd %d.\n", fd);
#endif
	if (ioctl(fd, NWIOGTCPCONF, &tcpconf)==-1) {
#ifdef DEBUG
	   fprintf(stderr,"mnx_getsockname: error %d\n", errno);
#endif
	   return (-1);
	   }
#ifdef DEBUG1
	fprintf(stderr, "mnx_getsockname: from %s, %u",
			inet_ntoa(tcpconf.nwtc_remaddr),
			ntohs(tcpconf.nwtc_remport));
	fprintf(stderr," for %s, %u\n",
			inet_ntoa(tcpconf.nwtc_locaddr),
			ntohs(tcpconf.nwtc_locport));
#endif
/*
	addr->sin_addr.s_addr = tcpconf.nwtc_remaddr ;
	addr->sin_port = tcpconf.nwtc_locport;
*/
	memset(&sin, '\0', sizeof(sin));
	sin.sin_family= AF_INET;
	sin.sin_addr.s_addr= tcpconf.nwtc_remaddr ;
	sin.sin_port= tcpconf.nwtc_locport;

	len= *address_len;
	if (len > sizeof(sin))
		len= sizeof(sin);
	memcpy(address, &sin, len);
	*address_len= len;

	return 0;
}








