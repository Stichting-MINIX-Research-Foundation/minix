/* net.c Copyright Michael Temari 07/22/1996 All Rights Reserved */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <net/netlib.h>
#include <net/hton.h>
#include <net/gen/netdb.h>
#include <net/gen/in.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>
#include <net/gen/udp_hdr.h>

#include "talk.h"
#include "talkd.h"
#include "net.h"

static unsigned char buffer[8192];

static int udp_in;
static int udp_out;

static udpport_t ntalk_port;

int NetInit()
{
int s;
struct servent *servent;
char *udp_device;
nwio_udpopt_t udpopt;

   if((udp_device = getenv("UDP_DEVICE")) == (char *)NULL)
   	udp_device = UDP_DEVICE;

   if((udp_in = open(udp_device, O_RDWR)) < 0) {
   	fprintf(stderr, "talkd: Could not open %s: %s\n",
   		udp_device, strerror(errno));
   	return(-1);
   }

   if((udp_out = open(udp_device, O_RDWR)) < 0) {
   	fprintf(stderr, "talkd: Could not open %s: %s\n",
   		udp_device, strerror(errno));
   	close(udp_in);
   	return(-1);
   }

   if((servent = getservbyname("ntalk", "udp")) == (struct servent *)NULL) {
   	fprintf(stderr, "talkd: Could not find ntalk udp service\n");
   	close(udp_in);
   	close(udp_out);
   	return(-1);
   }

   ntalk_port = (udpport_t)servent->s_port;

   udpopt.nwuo_flags = NWUO_NOFLAGS;
   udpopt.nwuo_flags |= NWUO_COPY | NWUO_LP_SET | NWUO_EN_LOC;
   udpopt.nwuo_flags |= NWUO_DI_BROAD | NWUO_RP_ANY | NWUO_RA_ANY;
   udpopt.nwuo_flags |= NWUO_RWDATALL | NWUO_DI_IPOPT;
   udpopt.nwuo_locport = ntalk_port;

   s = ioctl(udp_in, NWIOSUDPOPT, &udpopt);
   if(s < 0) {
   	perror("talkd: ioctl NWIOSUDPOPT");
   	close(udp_in);
   	close(udp_out);
   	return(-1);
   }

   s = ioctl(udp_in, NWIOGUDPOPT, &udpopt);
   if(s < 0) {
   	perror("talkd: ioctl NWIOGUDPOPT");
   	close(udp_in);
   	close(udp_out);
   	return(-1);
   }

   return(0);
}

int getrequest(request)
struct talk_request *request;
{
int s;
udp_io_hdr_t *udp_io_hdr;

   s = read(udp_in, buffer, sizeof(buffer));
   if(s < 0) {
   	perror("talkd: Read error in getrequest");
   	return(-1);
   }
   if(s < sizeof(udp_io_hdr_t)) {
	fprintf(stderr, "talkd: Packet size read %d is smaller the udp_io_hdr\n", s);
	return(-1);
   }
   udp_io_hdr = (udp_io_hdr_t *)buffer;
   s = s - sizeof(udp_io_hdr_t);

   /* why is uih_data_len already in host order??? */

   if(udp_io_hdr->uih_data_len != s) {
	fprintf(stderr, "talkd: Size mismatch Packet %d  Udp Data %d\n",
		s, udp_io_hdr->uih_data_len);
	return(-1);
   }

   if(s != sizeof(struct talk_request)) {
   	fprintf(stderr, "talkd: Size mismatch in request %d %d\n",
   			s, sizeof(struct talk_request));
   	return(-1);
   }

   memcpy((char *)request, buffer + sizeof(udp_io_hdr_t), s);

   if(opt_d) {
   	fprintf(stderr, "Request: ");
	fprintf(stderr, "%02x %02x %02x %02x ",
	request->version, request->type, request->answer, request->junk);
	fprintf(stderr, "%08lx ", request->id);
	fprintf(stderr, "%04x %08lx:%04x\n",
		request->addr.sa_family, request->addr.sin_addr, request->addr.sin_port);
	fprintf(stderr, "                     %08lx ", request->pid);
	fprintf(stderr, "%04x %08lx:%04x\n",
		request->ctl_addr.sa_family, request->ctl_addr.sin_addr, request->ctl_addr.sin_port);
	fprintf(stderr, "          %-12.12s %-12.12s %-16.16s\n",
		request->luser, request->ruser, request->rtty);
   }

   return(0);
}

int sendreply(request, reply)
struct talk_request *request;
struct talk_reply *reply;
{
int s;
nwio_udpopt_t udpopt;
udp_io_hdr_t *udp_io_hdr;

   udpopt.nwuo_flags = NWUO_NOFLAGS;
   udpopt.nwuo_flags |= NWUO_COPY | NWUO_LP_SET | NWUO_EN_LOC;
   udpopt.nwuo_flags |= NWUO_DI_BROAD | NWUO_RP_SET | NWUO_RA_SET;
   udpopt.nwuo_flags |= NWUO_RWDATONLY | NWUO_DI_IPOPT;
   udpopt.nwuo_locport = ntalk_port;
   udpopt.nwuo_remaddr = request->ctl_addr.sin_addr;
   udpopt.nwuo_remport = request->ctl_addr.sin_port;

   s = ioctl(udp_out, NWIOSUDPOPT, &udpopt);
   if(s < 0) {
   	perror("talkd: ioctl NWIOSUDPOPT");
   	return(-1);
   }

   s = ioctl(udp_out, NWIOGUDPOPT, &udpopt);
   if(s < 0) {
   	perror("talkd: ioctl NWIOGUDPOPT");
   	return(-1);
   }

   if(opt_d) {
   	fprintf(stderr, "Reply:   ");
	fprintf(stderr, "%02x %02x %02x %02x ",
		reply->version, reply->type, reply->answer, reply->junk);
	fprintf(stderr, "%08lx ", reply->id);
	fprintf(stderr, "%04x %08lx:%04x",
		reply->addr.sa_family, reply->addr.sin_addr, reply->addr.sin_port);
	fprintf(stderr, "\n");
   }

   s = write(udp_out, reply, sizeof(struct talk_reply));
   if(s < 0) {
	perror("talkd: write");
	return(-1);
   }
   if(s != sizeof(struct talk_reply)) {
	fprintf(stderr, "talkd: write size mismatch %d %d\n",
		s, sizeof(struct talk_reply));
	return(-1);
   }
	
   return(0);
}
