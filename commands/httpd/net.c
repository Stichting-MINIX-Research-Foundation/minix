/* net.c
 *
 * This file is part of httpd.
 *
 * 01/25/1996 			Michael Temari <Michael@TemWare.Com>
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002 			Michael Temari <Michael@TemWare.Com>
 *
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <net/netlib.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/socket.h>
#include <net/gen/netdb.h>

#include "net.h"

_PROTOTYPE(static void release, (int *fd));

ipaddr_t myipaddr, rmtipaddr;
tcpport_t myport, rmtport;
char myhostname[256];
char rmthostname[256];
char rmthostaddr[3+1+3+1+3+1+3+1];

void GetNetInfo()
{
nwio_tcpconf_t tcpconf;
int s;
struct hostent *hostent;

   /* Ask the system what our hostname is. */
   if(gethostname(myhostname, sizeof(myhostname)) < 0)
	strcpy(myhostname, "unknown");

   /* lets get our ip address and the clients ip address */
   s = ioctl(0, NWIOGTCPCONF, &tcpconf);
   if(s < 0) {
   	myipaddr = 0;
   	myport = 0;
   	rmtipaddr = 0;
   	rmtport = 0;
   	strcpy(rmthostname, "??Unknown??");
	strcpy(rmthostaddr, "???.???.???.???");
   	return;
   }

   myipaddr = tcpconf.nwtc_locaddr;
   myport = tcpconf.nwtc_locport;
   rmtipaddr = tcpconf.nwtc_remaddr;
   rmtport = tcpconf.nwtc_remport;

   /* Look up the host name of the remote host. */
   hostent = gethostbyaddr((char *) &rmtipaddr, sizeof(rmtipaddr), AF_INET);
   if(!hostent)
	strncpy(rmthostname, inet_ntoa(rmtipaddr), sizeof(rmthostname)-1);
   else
	strncpy(rmthostname, hostent->h_name, sizeof(rmthostname)-1);

   strcpy(rmthostaddr, inet_ntoa(rmtipaddr));

   rmthostname[sizeof(rmthostname)-1] = '\0';

   return;
}

static void release(fd)
int *fd;
{
   if(*fd != -1) {
	close(*fd);
	*fd= -1;
   }
}

void daemonloop(service)
char *service;
{
tcpport_t port;
struct nwio_tcpcl tcplistenopt;
struct nwio_tcpconf tcpconf;
struct nwio_tcpopt tcpopt;
struct servent *servent;
char *tcp_device;
int tcp_fd, client_fd, r;
int pfd[2];
unsigned stall= 0;

   if((servent= getservbyname(service, "tcp")) == NULL) {
	unsigned long p;
	char *end;

	p = strtoul(service, &end, 0);
	if(p <= 0 || p > 0xFFFF || *end != 0) {
		fprintf(stderr, "httpd: %s: Unknown service\n", service);
		exit(1);
	}
	port= htons((tcpport_t) p);
   } else
	port= servent->s_port;

   /* No client yet. */
   client_fd= -1;

   while (1) {
   	if((tcp_device = getenv("TCP_DEVICE")) == NULL)
   		tcp_device = TCP_DEVICE;
	if ((tcp_fd= open(tcp_device, O_RDWR)) < 0) {
			fprintf(stderr, "httpd: Can't open %s: %s",
				tcp_device, strerror(errno));
			if (errno == ENOENT || errno == ENODEV
							|| errno == ENXIO) {
				exit(1);
			}
			goto bad;
		}

		tcpconf.nwtc_flags= NWTC_LP_SET | NWTC_UNSET_RA | NWTC_UNSET_RP;
		tcpconf.nwtc_locport= port;

		if (ioctl(tcp_fd, NWIOSTCPCONF, &tcpconf) < 0) {
			fprintf(stderr, "httpd: Can't configure TCP channel",
				strerror(errno));
			exit(1);
		}

#ifdef NWTO_DEL_RST
		tcpopt.nwto_flags= NWTO_DEL_RST;

		if (ioctl(tcp_fd, NWIOSTCPOPT, &tcpopt) < 0) {
			fprintf(stderr, "httpd: Can't set TCP options",
				strerror(errno));
			exit(1);
		}
#endif

		if (client_fd != -1) {
			/* We have a client, so start a server for it. */

#ifdef NWTO_DEL_RST
			tcpopt.nwto_flags= 0;
			(void) ioctl(client_fd, NWIOSTCPOPT, &tcpopt);
#endif

			fflush(NULL);

			/* Create a pipe to serve as an error indicator. */
			if (pipe(pfd) < 0) {
				fprintf(stderr, "httpd: pipe", strerror(errno));
				goto bad;
			}

			/* Fork twice to daemonize child. */
			switch (fork()) {
			case -1:
				fprintf(stderr, "httpd: fork", strerror(errno));
				close(pfd[0]);
				close(pfd[1]);
				goto bad;
			case 0:
				close(tcp_fd);
				close(pfd[0]);
				switch (fork()) {
				case -1:
					fprintf(stderr, "httpd: fork",
						strerror(errno));
					write(pfd[1], &errno, sizeof(errno));
					exit(1);
				case 0:
					break;
				default:
					exit(0);
				}
				dup2(client_fd, 0);
				dup2(client_fd, 1);
				close(client_fd);
				close(pfd[1]);

				/* Break out of the daemon loop, continuing with
				 * the normal httpd code to serve the client.
				 */
				return;

			default:
				release(&client_fd);
				close(pfd[1]);
				wait(NULL);
				r= read(pfd[0], &errno, sizeof(errno));
				close(pfd[0]);
				if (r != 0) goto bad;
				break;
			}
		}

		/* Wait for a new connection. */
		tcplistenopt.nwtcl_flags= 0;

		while (ioctl(tcp_fd, NWIOTCPLISTEN, &tcplistenopt) < 0) {
			if (errno != EAGAIN) {
				fprintf(stderr, "httpd: Unable to listen: %s",
					strerror(errno));
			}
			goto bad;
		}

		/* We got a connection. */
		client_fd= tcp_fd;
		tcp_fd= -1;

		/* All is well, no need to stall. */
		stall= 0;
		continue;

	bad:
		/* All is not well, release resources. */
		release(&tcp_fd);
		release(&client_fd);

		/* Wait a bit if this happens more than once. */
		if (stall != 0) {
			sleep(stall);
			stall <<= 1;
		} else {
			stall= 1;
		}
	}
}
