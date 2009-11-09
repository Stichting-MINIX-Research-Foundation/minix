/* net.c Copyright Michael Temari 08/01/1996 All Rights Reserved */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <net/netlib.h>
#include <net/hton.h>
#include <net/gen/netdb.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>
#include <net/gen/udp_hdr.h>

#include "talk.h"
#include "net.h"

_PROTOTYPE(void TimeOut, (int sig));

static unsigned char buffer[8192];

static int udp_ctl;
int tcp_fd;

static udpport_t ntalk_port;

char luser[USER_SIZE+1], ruser[USER_SIZE+1];
char lhost[HOST_SIZE+1], rhost[HOST_SIZE+1];
char ltty[TTY_SIZE+1], rtty[TTY_SIZE+1];
udpport_t ctlport;
tcpport_t dataport;
ipaddr_t laddr, raddr;

int NetInit()
{
int s;
struct servent *servent;
char *udp_device;
char *tcp_device;
nwio_udpopt_t udpopt;
nwio_tcpconf_t tcpconf;

   if((udp_device = getenv("UDP_DEVICE")) == (char *)NULL)
   	udp_device = UDP_DEVICE;

   if((udp_ctl = open(udp_device, O_RDWR)) < 0) {
   	fprintf(stderr, "talk: Could not open %s: %s\n",
   		udp_device, strerror(errno));
   	return(-1);
   }

   if((servent = getservbyname("ntalk", "udp")) == (struct servent *)NULL) {
   	fprintf(stderr, "talk: Could not find ntalk udp service\n");
   	close(udp_ctl);
   	return(-1);
   }

   ntalk_port = (udpport_t)servent->s_port;

   udpopt.nwuo_flags = NWUO_NOFLAGS;
   udpopt.nwuo_flags |= NWUO_COPY | NWUO_LP_SEL | NWUO_EN_LOC;
   udpopt.nwuo_flags |= NWUO_DI_BROAD | NWUO_RP_SET | NWUO_RA_SET;
   udpopt.nwuo_flags |= NWUO_RWDATONLY | NWUO_DI_IPOPT;
   udpopt.nwuo_remaddr = raddr;
   udpopt.nwuo_remport = ntalk_port;

   s = ioctl(udp_ctl, NWIOSUDPOPT, &udpopt);
   if(s < 0) {
   	perror("talk: ioctl NWIOSUDPOPT");
   	close(udp_ctl);
   	return(-1);
   }

   s = ioctl(udp_ctl, NWIOGUDPOPT, &udpopt);
   if(s < 0) {
   	perror("talk: ioctl NWIOGUDPOPT");
   	close(udp_ctl);
   	return(-1);
   }
   laddr = udpopt.nwuo_locaddr;
   ctlport = udpopt.nwuo_locport;

   if((tcp_device = getenv("TCP_DEVICE")) == (char *)NULL)
   	tcp_device = TCP_DEVICE;

   if((tcp_fd = open(tcp_device, O_RDWR)) < 0) {
   	fprintf(stderr, "talk: Could not open %s: %s\n",
   		tcp_device, strerror(errno));
   	close(udp_ctl);
   	return(-1);
   }

   tcpconf.nwtc_flags = NWTC_NOFLAGS;
   tcpconf.nwtc_flags |= NWTC_LP_SEL | NWTC_SET_RA | NWTC_UNSET_RP;
   tcpconf.nwtc_remaddr = raddr;

   s = ioctl(tcp_fd, NWIOSTCPCONF, &tcpconf);
   if(s < 0) {
   	perror("talk: ioctl NWIOSTCPCONF");
   	close(udp_ctl);
   	close(tcp_fd);
   	return(-1);
   }

   s = ioctl(tcp_fd, NWIOGTCPCONF, &tcpconf);
   if(s < 0) {
   	perror("talk: ioctl NWIOGTCPCONF");
   	close(udp_ctl);
   	close(tcp_fd);
   	return(-1);
   }

   dataport = tcpconf.nwtc_locport;

   return(0);
}

int getreply(reply, timeout)
struct talk_reply *reply;
int timeout;
{
int s;
int terrno;
udp_io_hdr_t *udp_io_hdr;

   signal(SIGALRM, TimeOut);
   alarm(timeout);
   s = read(udp_ctl, buffer, sizeof(buffer));
   terrno = errno;
   alarm(0);
   errno = terrno;
   if(s < 0 && errno == EINTR)
   	return(1);
   if(s < 0) {
   	perror("talk: Read error in getreply");
   	return(-1);
   }

   if(s == sizeof(struct talk_reply))
	memcpy((char *)reply, buffer, s);

   return(0);
}

int sendrequest(request, here)
struct talk_request *request;
int here;
{
int s;
nwio_udpopt_t udpopt;
udp_io_hdr_t *udp_io_hdr;

   udpopt.nwuo_flags = NWUO_NOFLAGS;
   udpopt.nwuo_flags |= NWUO_COPY | NWUO_LP_SET | NWUO_EN_LOC;
   udpopt.nwuo_flags |= NWUO_DI_BROAD | NWUO_RP_SET | NWUO_RA_SET;
   udpopt.nwuo_flags |= NWUO_RWDATONLY | NWUO_DI_IPOPT;
   udpopt.nwuo_locport = ctlport;
   if(here)
	udpopt.nwuo_remaddr = laddr;
   else
	udpopt.nwuo_remaddr = raddr;
   udpopt.nwuo_remport = ntalk_port;

   s = ioctl(udp_ctl, NWIOSUDPOPT, &udpopt);
   if(s < 0) {
   	perror("talk: ioctl NWIOSUDPOPT");
   	return(-1);
   }

   s = ioctl(udp_ctl, NWIOGUDPOPT, &udpopt);
   if(s < 0) {
   	perror("talk: ioctl NWIOGUDPOPT");
   	return(-1);
   }

   s = write(udp_ctl, request, sizeof(struct talk_request));
   if(s < 0) {
   	perror("talk: write error in sendrequest");
   	return(-1);
   }

   if(s != sizeof(struct talk_request)) {
   	fprintf(stderr, "talk: sendrequest size mismatch %d %d\n", s, sizeof(struct talk_request));
   	return(-1);
   }

   return(0);
}

void TimeOut(sig)
int sig;
{
}

int NetConnect(port)
u16_t port;
{
int s;
nwio_tcpconf_t tcpconf;
nwio_tcpcl_t tcpcopt;

   tcpconf.nwtc_flags = NWTC_NOFLAGS;
   tcpconf.nwtc_flags |= NWTC_LP_SET | NWTC_SET_RA | NWTC_SET_RP;
   tcpconf.nwtc_locport = dataport;
   tcpconf.nwtc_remaddr = raddr;
   tcpconf.nwtc_remport = port;

   s = ioctl(tcp_fd, NWIOSTCPCONF, &tcpconf);
   if(s < 0) {
   	perror("talk: ioctl NWIOSTCPCONF");
   	return(-1);
   }

   s = ioctl(tcp_fd, NWIOGTCPCONF, &tcpconf);
   if(s < 0) {
   	perror("talk: ioctl NWIOGTCPCONF");
   	return(-1);
   }

   tcpcopt.nwtcl_flags = 0;

   s = ioctl(tcp_fd, NWIOTCPCONN, &tcpcopt);
   if(s < 0 && errno == ECONNREFUSED)
   	return(1);
   if(s < 0) {
   	perror("talk: ioctl NWIOTCPCONN");
   	return(-1);
   }

   return(0);
}

int NetListen(timeout)
int timeout;
{
int s;
nwio_tcpcl_t tcplopt;
int terrno;

   tcplopt.nwtcl_flags = 0;

   signal(SIGALRM, TimeOut);
   alarm(timeout);
   s = ioctl(tcp_fd, NWIOTCPLISTEN, &tcplopt);
   terrno = errno;
   alarm(0);
   errno = terrno;

   if(s < 0 && errno == EINTR)
   	return(1);

   if(s < 0) {
   	perror("talk: ioctl NWIOTCPLISTEN");
   	return(-1);
   }

   return(0);
}
