/* net.c
 *
 * This file is part of ftp.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <temari@ix.netcom.com>
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <net/netlib.h>
#include <net/hton.h>
#include <net/gen/netdb.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

#include "ftp.h"
#include "file.h"
#include "net.h"

_PROTOTYPE(void donothing, (int sig));

static int ftpcomm_fd;
static ipaddr_t myip;
static ipaddr_t hostip;
static char host[256];
static int lpid;

void NETinit()
{
int s;
char *tcp_device;
int tcp_fd;
nwio_tcpconf_t nwio_tcpconf;

   /* All this just to get our ip address */

   if((tcp_device = getenv("TCP_DEVICE")) == (char *)NULL)
	tcp_device = TCP_DEVICE;

   tcp_fd = open(tcp_device, O_RDWR);
   if(tcp_fd < 0) {
	perror("ftp: Could not open tcp_device");
	exit(-1);
   }
   s = ioctl(tcp_fd, NWIOGTCPCONF, &nwio_tcpconf);
   if(s < 0) {
	perror("ftp: Could not get tcp configuration");
	exit(-1);
   }

   myip = nwio_tcpconf.nwtc_locaddr;

   close(tcp_fd);
}

int DOopen()
{
nwio_tcpconf_t tcpconf;
nwio_tcpcl_t tcpcopt;
char *tcp_device;
tcpport_t port;
int s;
struct hostent *hp;
struct servent *servent;

   if(linkopen) {
	printf("Use \"CLOSE\" to close the connection first.\n");
	return(0);
   }

   if(cmdargc < 2)
	readline("Host: ", host, sizeof(host));
   else
	strncpy(host, cmdargv[1], sizeof(host));

   if((servent = getservbyname("ftp", "tcp")) == (struct servent *)NULL) {
   	fprintf(stderr, "ftp: Could not find ftp tcp service\n");
   	return(-1);
   }
   port = (tcpport_t)servent->s_port;

   hp = gethostbyname(host);
   if (hp == (struct hostent *)NULL) {
	hostip = (ipaddr_t)0;
	printf("Unresolved host %s\n", host);
	return(0);
   } else
	memcpy((char *) &hostip, (char *) hp->h_addr, hp->h_length);

   /* This HACK allows the server to establish data connections correctly */
   /* when using the loopback device to talk to ourselves */
   if(hostip == inet_addr("127.0.0.1"))
	hostip = myip;

   if((tcp_device = getenv("TCP_DEVICE")) == NULL)
	tcp_device = "/dev/tcp";

   if((ftpcomm_fd = open(tcp_device, O_RDWR)) < 0) {
	perror("ftp: open error on tcp device");
	return(-1);
   }

   tcpconf.nwtc_flags = NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
   tcpconf.nwtc_remaddr = hostip;
   tcpconf.nwtc_remport = port;

   s = ioctl(ftpcomm_fd, NWIOSTCPCONF, &tcpconf);
   if(s < 0) {
	perror("ftp: ioctl error on NWIOSTCPCONF");
	close(ftpcomm_fd);
	return(s);
   }

   tcpcopt.nwtcl_flags = 0;

   s = ioctl(ftpcomm_fd, NWIOTCPCONN, &tcpcopt);
   if(s < 0) {
	perror("ftp: ioctl error on NWIOTCPCONN");
	close(ftpcomm_fd);
	return(s);
   }

   s = ioctl(ftpcomm_fd, NWIOGTCPCONF, &tcpconf);
   if(s < 0) {
	perror("ftp: ioctl error on NWIOGTCPCONF");
	close(ftpcomm_fd);
	return(s);
   }

   fpcommin  = fdopen(ftpcomm_fd, "r");
   fpcommout = fdopen(ftpcomm_fd, "w");

   s = DOgetreply();

   if(s < 0) {
	fclose(fpcommin);
	fclose(fpcommout);
	close(ftpcomm_fd);
	return(s);
   }

   if(s != 220) {
	fclose(fpcommin);
	fclose(fpcommout);
	close(ftpcomm_fd);
	return(0);
   }

   linkopen = 1;

   return(s);
}

int DOclose()
{
   if(!linkopen) {
	printf("You can't close a connection that isn't open.\n");
	return(0);
   }

   fclose(fpcommin);
   fclose(fpcommout);
   close(ftpcomm_fd);

   linkopen = 0;
   loggedin = 0;

   return(0);
}

int DOquit()
{
int s;

   if(linkopen) {
	s = DOcommand("QUIT", "");
	s = DOclose();
   }

   printf("FTP done.\n");

   exit(0);
}

void donothing(sig)
int sig;
{
}

int DOdata(datacom, file, direction, fd)
char *datacom;
char *file;
int direction;  /* RETR or STOR */
int fd;
{
nwio_tcpconf_t tcpconf;
nwio_tcpcl_t tcplopt, tcpcopt;
char *tcp_device;
int ftpdata_fd;
char *buff;
ipaddr_t ripaddr;
tcpport_t rport;
static tcpport_t lport = HTONS(0xF000);
int s;
int i;
int cs;
int pfd[2];
char dummy;
char port[32];

   ripaddr = hostip;
   rport = HTONS(20);

   /* here we set up a connection to listen on if not passive mode */
   /* otherwise we use this to connect for passive mode */

   if((tcp_device = getenv("TCP_DEVICE")) == NULL)
	tcp_device = "/dev/tcp";

   if((ftpdata_fd = open(tcp_device, O_RDWR)) < 0) {
	perror("ftp: open error on tcp device");
	return(-1);
   }

   if(passive) {
	s = DOcommand("PASV", "");
	if(s != 227) {
		close(ftpdata_fd);
		return(s);
	}
	/* decode host and port */
	buff = reply;
	while(*buff && (*buff != '(')) buff++;
	buff++;
	ripaddr = (ipaddr_t)0;
	for(i = 0; i < 4; i++) {
		ripaddr = (ripaddr << 8) + (ipaddr_t)atoi(buff);
		if((buff = strchr(buff, ',')) == (char *)0) {
			printf("Could not parse PASV reply\n");
			return(-1);
		}
		buff++;
	}
	rport = (tcpport_t)atoi(buff);
	if((buff = strchr(buff, ',')) == (char *)0) {
		printf("Could not parse PASV reply\n");
		return(-1);
	}
	buff++;
	rport = (rport << 8) + (tcpport_t)atoi(buff);
	ripaddr = ntohl(ripaddr);
	rport = ntohs(rport);
   }

   for (;;) {
	tcpconf.nwtc_flags = NWTC_SET_RA | NWTC_SET_RP;
	if (passive || ntohs(lport) >= 0xF000) {
		tcpconf.nwtc_flags |= NWTC_LP_SEL;
	} else {
		/* For no good reason Sun hosts don't like it if they have to
		 * connect to the same port twice in a short time...
		 */
		lport = htons(ntohs(lport) + 1);
		tcpconf.nwtc_flags |= NWTC_LP_SET;
		tcpconf.nwtc_locport = lport;
	}

	tcpconf.nwtc_remaddr = ripaddr;
	tcpconf.nwtc_remport = rport;

	s = ioctl(ftpdata_fd, NWIOSTCPCONF, &tcpconf);
	if(s < 0) {
		if (errno == EADDRINUSE) continue;
		perror("ftp: ioctl error on NWIOSTCPCONF");
		close(ftpdata_fd);
		return(s);
	}
	break;
   }

   s = ioctl(ftpdata_fd, NWIOGTCPCONF, &tcpconf);
   if(s < 0) {
	perror("ftp: ioctl error on NWIOGTCPCONF");
	close(ftpdata_fd);
	return(s);
   }
   lport = tcpconf.nwtc_locport;

   if(passive) {
	tcplopt.nwtcl_flags = 0;
	s = ioctl(ftpdata_fd, NWIOTCPCONN, &tcpcopt);
	if(s < 0) {
		perror("ftp: error on ioctl NWIOTCPCONN");
		close(ftpdata_fd);
		return(0);
	}
	s = ioctl(ftpdata_fd, NWIOGTCPCONF, &tcpconf);
	if(s < 0) {
		perror("ftp: error on ioctl NWIOGTCPCONF");
		close(ftpdata_fd);
		return(0);
	}
   } else {
	tcplopt.nwtcl_flags = 0;

	if (pipe(pfd) < 0) {
		perror("ftp: could not create a pipe");
		return(s);
	}
	lpid = fork();
	if(lpid < 0) {
		perror("ftp: could not fork listener");
		close(ftpdata_fd);
		close(pfd[0]);
		close(pfd[1]);
		return(s);
	} else if(lpid == 0) {
		close(pfd[0]);
		signal(SIGALRM, donothing);
		alarm(15);
		close(pfd[1]);
		s = ioctl(ftpdata_fd, NWIOTCPLISTEN, &tcplopt);
		alarm(0);
		if(s < 0)
			if(errno == EINTR)
				exit(1);	/* timed out */
			else
				exit(-1);	/* error */
		else
			exit(0);		/* connection made */
	}
	/* Wait for the pipe to close, then the listener is ready (almost). */
	close(pfd[1]);
	(void) read(pfd[0], &dummy, 1);
	close(pfd[0]);
	while(1) {
		signal(SIGALRM, donothing);
		alarm(1);
		s = ioctl(ftpdata_fd, NWIOGTCPCONF, &tcpconf);
		alarm(0);
		if(s == -1) break;
	}
   }

#define hiword(x)       ((u16_t)((x) >> 16))
#define loword(x)       ((u16_t)(x & 0xffff)) 
#define hibyte(x)       (((x) >> 8) & 0xff)
#define lobyte(x)       ((x) & 0xff)

   if(!passive) {
	sprintf(port, "%u,%u,%u,%u,%u,%u",
		hibyte(hiword(ntohl(myip))), lobyte(hiword(ntohl(myip))),
		hibyte(loword(ntohl(myip))), lobyte(loword(ntohl(myip))),
		hibyte(ntohs(lport)), lobyte(ntohs(lport)));
	s = DOcommand("PORT", port);
	if(s != 200) {
		close(ftpdata_fd);
		kill(lpid, SIGKILL);
		return(s);
	}
   }

   s = DOcommand(datacom, file);
   if(s == 125 || s == 150) {
	if(!passive) {
		while(1) {
			s = wait(&cs);
			if(s < 0 || s == lpid)
				break;
		}
		if(s < 0) {
			perror("wait error:");
			close(ftpdata_fd);
			kill(lpid, SIGKILL);
			return(s);
		}
		if((cs & 0x00ff)) {
			printf("Child listener failed %04x\n", cs);
			close(ftpdata_fd);
			return(-1);
		}
		cs = (cs >> 8) & 0x00ff;
		if(cs == 1) {
			printf("Child listener timed out\n");
			return(DOgetreply());
		} else if(cs) {
			printf("Child listener returned %02x\n", cs);
			close(ftpdata_fd);
			return(-1);
		}
	}
	switch(direction) {
		case RETR:
			s = recvfile(fd, ftpdata_fd);
			break;
		case STOR:
			s = sendfile(fd, ftpdata_fd);
			break;
	}
	close(ftpdata_fd);
	s = DOgetreply();
   } else {
	if(!passive)
		kill(lpid, SIGKILL);
	close(ftpdata_fd);
   }

   return(s);
}
