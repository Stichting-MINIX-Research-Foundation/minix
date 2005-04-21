/* net.c Copyright 2000 by Michael Temari All Rights Reserved */
/* 04/05/2000 Michael Temari <Michael@TemWare.Com> */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <net/netlib.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/socket.h>
#include <net/gen/netdb.h>

#include "net.h"

int connect(host, port)
char *host;
int port;
{
nwio_tcpconf_t tcpconf;
nwio_tcpcl_t tcpcopt;
char *tcp_device;
int netfd;
ipaddr_t nethost;
tcpport_t netport;
struct hostent *hp;
struct servent *sp;
char *p;
int s;
int tries;

   if((hp = gethostbyname(host)) == (struct hostent *)NULL) {
	fprintf(stderr, "Unknown host %s!\n", host);  
	return(-1);
   } else
	memcpy((char *) &nethost, (char *) hp->h_addr, hp->h_length);

   netport = htons(port);

   /* Connect to the host */
   if((tcp_device = getenv("TCP_DEVICE")) == NULL)
	tcp_device = TCP_DEVICE;

   if((netfd = open(tcp_device, O_RDWR)) < 0) {
	perror("httpget: opening tcp");
	return(-1);
   }

   tcpconf.nwtc_flags = NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
   tcpconf.nwtc_remaddr = nethost;
   tcpconf.nwtc_remport = netport;

   s = ioctl(netfd, NWIOSTCPCONF, &tcpconf);
   if(s < 0) {
	perror("httpget: NWIOSTCPCONF");
	close(netfd);
	return(-1);
   }

   s = ioctl(netfd, NWIOGTCPCONF, &tcpconf);
   if(s < 0) {
	perror("httpget: NWIOGTCPCONF");
	close(netfd);
	return(-1);
   }

   tcpcopt.nwtcl_flags = 0;

   tries = 0;
   do {
	s = ioctl(netfd, NWIOTCPCONN, &tcpcopt);
	if(s == -1 && errno == EAGAIN) {
		if(tries++ >= 10)
			break;
		sleep(10);
	} else
		break;
   } while(1);

   if(s < 0) {
	perror("httpget: NWIOTCPCONN");
	close(netfd);
	return(-1);
   }

   return(netfd);
}
